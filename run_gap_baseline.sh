#!/usr/bin/env bash
set -euo pipefail

# Runs the baseline no-prefetch configuration across all GAP traces and plots IPC.
# Usage: WARMUP=10000000 SIM=50000000 ./run_gap_baseline.sh

CONFIG="dpc4/1C.fullBW.nopref.json"
TRACE_DIR="traces/GAP"
OUT_DIR="results_gap_baseline"
LOG_DIR="${OUT_DIR}/logs"
CSV_FILE="${OUT_DIR}/ipc.csv"

WARMUP=${WARMUP:-10000000}
SIM=${SIM:-50000000}

mkdir -p "${LOG_DIR}"

echo "==> Building baseline (${CONFIG})"
./config.sh "${CONFIG}"
make -j"$(sysctl -n hw.logicalcpu)" >/dev/null

BIN="$(jq -r '.executable_name' "${CONFIG}")"

echo "trace,ipc" > "${CSV_FILE}"

find "${TRACE_DIR}" -maxdepth 1 -type f \( -name "*.trace" -o -name "*.champsim" \) | sort | while IFS= read -r TRACE; do
  NAME="$(basename "${TRACE}")"
  LOG="${LOG_DIR}/${NAME}.log"
  echo "==> Running ${NAME}"
  "./bin/${BIN}" --warmup-instructions "${WARMUP}" --simulation-instructions "${SIM}" "${TRACE}" | tee "${LOG}"
  IPC=$(grep -Eo 'CPU 0 cumulative IPC: [0-9]+(\.[0-9]+)?' "${LOG}" | awk '{print $5}' | tail -n1)
  echo "${NAME},${IPC:-0}" >> "${CSV_FILE}"
done

cat > "${OUT_DIR}/plot_ipc.py" <<'PY'
#!/usr/bin/env python3
import csv
import sys
from pathlib import Path
import matplotlib.pyplot as plt

csv_path = Path(sys.argv[1])
workloads, ipcs = [], []
with csv_path.open() as f:
    reader = csv.DictReader(f)
    for row in reader:
        workloads.append(row["trace"])
        ipcs.append(float(row["ipc"]))

plt.figure(figsize=(max(6, len(workloads) * 0.6), 5))
bars = plt.bar(workloads, ipcs, color="#6baed6")
plt.ylabel("IPC")
plt.xticks(rotation=45, ha="right")
for bar, val in zip(bars, ipcs):
    plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f"{val:.2f}", ha="center", va="bottom", fontsize=8)

plt.tight_layout()
plt.savefig(csv_path.with_suffix(".png"), dpi=200)
PY

python3 "${OUT_DIR}/plot_ipc.py" "${CSV_FILE}"
echo "==> Results:"
cat "${CSV_FILE}"
echo "Plot: ${CSV_FILE%.csv}.png"
