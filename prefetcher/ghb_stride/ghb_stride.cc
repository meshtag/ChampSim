#include "ghb_stride.h"

#include <algorithm>

#include "cache.h"
#include "dpc_api.h"

void ghb_stride::prefetcher_initialize()
{
  ghbPcCs.reset();
  currentPrefetchDegree = PREFETCH_DEGREE;
  epochCycleCounter = 0;
  epochPfIssued = 0;
  epochPfUseful = 0;
}

// Main prefetcher entry point for cache accesses.
// addr: byte address accessed.
// ip: instruction pointer of the access.
// cache_hit: cache hit indicator (unused).
// useful_prefetch: whether a prior prefetch was useful (unused).
// type: access type (used to skip training on prefetches).
// metadata_in: incoming cache metadata to pass through.
uint32_t ghb_stride::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cacheHit, bool usefulPrefetch, access_type type,
                                              uint32_t metadataIn)
{
  champsim::block_number lineAddr{addr};
  ghbPcCs.on_access(ip, lineAddr);

  champsim::block_number history[HISTORY_LEN];
  int numHistElems = ghbPcCs.get_last_line_addrs(ip, history, HISTORY_LEN);

  if (numHistElems < HISTORY_LEN)
    return metadataIn; // not enough history yet to assert a stride.

  // history[0] = most recent, history[1] = previous, history[2] = 2nd previous
  champsim::block_number::difference_type stride1 = champsim::offset(history[1], history[0]);
  champsim::block_number::difference_type stride2 = champsim::offset(history[2], history[1]);

  // Check for matching strides.
  if (stride1 == 0 || stride2 == 0)
    return metadataIn; // ignore zero stride.
  if (stride1 != stride2)
    return metadataIn; // not a clear stride pattern.

  auto d = stride1;

  // Issue prefetches along the detected stride, offset by PREFETCH_DISTANCE.
  for (int i = 0; i < currentPrefetchDegree; ++i) {
    int64_t k = PREFETCH_DISTANCE + i;

    champsim::address pfAddress{champsim::block_number{lineAddr + k * d}};

    if (intern_->virtual_prefetch || champsim::page_number{pfAddress} == champsim::page_number{addr}) {
      const bool mshrUnderLightLoad = intern_->get_mshr_occupancy_ratio() < 0.5;
      if (prefetch_line(pfAddress, mshrUnderLightLoad, 0))
        ++epochPfIssued;
    }
  }

  if (usefulPrefetch)
    ++epochPfUseful;

  return metadataIn;
}

// Cache fill hook; currently a passthrough.
// addr: filled line address.
// set/way: cache location being filled (unused).
// prefetch: nonzero if this fill is a prefetch (unused).
// evicted_addr: line being evicted (unused).
// metadata_in: incoming metadata to return unchanged.
uint32_t ghb_stride::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evictedAddr, uint32_t metadataIn)
{
  return metadataIn;
}

void ghb_stride::prefetcher_cycle_operate()
{
  if (++epochCycleCounter >= EPOCH_CYCLES) {
    update_prefetch_degree();
    epochCycleCounter = 0;
    epochPfIssued = 0;
    epochPfUseful = 0;
  }
}

void ghb_stride::update_prefetch_degree()
{
  const double accuracyPct = epochPfIssued ? (100.0 * epochPfUseful / epochPfIssued) : 0.0;
  const uint8_t bwPctRaw = get_dram_bw();
  const uint32_t bwPct = (bwPctRaw * 100) / 16;

  int delta = 0;
  if (bwPct >= 90) {
    if (accuracyPct >= 90.0)
      delta = 0;
    else if (accuracyPct >= 50.0)
      delta = -1;
    else
      delta = -2;
  } else if (bwPct >= 25) {
    if (accuracyPct >= 90.0)
      delta = 1;
    else if (accuracyPct >= 50.0)
      delta = 0;
    else
      delta = -1;
  } else { // bwPct < 25
    if (accuracyPct >= 90.0)
      delta = 2;
    else if (accuracyPct >= 50.0)
      delta = 1;
    else
      delta = 0;
  }

  currentPrefetchDegree = std::clamp(currentPrefetchDegree + delta, MIN_PREFETCH_DEGREE, PREFETCH_DEGREE);
}
