#!/bin/bash
set -euo pipefail

BIN="./bin/1C.fullBW.nopref"
TRACE_DIR="traces"
WARMUP=10000000
SIM=50000000

for trace in "$TRACE_DIR"/*.gz*; do
  printf "\n== Running %s ==\n" "$trace"
  "$BIN" --warmup-instructions=$WARMUP --simulation-instructions=$SIM "$trace" \
    | tee "results_gap_nopref/logs/$(basename "$trace").log"
done
