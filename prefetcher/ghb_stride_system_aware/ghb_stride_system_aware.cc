#include "ghb_stride_system_aware.h"

#include <algorithm>

#include "cache.h"
#include "dpc_api.h"

void ghb_stride_system_aware::prefetcher_initialize()
{
  ghbPcCs.reset();
  currentPrefetchDegree = PREFETCH_DEGREE;
  epochCycleCounter = 0;
  lastPfIssued = intern_->sim_stats.pf_issued;
  lastPfUseful = intern_->sim_stats.pf_useful;
}

uint32_t ghb_stride_system_aware::prefetcher_cache_operate(champsim::address addr, champsim::address ip,
                                                           uint8_t cacheHit, bool usefulPrefetch, access_type type,
                                                           uint32_t metadataIn)
{
  champsim::block_number lineAddr{addr};
  ghbPcCs.on_access(ip, lineAddr);

  champsim::block_number history[HISTORY_LEN];
  int numHistElems = ghbPcCs.get_last_line_addrs(ip, history, HISTORY_LEN);

  if (numHistElems < HISTORY_LEN)
    return metadataIn;

  auto stride1 = champsim::offset(history[1], history[0]);
  auto stride2 = champsim::offset(history[2], history[1]);

  if (stride1 == 0 || stride2 == 0 || stride1 != stride2)
    return metadataIn;

  auto d = stride1;
  for (int i = 0; i < currentPrefetchDegree; ++i) {
    int64_t k = PREFETCH_DISTANCE + i;
    champsim::address pfAddress{champsim::block_number{lineAddr + k * d}};

    if (intern_->virtual_prefetch || champsim::page_number{pfAddress} == champsim::page_number{addr}) {
      const bool lightLoad = intern_->get_mshr_occupancy_ratio() < 0.5;
      prefetch_line(pfAddress, lightLoad, 0);
    }
  }

  return metadataIn;
}

uint32_t ghb_stride_system_aware::prefetcher_cache_fill(champsim::address addr, long set, long way,
                                                        uint8_t prefetch, champsim::address evictedAddr,
                                                        uint32_t metadataIn)
{
  return metadataIn;
}

void ghb_stride_system_aware::prefetcher_cycle_operate()
{
  if (++epochCycleCounter >= EPOCH_CYCLES) {
    uint64_t issued = intern_->sim_stats.pf_issued - lastPfIssued;
    uint64_t useful = intern_->sim_stats.pf_useful - lastPfUseful;

    lastPfIssued = intern_->sim_stats.pf_issued;
    lastPfUseful = intern_->sim_stats.pf_useful;

    double accuracyPct = issued ? (100.0 * useful / issued) : 0.0;
    uint8_t bwPctRaw = get_dram_bw();
    uint32_t bwPct = (bwPctRaw * 100) / 16;

    adjust_prefetch_degree(accuracyPct, bwPct);
    epochCycleCounter = 0;
  }
}

void ghb_stride_system_aware::adjust_prefetch_degree(double accuracyPct, uint32_t bwPct)
{
  int delta = 0;
  if (bwPct >= 90) {
    if (accuracyPct >= 90.0)
      delta = 0;
    else if (accuracyPct >= 75.0)
      delta = 1;
    else if (accuracyPct >= 50.0)
      delta = -1;
    else
      delta = -2;
  } else if (bwPct >= 25) {
    if (accuracyPct >= 90.0)
      delta = 2;
    else if (accuracyPct >= 50.0)
      delta = 0;
    else
      delta = -1;
  } else {
    if (accuracyPct >= 50.0)
      delta = 1;
    else
      delta = 0;
  }

  currentPrefetchDegree = std::clamp(currentPrefetchDegree + delta, MIN_PREFETCH_DEGREE, PREFETCH_DEGREE);
}
