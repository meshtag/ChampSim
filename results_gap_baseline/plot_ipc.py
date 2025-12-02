#!/usr/bin/env python3
"""Plot IPC bar chart from results_gap_baseline/ipc.csv."""
import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} path/to/ipc.csv")
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    workloads, ipcs = [], []
    with csv_path.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            workloads.append(row["trace"])
            ipcs.append(float(row["ipc"]))

    if not workloads:
        print("No rows found in CSV.")
        sys.exit(1)

    plt.figure(figsize=(max(6, len(workloads) * 0.6), 5))
    bars = plt.bar(workloads, ipcs, color="#6baed6")
    plt.ylabel("IPC")
    plt.xticks(rotation=45, ha="right")
    for bar, val in zip(bars, ipcs):
        plt.text(bar.get_x() + bar.get_width() / 2, bar.get_height(), f"{val:.2f}", ha="center", va="bottom", fontsize=8)

    plt.tight_layout()
    out_path = csv_path.with_suffix(".png")
    plt.savefig(out_path, dpi=200)
    print(f"Wrote plot to {out_path}")


if __name__ == "__main__":
    main()
