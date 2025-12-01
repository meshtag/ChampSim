#include <cstdint>
#include <array>
#include <cassert>

#include "address.h"
#include "champsim.h"
#include "modules.h"

struct ITEntry
{
  champsim::address ip{}; // The IP we are tracking.
  int32_t ghbHead = -1;   // Index of most recent GHB entry for this PC (-1 = none).
  bool valid = false;
};

class IndexTable
{
public:
  // Tracks most recent GHB entry per PC using a small set-associative table.
  static constexpr uint32_t NUM_SETS = 256;
  static constexpr uint32_t NUM_WAYS = 4;

  static constexpr uint32_t INVALID_SET = UINT32_MAX;

  // Construct an empty index table with all entries invalid.
  IndexTable() { reset(); }

  // Clear all entries and reset replacement state.
  void reset()
  {
    for (auto& entry : entries)
      entry.valid = false;
    for (auto& victim : nextVictim)
      victim = 0;
    }

    // Compute set index from PC (ip). Requires NUM_SETS to be a power of two.
    // ip: instruction pointer of the access.
    inline uint32_t get_set(champsim::address ip) const
    {
        return (ip.to<uint64_t>()) & (NUM_SETS - 1); // shift off low bits (byte offset / alignment)
    }

    // Returns pointer to the entry matching this PC, or nullptr.
    // ip: instruction pointer to search for.
    ITEntry* lookup(champsim::address ip)
    {
      uint32_t setIdx = get_set(ip);
      uint32_t baseIdx = setIdx * NUM_WAYS;

      for (uint32_t way = 0; way < NUM_WAYS; ++way) {
        ITEntry& entry = entries[baseIdx + way];
        if (entry.valid && entry.ip == ip)
          return &entry;
        }
        return nullptr;
    }

    // Allocate / replace an entry for this IP and set its ghb_head pointer.
    // Returns pointer to the entry (never nullptr).
    // ip: instruction pointer being inserted.
    // ghbHeadIdx: index of most recent GHB entry for this IP.
    ITEntry* allocate(champsim::address ip, int32_t ghbHeadIdx)
    {
      uint32_t setIdx = get_set(ip);
      uint32_t baseIdx = setIdx * NUM_WAYS;

      // First try to find an invalid way (free slot).
      for (uint32_t way = 0; way < NUM_WAYS; ++way) {
        ITEntry& entry = entries[baseIdx + way];
        if (!entry.valid) {
          entry.valid = true;
          entry.ip = ip;
          entry.ghbHead = ghbHeadIdx;
          return &entry;
        }
        }

        // Otherwise, evict using FIFO (nextVictim[set]).
        uint32_t victimLine = nextVictim[setIdx];
        ITEntry& victim = entries[baseIdx + victimLine];

        victim.valid = true;
        victim.ip = ip;
        victim.ghbHead = ghbHeadIdx;

        // Update FIFO pointer for this set in circular fashion.
        nextVictim[setIdx] = (victimLine + 1) % NUM_WAYS;

        return &victim;
    }

    // Convenience: update or allocate entry for this PC.
    // This is what you typically call on every L2 access:
    //
    //  - If PC already exists in IT: just update ghb_head.
    //  - Else: allocate a new IT entry for this PC.
    // ip: instruction pointer being touched.
    // ghbHeadIdx: index of most recent GHB entry for this IP.
    ITEntry* touch(champsim::address ip, int32_t ghbHeadIdx)
    {
      ITEntry* entry = lookup(ip);
      if (entry) {
        entry->ghbHead = ghbHeadIdx;
        return entry;
      }
      return allocate(ip, ghbHeadIdx);
    }

private:
    // Flattened [set][way] array of entries.
    std::array<ITEntry, NUM_SETS * NUM_WAYS> entries;

    // Per-set FIFO pointer: which line to evict next.
    std::array<uint8_t, NUM_SETS> nextVictim;
};


// ===== Global History Buffer (GHB) entry =====
struct GHBEntry
{
  champsim::block_number lineAddr{}; // cacheline address (A).
  champsim::address ip{};            // IP that caused this access.
  int32_t prev = -1;                 // index of previous GHB entry for this PC (-1 = none).
  bool valid = false;
};

// ===== Global History Buffer =====
class GlobalHistoryBuffer
{
public:
    static constexpr int32_t NUM_ENTRIES = 256;

    GlobalHistoryBuffer()
    {
        reset();
    }

    // Clear all stored history entries.
    void reset()
    {
      for (auto& entry : entries)
        entry.valid = false;
      nextInsert = 0;
      numValid = 0;
    }

