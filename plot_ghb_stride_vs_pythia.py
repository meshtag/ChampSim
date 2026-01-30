#!/usr/bin/env python3
"""
Build a per-trace IPC comparison plot for ghb_stride vs. Pythia using the
simulator logs (parses the final "CPU 0 cumulative IPC" line in each log).
Defaults assume GAP experiment layout; override paths via CLI arguments.

Usage:
    python plot_ghb_stride_vs_pythia.py [ghb_log_dir] [pythia_log_dir] [out_png]
"""

import csv
import math
import os
import re
import sys
from pathlib import Path
from typing import Dict, Optional

# Ensure the local pip --target install is on sys.path so matplotlib/numpy resolve.
LOCAL_SITE = Path(__file__).with_name(".venv")
if LOCAL_SITE.exists():
    sys.path.insert(0, str(LOCAL_SITE))

# Force a headless backend and cache directory inside the workspace to avoid sandbox issues.
MPL_CACHE = Path(__file__).with_name(".matplotlib-cache")
MPL_CACHE.mkdir(exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", str(MPL_CACHE))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


IPC_RE = re.compile(r"cumulative IPC:\s*([0-9]*\.?[0-9]+)")


def extract_final_ipc(log_path: Path) -> Optional[float]:
    """Grab the last cumulative IPC value from a log file."""
    try:
        lines = log_path.read_text().splitlines()
    except OSError:
        return None

    for line in reversed(lines):
        match = IPC_RE.search(line)
        if match:
            try:
                return float(match.group(1))
            except ValueError:
                return None
    return None


def collect_ipc(log_dir: Path) -> Dict[str, float]:
    """
    Collect per-trace IPCs from logs, preferring *.gz.log files to avoid
    double-counting uncompressed duplicates.
    """
    ipc: Dict[str, float] = {}
    patterns = ("*.trace.gz.log", "*.champsim.gz.log")
    for pattern in patterns:
        for log in sorted(log_dir.glob(pattern)):
            val = extract_final_ipc(log)
            if val is not None:
                ipc[log.stem] = val
    return ipc


def annotate_bars(ax, bars):
    for bar in bars:
        height = bar.get_height()
        if math.isnan(height):
            continue
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            height,
            f"{height:.3f}",
            ha="center",
            va="bottom",
            fontsize=7,
        )


def main(argv: list[str]) -> int:
    ghb_dir = Path(argv[1]) if len(argv) > 1 else Path("results_gap_ghb_stride/logs")
    pythia_dir = Path(argv[2]) if len(argv) > 2 else Path("results_gap_pythia/logs")
    out_path = Path(argv[3]) if len(argv) > 3 else Path("results_gap_ghb_vs_pythia.png")

    ghb_ipc = collect_ipc(ghb_dir)
    pythia_ipc = collect_ipc(pythia_dir)
    traces = sorted(set(ghb_ipc) | set(pythia_ipc))

    if not traces:
        print("No trace logs found to plot.", file=sys.stderr)
        return 1

    x = np.arange(len(traces))
    width = 0.38
    fig, ax = plt.subplots(figsize=(max(10, len(traces) * 0.7), 5))

    pythia_vals = [pythia_ipc.get(t, math.nan) for t in traces]
    ghb_vals = [ghb_ipc.get(t, math.nan) for t in traces]

    bars_pythia = ax.bar(
        x - width / 2,
        pythia_vals,
        width,
        label="pythia",
        color="#3182bd",
    )
    bars_ghb = ax.bar(
        x + width / 2,
        ghb_vals,
        width,
        label="ghb_stride",
        color="#e6550d",
    )

    ax.set_ylabel("IPC")
    ax.set_title("Prefetcher IPC by trace (logs)")
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha="right")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.4)
    annotate_bars(ax, bars_pythia)
    annotate_bars(ax, bars_ghb)

    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=200)

    csv_path = out_path.with_suffix(".csv")
    with csv_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["trace", "pythia_ipc", "ghb_stride_ipc"])
        for trace, p_val, g_val in zip(traces, pythia_vals, ghb_vals):
            writer.writerow([trace, f"{p_val:.4f}", f"{g_val:.4f}"])

    print(f"Saved plot to {out_path}")
    print(f"Wrote merged IPC CSV to {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
