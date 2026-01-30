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
