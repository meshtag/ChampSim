#ifndef PREFETCHER_GHB_CONFIDENCE_H
#define PREFETCHER_GHB_CONFIDENCE_H

#include <cstdint>
#include <unordered_map>

#include "address.h"
#include "champsim.h"
#include "modules.h"

struct DeltaHistory
{
  champsim::block_number last_line{};
  champsim::block_number prev_line{};
  int repeat_count = 0;
};

class ghb_confidence : public champsim::modules::prefetcher
{
public:
  using prefetcher::prefetcher;

  static constexpr int MAX_DEGREE = 4;
  static constexpr int CONFIDENCE_THRESHOLD = 2;

  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cacheHit,
                                    bool usefulPrefetch, access_type type, uint32_t metadataIn);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch,
                                 champsim::address evictedAddr, uint32_t metadataIn);

private:
  std::unordered_map<uint64_t, DeltaHistory> history_;
};

#endif // PREFETCHER_GHB_CONFIDENCE_H
