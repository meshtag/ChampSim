#!/usr/bin/env bash
set -euo pipefail

BIN="./bin/1C.fullBW.ghb_stride_system_aware"
TRACE_DIR="traces"
LOG_DIR="results_gap_fullBW_system_aware/logs"
WARMUP=10000000
SIM=50000000

mkdir -p "$LOG_DIR"

for trace in "$TRACE_DIR"/*.gz*; do
  trace_name=$(basename "$trace")
  echo
  echo "== Running full-bandwidth system-aware GHB on $trace_name =="
  "$BIN" \
    --warmup-instructions=$WARMUP \
    --simulation-instructions=$SIM \
    "$trace" \
    | tee "$LOG_DIR/$trace_name.log"
done
