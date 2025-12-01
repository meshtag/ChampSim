#include "ghb_stride.h"

#include "cache.h"

void ghb_stride::prefetcher_initialize() {
    ghb_pc_cs.reset();
}

uint32_t ghb_stride::prefetcher_cache_operate(champsim::address addr, champsim::address ip,
                                              uint8_t cache_hit, bool useful_prefetch,
                                              access_type type, uint32_t metadata_in)
{
  ghb_pc_cs.on_access(ip, addr);

  champsim::address history[HISTORY_LEN];
  int numHistElems = ghb_pc_cs.get_last_line_addrs(ip, history, HISTORY_LEN);

  if (numHistElems < HISTORY_LEN)
    return 0; // not enough history yet.

  // history[0] = most recent, history[1] = previous, history[2] = 2nd previous

  int64_t stride1 = history[0].to<int64_t>() - history[1].to<int64_t>();
  int64_t stride2 = history[1].to<int64_t>() - history[2].to<int64_t>();

  // Check for matching strides.
  if (stride1 == 0 || stride2 == 0)
    return 0; // ignore zero stride.
  if (stride1 != stride2)
    return 0; // not a clear stride pattern.

  int64_t d = stride1;

  // Issue prefetches.
  for (int i = 0; i < PREFETCH_DEGREE; ++i) {
    int64_t k = PREFETCH_DISTANCE + i;
    // int64_t pfLine = static_cast<int64_t>(addr) + k * d;
    // if (pf_line < 0)
    //   continue; // avoid underflow

    champsim::address pfAddress{addr + k * d};

    if (intern_->virtual_prefetch || champsim::page_number{pfAddress} == champsim::page_number{addr}) {
        const bool mshr_under_light_load = intern_->get_mshr_occupancy_ratio() < 0.5;
        const bool success = prefetch_line(pfAddress, mshr_under_light_load, 0);
    }
  }

  return metadata_in;
}

uint32_t ghb_stride::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
