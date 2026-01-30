#!/usr/bin/env bash
set -euo pipefail

BIN="./bin/1C.limitBW.ghb_confidence"
TRACE_DIR="traces"
LOG_DIR="results_gap_limitBW_ghb_confidence/logs"
WARMUP=10000000
SIM=50000000

mkdir -p "$LOG_DIR"

for trace in "$TRACE_DIR"/*.champsim*; do
  name=$(basename "$trace")
  echo
  echo "== Running limited-bandwidth ghb_confidence on $name =="
  "$BIN" \
    --warmup-instructions=$WARMUP \
    --simulation-instructions=$SIM \
    "$trace" \
    | tee "$LOG_DIR/$name.log"
done
