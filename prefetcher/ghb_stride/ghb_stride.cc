#include "ghb_stride.h"

#include "cache.h"

void ghb_stride::prefetcher_initialize() { ghbPcCs.reset(); }

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
  for (int i = 0; i < PREFETCH_DEGREE; ++i) {
    int64_t k = PREFETCH_DISTANCE + i;

    champsim::address pfAddress{champsim::block_number{lineAddr + k * d}};

    if (intern_->virtual_prefetch || champsim::page_number{pfAddress} == champsim::page_number{addr}) {
      const bool mshrUnderLightLoad = intern_->get_mshr_occupancy_ratio() < 0.5;
      prefetch_line(pfAddress, mshrUnderLightLoad, 0);
    }
  }

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
