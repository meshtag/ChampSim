// Utility for DRAM-side metrics that can be queried from other components.
#pragma once

#include <atomic>
#include <cstdint>

namespace champsim::dram_metrics
{

// Inline storage and helpers to keep this header self-contained.
inline std::atomic<uint64_t> row_conflict_count{0};

inline void record_row_conflict() { row_conflict_count.fetch_add(1, std::memory_order_relaxed); }

inline uint64_t row_conflicts() { return row_conflict_count.load(std::memory_order_relaxed); }

inline void reset_row_conflicts() { row_conflict_count.store(0, std::memory_order_relaxed); }

} // namespace champsim::dram_metrics
