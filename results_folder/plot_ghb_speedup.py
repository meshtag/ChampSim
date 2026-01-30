#!/usr/bin/env python3
"""Plot ghb_stride vs baseline speedups using existing IPC logs."""

from __future__ import annotations

from pathlib import Path
import csv
import math
from typing import Dict, Tuple, List

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BASELINE_CSV = Path("results_gap_nopref/ipc_combined.csv")
GHB_CSV = Path("results_gap_ghb_stride/ipc.csv")
OUTPUT_CSV = Path("results_gap_ghb_stride/speedup.csv")
OUTPUT_PNG = Path("results_gap_ghb_stride/speedup.png")


def load_ipcs(csv_path: Path) -> Dict[str, float]:
    if not csv_path.exists():
        raise FileNotFoundError(f"{csv_path} missing")
    with csv_path.open() as f:
        reader = csv.DictReader(f)
        return {row["trace"]: float(row["ipc"]) for row in reader if row["ipc"]}


def compute_speedups(
    baseline: Dict[str, float], ghb: Dict[str, float]
) -> List[Tuple[str, float]]:
    traces = sorted(set(baseline).intersection(ghb))
    return [(trace, ghb[trace] / baseline[trace]) for trace in traces if baseline[trace] > 0]


def geometric_mean(values: List[float]) -> float:
    if not values:
        return 1.0
    product = math.prod(values)
    return product ** (1.0 / len(values))


def write_csv(data: List[Tuple[str, float]]) -> None:
    OUTPUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_CSV.open("w", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["trace", "speedup"])
        for trace, speedup in data:
            writer.writerow([trace, f"{speedup:.4f}"])
        writer.writerow(["GEOMEAN", f"{geometric_mean([s for _, s in data]):.4f}"])


def plot_speedups(data: List[Tuple[str, float]], geomean: float) -> Tuple[Tuple[str, float], Tuple[str, float]]:
    traces = [t for t, _ in data]
    speedups = [s for _, s in data]
    categories = traces + ["GEOMEAN"]
    values = speedups + [geomean]

    plt.figure(figsize=(max(8, len(categories) * 0.6), 5))
    colors = []
    highest = max(zip(categories, values), key=lambda pair: pair[1])
    lowest = min(zip(categories, values), key=lambda pair: pair[1])
    for trace in categories:
        if trace == highest[0]:
            colors.append("#fd8d3c")
        elif trace == lowest[0]:
            colors.append("#9e9ac8")
        else:
            colors.append("#6baed6")

    bars = plt.bar(categories, values, color=colors)
    plt.ylabel("Speedup over no-prefetch")
    plt.xticks(rotation=45, ha="right")

    for bar, val in zip(bars, values):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            f"{val:.3f}",
            ha="center",
            va="bottom",
            fontsize=8,
        )

    plt.figtext(
        0.01,
        0.01,
        f"Highest: {highest[0]} ({highest[1]:.3f}) | Lowest: {lowest[0]} ({lowest[1]:.3f})",
        ha="left",
        va="bottom",
        fontsize=8,
    )

    plt.tight_layout()
    plt.savefig(OUTPUT_PNG, dpi=200)

    return highest, lowest


def main() -> None:
    baseline = load_ipcs(BASELINE_CSV)
    ghb = load_ipcs(GHB_CSV)
    speedups = compute_speedups(baseline, ghb)
    if not speedups:
        raise RuntimeError("no matching traces between baseline and ghb logs")
    geomean = geometric_mean([s for _, s in speedups])
    write_csv(speedups)
    highest, lowest = plot_speedups(speedups, geomean)
    print(f"Wrote speedups for {len(speedups)} traces to {OUTPUT_CSV}")
    print(f"GEOMEAN speedup: {geomean:.4f}")
    print(f"Highest speedup: {highest[0]} ({highest[1]:.4f})")
    print(f"Lowest speedup: {lowest[0]} ({lowest[1]:.4f})")


if __name__ == "__main__":
    main()
