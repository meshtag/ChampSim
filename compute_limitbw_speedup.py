#!/usr/bin/env python3
"""Compute GHB stride speedups over limited-BW no-prefetch logs."""

from pathlib import Path
import re
import math

BASE_DIR = Path("results_gap_limitBW_nopref/logs")
GHB_DIR = Path("results_gap_limitBW_ghb_stride/logs")
PATTERN = re.compile(r"CPU 0 cumulative IPC:\s*([0-9.]+)")


def read_ipcs(folder: Path) -> dict[str, float]:
    ipcs = {}
    for log in sorted(folder.glob("*.log")):
        text = log.read_text(errors="ignore")
        matches = PATTERN.findall(text)
        if matches:
            ipcs[log.name] = float(matches[-1])
    return ipcs


def geometric_mean(values: list[float]) -> float:
    if not values:
        return 1.0
    product = math.prod(values)
    return product ** (1.0 / len(values))


def main() -> None:
    base_ipcs = read_ipcs(BASE_DIR)
    ghb_ipcs = read_ipcs(GHB_DIR)

    traces = sorted(set(base_ipcs) & set(ghb_ipcs))
    if not traces:
        raise SystemExit("No overlapping traces found")

    ratios: list[tuple[str,float]] = []
    ratios_values = []
    for trace in traces:
        base = base_ipcs[trace]
        ghb = ghb_ipcs[trace]
        ratio = ghb / base if base else float("inf")
        ratios.append((trace, ratio))
        ratios_values.append(ratio)

    print(f"{'trace':<40} | {'bg-speedup':>9}")
    print("-" * 54)
    for trace, ratio in ratios:
        print(f"{trace:<40} | {ratio:9.4f}")

    geomean = geometric_mean(ratios_values)
    print("-" * 54)
    print(f"{'GEOMEAN':<40} | {geomean:9.4f}")


if __name__ == "__main__":
    main()
