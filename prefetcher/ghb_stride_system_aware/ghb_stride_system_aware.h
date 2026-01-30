#ifndef PREFETCHER_GHB_STRIDE_SYSTEM_AWARE_H
#define PREFETCHER_GHB_STRIDE_SYSTEM_AWARE_H

#include "../ghb_stride/ghb_stride.h"

#include <cstdint>

#include "address.h"
#include "champsim.h"
#include "modules.h"

class ghb_stride_system_aware : public champsim::modules::prefetcher
{
public:
  using prefetcher::prefetcher;

  // Prefetch parameters.
  static constexpr int PREFETCH_DEGREE = 6;
  static constexpr int PREFETCH_DISTANCE = 4;
  static constexpr int HISTORY_LEN = 3;
  static constexpr int MIN_PREFETCH_DEGREE = 1;
  static constexpr int EPOCH_CYCLES = 1000;

  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cacheHit,
                                    bool usefulPrefetch, access_type type, uint32_t metadataIn);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch,
                                 champsim::address evictedAddr, uint32_t metadataIn);
  void prefetcher_cycle_operate();

private:
  GHB_PC_CS ghbPcCs;
  int currentPrefetchDegree = PREFETCH_DEGREE;
  int epochCycleCounter = 0;
  uint64_t lastPfIssued = 0;
  uint64_t lastPfUseful = 0;

  void adjust_prefetch_degree(double accuracyPct, uint32_t bwPct);
};

#endif // PREFETCHER_GHB_STRIDE_SYSTEM_AWARE_H
