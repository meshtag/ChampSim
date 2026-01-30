#!/usr/bin/env python3
"""
Plot speedup of the row-conflict-aware Pythia run vs. baseline Pythia.
Speedup = IPC_row_conflict / IPC_baseline. Includes per-workload bars and a GEOMEAN bar.
Labels each bar with its value and notes the highest/lowest workloads.
"""

import csv
import math
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np


def load_ipc(path: Path) -> Dict[str, float]:
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
    root = Path(argv[1]) if len(argv) > 1 else Path(".")
    out_path = Path(argv[2]) if len(argv) > 2 else root / "results_pythia_row_conflict_speedup.png"

    baseline_csv = root / "results_gap_pythia" / "ipc.csv"
    rc_csv = root / "pythia_row_conflicts" / "results_gap_pythia" / "ipc.csv"

    if not baseline_csv.is_file() or not rc_csv.is_file():
        print(f"Missing IPC CSVs. Expected:\n  {baseline_csv}\n  {rc_csv}", file=sys.stderr)
        return 1

    baseline = load_ipc(baseline_csv)
    rc = load_ipc(rc_csv)

    traces = sorted(set(baseline) & set(rc))
    if not traces:
        print("No common traces found between baseline and row-conflict runs.", file=sys.stderr)
        return 1

    speedups: List[Tuple[str, float]] = []
    for tr in traces:
        base_ipc = baseline.get(tr, 0.0)
        rc_ipc = rc.get(tr, 0.0)
        s = (rc_ipc / base_ipc) if base_ipc > 0 else float("nan")
        speedups.append((tr, s))

    geo = geomean([s for _, s in speedups if not math.isnan(s)])
    speedups.append(("GEOMEAN", geo))

    valid = [(tr, s) for tr, s in speedups if tr != "GEOMEAN" and not math.isnan(s)]
    hi = max(valid, key=lambda x: x[1]) if valid else None
    lo = min(valid, key=lambda x: x[1]) if valid else None

    labels = [t for t, _ in speedups]
    vals = [v for _, v in speedups]
    x = np.arange(len(labels))
    width = 0.6

    plt.figure(figsize=(max(10, len(labels) * 0.6), 5))
    bars = plt.bar(x, vals, width, color=["#e6550d" if l == "GEOMEAN" else "#3182bd" for l in labels])
    plt.axhline(1.0, color="black", linestyle="--", linewidth=0.8)
    plt.ylabel("Speedup (IPC row-conflict / IPC baseline)")
    plt.xticks(x, labels, rotation=45, ha="right")
    plt.title("Pythia row-conflict vs baseline speedup")

    for bar, val in zip(bars, vals):
        if math.isnan(val):
            text = "N/A"
            display_val = 0
        else:
            text = f"{val:.2f}"
            display_val = val
        plt.text(bar.get_x() + bar.get_width() / 2, display_val + (0.02 if display_val >= 0 else -0.02), text, ha="center",
                 va="bottom" if display_val >= 0 else "top", fontsize=8)

    note_lines = []
    if hi:
        note_lines.append(f"Highest: {hi[0]} ({hi[1]:.2f}x)")
    if lo:
        note_lines.append(f"Lowest: {lo[0]} ({lo[1]:.2f}x)")
    if note_lines:
        plt.figtext(0.99, 0.01, " | ".join(note_lines), ha="right", va="bottom", fontsize=9)

    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=200)
    print(f"Saved speedup plot to {out_path}")
    if hi:
        print(f"Highest speedup: {hi[0]} = {hi[1]:.2f}x")
    if lo:
        print(f"Lowest speedup: {lo[0]} = {lo[1]:.2f}x")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
