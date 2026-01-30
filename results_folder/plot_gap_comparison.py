#!/usr/bin/env python3
"""
Compare baseline, Pythia, and ghb_stride GAP runs in one figure.
Reads existing IPC CSVs and produces a single plot with per-trace bars and
geomean improvement vs. baseline.
"""

import csv
import math
import sys
from pathlib import Path
from typing import Dict, List

import matplotlib.pyplot as plt
import numpy as np


DATASETS = {
    "baseline": Path("results_gap_baseline/ipc.csv"),
    "pythia": Path("results_gap_pythia/ipc.csv"),
    "ghb_stride": Path("results_gap_ghb_stride/ipc.csv"),
}


def load_ipc(path: Path) -> Dict[str, float]:
    if not path.is_file():
        raise FileNotFoundError(f"Missing IPC CSV: {path}")
    data: Dict[str, float] = {}
    with path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                data[row["trace"]] = float(row["ipc"])
            except (KeyError, ValueError):
                continue
    return data


def geomean(values: List[float]) -> float:
    vals = [v for v in values if v > 0]
    if not vals:
        return float("nan")
    return math.exp(sum(math.log(v) for v in vals) / len(vals))


def main(argv: List[str]) -> int:
    out_path = Path(argv[1]) if len(argv) > 1 else Path("results_gap_comparison.png")

    ipc_data: Dict[str, Dict[str, float]] = {}
    for name, path in DATASETS.items():
        ipc_data[name] = load_ipc(path)

    all_traces = sorted({t for dataset in ipc_data.values() for t in dataset})
    if not all_traces:
        print("No traces found in the provided IPC CSVs.", file=sys.stderr)
        return 1

    # Prepare bar data for per-trace IPC/CIPC comparison.
    labels = list(DATASETS.keys())
    width = 0.22
    x = np.arange(len(all_traces))
    colors = {"baseline": "#7f7f7f", "pythia": "#3182bd", "ghb_stride": "#e6550d"}

    fig, (ax_ipc, ax_geo) = plt.subplots(1, 2, figsize=(max(10, len(all_traces) * 0.7), 5))

    for idx, name in enumerate(labels):
        heights = [ipc_data[name].get(tr, np.nan) for tr in all_traces]
        ax_ipc.bar(x + idx * width - width, heights, width, label=name, color=colors.get(name))
    ax_ipc.set_ylabel("IPC (CIPC)")
    ax_ipc.set_title("Per-trace IPC")
    ax_ipc.set_xticks(x)
    ax_ipc.set_xticklabels(all_traces, rotation=45, ha="right")
    ax_ipc.legend()
    ax_ipc.grid(axis="y", linestyle="--", alpha=0.4)

    # Geomean improvement vs baseline.
    baseline = ipc_data["baseline"]
    improvements = {"baseline": 0.0}
    for name in labels:
        if name == "baseline":
            continue
        ratios = []
        for tr, base_ipc in baseline.items():
            other_ipc = ipc_data[name].get(tr)
            if other_ipc and base_ipc:
                ratios.append(other_ipc / base_ipc)
        g = geomean(ratios)
        improvements[name] = (g - 1.0) * 100.0 if not math.isnan(g) else float("nan")

    imp_labels = list(improvements.keys())
    imp_values = [improvements[k] for k in imp_labels]
    ax_geo.bar(imp_labels, imp_values, color=[colors.get(k, "#999999") for k in imp_labels])
    ax_geo.axhline(0, color="black", linewidth=0.8)
    ax_geo.set_ylabel("Geomean IPC Improvement vs baseline (%)")
    ax_geo.set_title("Geomean Improvement")
    for i, v in enumerate(imp_values):
        if math.isnan(v):
            txt = "N/A"
        else:
            txt = f"{v:+.1f}%"
        ax_geo.text(i, v + (0.5 if v >= 0 else -0.5), txt, ha="center", va="bottom" if v >= 0 else "top", fontsize=8)
    ax_geo.grid(axis="y", linestyle="--", alpha=0.4)

    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=200)
    print(f"Saved comparison plot to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