    // Allocate a new GHB entry for (ip, lineAddr) and link to previous head.
    // prevHeadIdx: previous head GHB index for this PC (from IT), or -1.
    // Returns index of the new entry.
    // ip: instruction pointer that generated the access.
    // lineAddr: cache-line address accessed.
    // prevHeadIdx: prior GHB head index for this IP, or -1 if none.
    int32_t insert(champsim::address ip, champsim::block_number lineAddr, int32_t prevHeadIdx)
    {
      int32_t idx = nextInsert;

      GHBEntry& entry = entries[static_cast<std::size_t>(idx)];
      entry.ip = ip;
      entry.lineAddr = lineAddr;
      entry.prev = prevHeadIdx;
      entry.valid = true;

      nextInsert = (nextInsert + 1) % NUM_ENTRIES;
      if (numValid < NUM_ENTRIES)
        ++numValid;

      // NOTE: We overwrite old entries in a circular manner.
      //       This means very old history is naturally dropped.
      return idx;
    }

    // Retrieve pointer to GHB entry at index; nullptr if out of range.
    // idx: index into the circular history buffer.
    const GHBEntry* get(int32_t idx) const
    {
        if (idx < 0 || idx >= NUM_ENTRIES)
            return nullptr;
        return &entries[static_cast<std::size_t>(idx)];
    }

private:
    std::array<GHBEntry, NUM_ENTRIES> entries;
    int32_t nextInsert = 0; // next index to overwrite.
    int32_t numValid = 0;   // how many entries are valid/encountered.
};

// ===== IT + GHB integration for PC/CS scheme =====
class GHB_PC_CS
{
public:
  // Record one L2 access.
  // ip        : PC of the memory access
  // lineAddr : cacheline address
  // Updates GHB and IT to link this access to prior accesses from the same PC.
  void on_access(champsim::address ip, champsim::block_number lineAddr)
  {
    // Step 1: Find previous head in IT (if any).
    int32_t oldHead = -1;
    if (ITEntry* entry = it.lookup(ip)) {
      if (entry->valid)
        oldHead = entry->ghbHead;
    }

    // Step 2: Insert new entry in GHB with link to previous head.
    int32_t newHead = ghb.insert(ip, lineAddr, oldHead);

    // Step 3: Update IT to point to new head.
    it.touch(ip, newHead);
    }

    // Retrieve last up-to max_len line addresses for this PC.
    // out_addrs[0] will be the most recent, etc.
    // ip: instruction pointer whose history we want.
    // outAddrs: caller-provided buffer to store recent line addresses.
    // maxLen: maximum number of history entries to return.
    int get_last_line_addrs(champsim::address ip, champsim::block_number* outAddrs, int maxLen)
    {
      const ITEntry* entry = it.lookup(ip);
      if (!entry || !entry->valid || entry->ghbHead < 0)
        return 0;

      int32_t idx = entry->ghbHead;
      int count = 0;

      while (idx >= 0 && count < maxLen) {
        const GHBEntry* ghbEntry = ghb.get(idx);
        if (!ghbEntry || !ghbEntry->valid || ghbEntry->ip != ip)
          break; // sanity check to avoid following stale or foreign entries

        outAddrs[count++] = ghbEntry->lineAddr;
        idx = ghbEntry->prev;
      }

      return count;
    }

    // Reset both IT and GHB state.
    void reset()
    {
        it.reset();
        ghb.reset();
    }

private:
    IndexTable          it;
    GlobalHistoryBuffer ghb;
};


class ghb_stride : public champsim::modules::prefetcher
{
public:
  using prefetcher::prefetcher;

  GHB_PC_CS ghbPcCs;

  // Prefetch parameters (from the lab pdf).
  static constexpr int PREFETCH_DEGREE  = 6;  // n (max).
  static constexpr int PREFETCH_DISTANCE = 4; // l.
  static constexpr int HISTORY_LEN = 3;  // last 3 addresses to be checked.
  static constexpr int MIN_PREFETCH_DEGREE = 1;
  static constexpr int EPOCH_CYCLES = 1000;

  void prefetcher_initialize();
  // void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {}
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cacheHit, bool usefulPrefetch, access_type type, uint32_t metadataIn);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evictedAddr, uint32_t metadataIn);
  void prefetcher_cycle_operate();
  // void prefetcher_final_stats() {}

private:
  int currentPrefetchDegree = PREFETCH_DEGREE;
  int epochCycleCounter = 0;
  uint64_t epochPfIssued = 0;
  uint64_t epochPfUseful = 0;

  void update_prefetch_degree();
};
