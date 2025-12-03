//=======================================================================================//
// File             : pythia/pythia.cc
// Author           : Rahul Bera, SAFARI Research Group (write2bera@gmail.com)
// Date             : 20/AUG/2025
// Description      : Implements Pythia prefectcher (Bera+, MICRO'21)
//=======================================================================================//

#include "pythia.h"

#include "cache.h"
#include "dpc_api.h"
#include "dram_metrics.h"
#include "pythia_params.h"

#include <iostream>

namespace
{
uint64_t g_last_row_conflicts_sample = 0;
uint64_t g_row_conflict_sample_counter = 0;
bool g_cached_high_row_conflict = false;
}

void pythia::prefetcher_initialize()
{
  init_knobs();

  last_evicted_tracker = NULL;
  brain_featurewise = new LearningEngineFeaturewise(PYTHIA::alpha, PYTHIA::gamma, PYTHIA::epsilon, (uint32_t)Actions.size(), PYTHIA::seed, PYTHIA::policy,
                                                    PYTHIA::learning_type);
}

uint32_t pythia::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                          uint32_t metadata_in)
{
  uint64_t address = addr.to<uint64_t>();
  uint64_t pc = ip.to<uint64_t>();

  uint64_t page = address >> LOG2_PAGE_SIZE;
  uint32_t offset = (uint32_t)((address >> LOG2_BLOCK_SIZE) & ((1ull << (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)) - 1));

  std::vector<uint64_t> pref_addr; // generated addresses to prefetch

  /* compute reward on demand */
  reward(address);

  /* global state tracking */
  update_global_state(pc, page, offset, address);
  /* per page state tracking */
  Scooby_STEntry* stentry = update_local_state(pc, page, offset, address);

  /* Measure state.
   * state can contain per page local information like delta signature, pc signature etc.
   * it can also contain global signatures like last three branch PCs etc.
   */
  State* state = new State();
  state->pc = pc;
  state->address = address;
  state->page = page;
  state->offset = offset;
  state->delta = !stentry->deltas.empty() ? stentry->deltas.back() : 0;
  state->local_delta_sig2 = stentry->get_delta_sig2();
  state->local_pc_sig = stentry->get_pc_sig();
  state->local_offset_sig = stentry->get_offset_sig();
  state->is_high_bw = is_high_bw(get_dram_bw());
  // periodically sample row-conflict deltas to derive a binary signal
  if (++g_row_conflict_sample_counter % PYTHIA::row_conflict_sample_period == 0) {
    bool prev_flag = g_cached_high_row_conflict;
    uint64_t curr = champsim::dram_metrics::row_conflicts();
    uint64_t delta = curr - g_last_row_conflicts_sample;
    g_cached_high_row_conflict = (delta >= PYTHIA::row_conflict_delta_thresh);
    g_last_row_conflicts_sample = curr;
    // if (g_cached_high_row_conflict != prev_flag) {
      // std::cout << "[pythia] row_conflict flag " << (g_cached_high_row_conflict ? "ON" : "OFF") << " delta=" << delta << " total=" << curr << std::endl;
    // }
  }
  state->row_conflicts = champsim::dram_metrics::row_conflicts();
  state->is_high_row_conflict = g_cached_high_row_conflict;

  // generate prefetch predictions
  predict(address, page, offset, state, pref_addr);

  /* issue prefetches */
  for (uint32_t addr_index = 0; addr_index < pref_addr.size(); ++addr_index) {
    champsim::address pf_addr{pref_addr[addr_index]};
    intern_->prefetch_line(pf_addr, true, 0);
  }

  return 0;
}

uint32_t pythia::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  register_fill(addr.to<uint64_t>());
  return 0;
}

void pythia::prefetcher_cycle_operate() {}

void pythia::prefetcher_final_stats() {}
