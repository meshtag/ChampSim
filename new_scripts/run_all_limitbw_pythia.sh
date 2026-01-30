#!/bin/bash
set -euo pipefail

BIN="./bin/1C.limitBW.baseline"
TRACE_DIR="traces"
RESULTS_DIR="results_gap_limitBW_pythia"
WARMUP=10000000
SIM=50000000

mkdir -p "$RESULTS_DIR/logs"

for trace in "$TRACE_DIR"/*.gz*; do
  printf "\n== Running %s ==\n" "$trace"
  "$BIN" --warmup-instructions=$WARMUP --simulation-instructions=$SIM "$trace" \
    | tee "$RESULTS_DIR/logs/$(basename "$trace").log"
done
