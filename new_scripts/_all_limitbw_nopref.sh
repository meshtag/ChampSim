#!/usr/bin/env bash
set -euo pipefail

BIN="./bin/1C.limitBW.nopref"
TRACE_DIR="traces"
LOG_DIR="results_gap_limitBW_nopref/logs"
WARMUP=10000000
SIM=50000000

mkdir -p "$LOG_DIR"

for trace in "$TRACE_DIR"/*.champsim*; do
  trace_name=$(basename "$trace")
  echo
  echo "== Running limited BW baseline on $trace_name =="
  "$BIN" \
    --warmup-instructions=$WARMUP \
    --simulation-instructions=$SIM \
    "$trace" \
    | tee "$LOG_DIR/$trace_name.log"
done
