#!/usr/bin/env python3
"""Compare full-bandwidth and limited-bandwidth GHB speedups."""

from pathlib import Path
import csv
import math

def load_csv(path: Path) -> dict[str, float]:
    with path.open() as f:
        reader = csv.DictReader(f)
        return {row["trace"]: float(row["ipc"]) for row in reader if row["ipc"]}

def ratios(baseline, ghb):
    traces = sorted(set(baseline) & set(ghb))
    return {t: ghb[t] / baseline[t] for t in traces if baseline[t] > 0}

def geomean(values):
    prod = math.prod(values)
    return prod ** (1 / len(values)) if values else 1.0

def main():
    full_baseline = load_csv(Path("results_gap_nopref/ipc_combined.csv"))
    full_ghb = load_csv(Path("results_gap_ghb_stride/ipc.csv"))
    full_ratios = ratios(full_baseline, full_ghb)

    limit_baseline = load_csv(Path("results_gap_limitBW_nopref/ipc.csv"))
    limit_ghb = load_csv(Path("results_gap_limitBW_ghb_stride/ipc.csv"))
    limit_ratios = ratios(limit_baseline, limit_ghb)

    full_geomean = geomean(list(full_ratios.values()))
    limit_geomean = geomean(list(limit_ratios.values()))

    print(f"Full BW GEOMEAN speedup: {full_geomean:.4f}")
    print(f"Limited BW GEOMEAN speedup: {limit_geomean:.4f}")
    print()
    print("Trace    FullBW  LimitedBW  Δspeedup")
    for trace in sorted(full_ratios):
        diff = limit_ratios.get(trace, 0) - full_ratios.get(trace, 0)
        print(f"{trace:20} {full_ratios[trace]:.4f}   {limit_ratios.get(trace, 0):.4f}   {diff:+.4f}")
    print()
    highest_limit = max(limit_ratios.items(), key=lambda kv: kv[1])
    lowest_limit = min(limit_ratios.items(), key=lambda kv: kv[1])
    print("Limit BW highest speedup:", highest_limit)
    print("Limit BW lowest speedup: ", lowest_limit)
    print("Speedup flips across 1.0:")
    flips = [t for t in limit_ratios if (full_ratios.get(t,1) > 1) != (limit_ratios[t] > 1)]
    print(flips or "None")

if __name__ == "__main__":
    main()
