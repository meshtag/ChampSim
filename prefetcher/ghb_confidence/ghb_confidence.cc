#include "ghb_confidence.h"

#include <cstddef>

#include "cache.h"

void ghb_confidence::prefetcher_initialize()
{
  history_.clear();
}

uint32_t ghb_confidence::prefetcher_cache_operate(champsim::address addr, champsim::address ip,
                                                  uint8_t cacheHit, bool usefulPrefetch, access_type type,
                                                  uint32_t metadataIn)
{
  auto& entry = history_[ip.to<uint64_t>()];
  champsim::block_number current_line{addr};

  if (entry.last_line == champsim::block_number{})
  {
    entry.last_line = current_line;
    return metadataIn;
  }

  champsim::block_number::difference_type delta = champsim::offset(entry.last_line, current_line);

  if (entry.prev_line != champsim::block_number{})
  {
    champsim::block_number::difference_type prev_delta = champsim::offset(entry.prev_line, entry.last_line);
    if (delta == prev_delta)
    {
      entry.repeat_count = std::min(entry.repeat_count + 1, MAX_DEGREE);
    }
    else
    {
      entry.repeat_count = 1;
    }
  }
  else
  {
    entry.repeat_count = 1;
  }

  entry.prev_line = entry.last_line;
  entry.last_line = current_line;

  if (entry.repeat_count >= CONFIDENCE_THRESHOLD)
  {
    for (int i = 1; i <= MAX_DEGREE; ++i)
    {
      champsim::address pf_addr{champsim::block_number{current_line + delta * i}};
      const bool light_load = intern_->get_mshr_occupancy_ratio() < 0.5;
      prefetch_line(pf_addr, light_load, 0);
    }
  }

  return metadataIn;
}

uint32_t ghb_confidence::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch,
                                               champsim::address evictedAddr, uint32_t metadataIn)
{
  (void)addr;
  (void)set;
  (void)way;
  (void)prefetch;
  (void)evictedAddr;
  return metadataIn;
}
