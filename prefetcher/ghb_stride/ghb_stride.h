#include <cstdint>
#include <array>
#include <cassert>

#include "address.h"
#include "champsim.h"
#include "modules.h"

struct ITEntry
{
    champsim::address ip{};  // The IP we are tracking. 
    int32_t  ghb_head = -1;  // Index of most recent GHB entry for this PC (-1 = none).
    bool     valid = false;
};

class IndexTable
{
public:
    static constexpr uint32_t NUM_SETS = 256;
    static constexpr uint32_t NUM_WAYS = 4;

    static constexpr uint32_t INVALID_SET = UINT32_MAX;

    IndexTable()
    {
        reset();
    }

    void reset()
    {
        for (auto &e : entries)
            e.valid = false;
        for (auto &v : next_victim)
            v = 0;
    }

    // Compute set index from PC (ip). Requires NUM_SETS to be a power of two.
    inline uint32_t get_set(champsim::address ip) const
    {
        return (ip.to<uint64_t>()) & (NUM_SETS - 1); // shift off low bits (byte offset / alignment)
    }

    // Returns pointer to the entry matching this PC, or nullptr.
    ITEntry* lookup(champsim::address ip)
    {
        uint32_t set = get_set(ip);
        uint32_t base = set * NUM_WAYS;

        for (uint32_t way = 0; way < NUM_WAYS; ++way)
        {
            ITEntry &e = entries[base + way];
            if (e.valid && e.ip == ip)
                return &e;
        }
        return nullptr;
    }

    // Allocate / replace an entry for this IP and set its ghb_head pointer.
    // Returns pointer to the entry (never nullptr).
    ITEntry* allocate(champsim::address ip, int32_t ghb_head_idx)
    {
        uint32_t set = get_set(ip);
        uint32_t base = set * NUM_WAYS;

        // First try to find an invalid way (free slot).
        for (uint32_t way = 0; way < NUM_WAYS; ++way)
        {
            ITEntry &e = entries[base + way];
            if (!e.valid)
            {
                e.valid = true;
                e.ip  = ip;
                e.ghb_head = ghb_head_idx;
                return &e;
            }
        }

        // Otherwise, evict using FIFO (next_victim[set]).
        uint32_t victim_way = next_victim[set];
        ITEntry &victim = entries[base + victim_way];

        victim.valid   = true;
        victim.ip  = ip;
        victim.ghb_head = ghb_head_idx;

        // Update FIFO pointer for this set.
        next_victim[set] = (victim_way + 1) % NUM_WAYS;

        return &victim;
    }

    // Convenience: update or allocate entry for this PC.
    // This is what you typically call on every L2 access:
    //
    //  - If PC already exists in IT: just update ghb_head.
    //  - Else: allocate a new IT entry for this PC.
    ITEntry* touch(champsim::address ip, int32_t ghb_head_idx)
    {
        ITEntry* e = lookup(ip);
        if (e)
        {
            e->ghb_head = ghb_head_idx;
            return e;
        }
        return allocate(ip, ghb_head_idx);
    }

private:
    // Flattened [set][way] array of entries.
    std::array<ITEntry, NUM_SETS * NUM_WAYS> entries;

    // Per-set FIFO pointer: which way to evict next.
    std::array<uint8_t, NUM_SETS> next_victim;
};


// ===== Global History Buffer (GHB) entry =====
struct GHBEntry
{
    champsim::address line_addr{};  // cacheline address (A).
    champsim::address ip{};         // IP that caused this access.
    int32_t  prev = -1;             // index of previous GHB entry for this PC (-1 = none).
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

    void reset()
    {
        for (auto &e : entries)
            e.valid = false;
        next_insert = 0;
        num_valid   = 0;
    }

    // Allocate a new GHB entry for (ip, line_addr) and link to previous head.
    // prev_head_idx: previous head GHB index for this PC (from IT), or -1.
    // Returns index of the new entry.
    int32_t insert(champsim::address ip, champsim::address line_addr, int32_t prev_head_idx)
    {
        int32_t idx = next_insert;

        GHBEntry &e = entries[idx];
        e.ip = ip;
        e.line_addr = line_addr;
        e.prev = prev_head_idx;
        e.valid = true;

        next_insert = (next_insert + 1) % NUM_ENTRIES;
        if (num_valid < NUM_ENTRIES)
            ++num_valid;

        // NOTE: We overwrite old entries in a circular manner.
        //       This means very old history is naturally dropped.
        return idx;
    }

    const GHBEntry* get(int32_t idx) const
    {
        if (idx < 0 || idx >= NUM_ENTRIES)
            return nullptr;
        return &entries[idx];
    }

private:
    std::array<GHBEntry, NUM_ENTRIES> entries;
    int32_t next_insert = 0; // next index to overwrite.
    int32_t num_valid   = 0; // how many entries are valid/encountered.
};

// ===== IT + GHB integration for PC/CS scheme =====
class GHB_PC_CS
{
public:
    // Record one L2 access.
    // ip        : PC of the memory access
    // line_addr : cacheline address
    void on_access(champsim::address ip, champsim::address line_addr)
    {
        // Step 1: Find previous head in IT (if any).
        int32_t old_head = 0;
        if (ITEntry* e = it.lookup(ip))
        {
            if (e->valid)
                old_head = e->ghb_head;
        }

        // Step 2: Insert new entry in GHB with link to previous head.
        int32_t new_head = ghb.insert(ip, line_addr, old_head);

        // Step 3: Update IT to point to new head.
        it.touch(ip, new_head);
    }

    // Retrieve last up-to max_len line addresses for this PC.
    // out_addrs[0] will be the most recent, etc.
    int get_last_line_addrs(champsim::address ip, champsim::address *out_addrs, int max_len)
    {
        const ITEntry* e = it.lookup(ip);
        if (!e || !e->valid || e->ghb_head < 0)
            return 0;

        int32_t idx = e->ghb_head;
        int count = 0;

        while (idx >= 0 && count < max_len)
        {
            const GHBEntry* ge = ghb.get(idx);
            if (!ge || ge->ip != ip)
                break; // sanity check to avoid following stale or foreign entries

            out_addrs[count++] = ge->line_addr;
            idx = ge->prev;
        }

        return count;
    }

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

  GHB_PC_CS ghb_pc_cs;

  // Prefetch parameters (from the assignment).
  static constexpr int PREFETCH_DEGREE  = 6;  // n.
  static constexpr int PREFETCH_DISTANCE = 4; // l.
  static constexpr int HISTORY_LEN = 3;  // last 3 addresses to be checked.

  void prefetcher_initialize();
  // void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {}
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
//   void prefetcher_cycle_operate();
  // void prefetcher_final_stats() {}
};
