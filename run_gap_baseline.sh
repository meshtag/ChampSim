#!/usr/bin/env bash
set -euo pipefail

# Runs GAP traces and plots IPC for the selected prefetcher(s).
# Usage: WARMUP=10000000 SIM=50000000 ./run_gap_baseline.sh [-r results_root] -p pythia -p nopref

TRACE_DIR="traces/GAP"

WARMUP=${WARMUP:-10000000}
SIM=${SIM:-50000000}

usage() {
  echo "Usage: $0 [-r|--results-root <dir>] [-p|--prefetcher <name>]..."
  echo "Supported prefetchers: nopref, pythia, ip_stride, next_line, ghb_stride"
  exit 1
}

prefetcher_config() {
  case "$1" in
    nopref) echo "dpc4/1C.fullBW.nopref.json" ;;
    pythia) echo "dpc4/1C.fullBW.baseline.json" ;;
    ip_stride) echo "dpc4/1C.fullBW.ip_stride_pref.json" ;;
    next_line) echo "dpc4/1C.fullBW.next_line_pref.json" ;;
    ghb_stride) echo "dpc4/1C.fullBW.ghb_stride_pref.json" ;;
    *) return 1 ;;
  esac
}

PREFETCHERS=()
RESULTS_ROOT="."
while [[ $# -gt 0 ]]; do
  case "$1" in
    -r|--results-root)
      [[ $# -ge 2 ]] || usage
      RESULTS_ROOT="$2"
      shift 2
      ;;
    -p|--prefetcher)
      [[ $# -ge 2 ]] || usage
      PREFETCHERS+=("$2")
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "Unknown option: $1"
      usage
      ;;
  esac
done

if [[ ${#PREFETCHERS[@]} -eq 0 ]]; then
  PREFETCHERS=(nopref)
fi

for PREFETCHER in "${PREFETCHERS[@]}"; do
  CONFIG="$(prefetcher_config "${PREFETCHER}")" || { echo "Unsupported prefetcher: ${PREFETCHER}"; usage; }
  OUT_DIR="${RESULTS_ROOT}/results_gap_${PREFETCHER}"
  LOG_DIR="${OUT_DIR}/logs"
  CSV_FILE="${OUT_DIR}/ipc.csv"

  mkdir -p "${LOG_DIR}"

  echo "==> Building ${PREFETCHER} (${CONFIG})"
  ./config.sh "${CONFIG}"
  make -j"$(sysctl -n hw.logicalcpu)" >/dev/null

  BIN="$(jq -r '.executable_name' "${CONFIG}")"

  echo "trace,ipc" > "${CSV_FILE}"

  find "${TRACE_DIR}" -maxdepth 1 -type f \( -name "*.trace" -o -name "*.trace.gz" -o -name "*.champsim" -o -name "*.champsim.gz" -o -name "*.champsimtrace.xz" \) | sort | while IFS= read -r TRACE; do
    NAME="$(basename "${TRACE}")"
    LOG="${LOG_DIR}/${NAME}.log"
    echo "==> [${PREFETCHER}] Running ${NAME}"
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
  echo "==> Results for ${PREFETCHER}:"
  cat "${CSV_FILE}"
  echo "Plot: ${CSV_FILE%.csv}.png"
done
