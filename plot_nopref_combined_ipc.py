#!/usr/bin/env python3
"""Combine no-prefetcher IPC logs and plot a single bar chart."""

from pathlib import Path
import csv
import re
from typing import Dict, List, Optional, Set, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Prefer the refreshed runs first so they override any duplicates in the older set.
LOG_DIRS = [
    Path("new_baseline_no_pref_for_missing_confs/results_gap_nopref/logs"),
    Path("results_gap_nopref/logs"),
]

OUTPUT_CSV = Path("results_gap_nopref/ipc_combined.csv")
OUTPUT_PNG = OUTPUT_CSV.with_suffix(".png")

IPC_PATTERN = re.compile(r"CPU 0 cumulative IPC:\s*([0-9.]+)")


def extract_ipc(log_path: Path) -> Optional[float]:
    """Return the last cumulative IPC value in the log, if present."""
    text = log_path.read_text(errors="ignore")
    matches = IPC_PATTERN.findall(text)
    if not matches:
        return None
    return float(matches[-1])


def collect_ipc() -> Tuple[Dict[str, float], List[str]]:
    entries: Dict[str, float] = {}
    seen_traces: Set[str] = set()

    for log_dir in LOG_DIRS:
        if not log_dir.is_dir():
            continue

        for log_path in sorted(log_dir.glob("*.log")):
            trace_name = log_path.name[:-4]  # drop .log suffix
            seen_traces.add(trace_name)

            if trace_name in entries:
                continue

            ipc = extract_ipc(log_path)
            if ipc is not None:
                entries[trace_name] = ipc

    missing = sorted(seen_traces - entries.keys())
    return entries, missing


def write_csv(entries: dict[str, float]) -> None:
    OUTPUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_CSV.open("w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(["trace", "ipc"])
        for trace in sorted(entries):
            writer.writerow([trace, f"{entries[trace]:.4f}"])


def find_extremes(entries: dict[str, float]) -> Tuple[Tuple[str, float], Tuple[str, float]]:
    items = list(entries.items())
    highest = max(items, key=lambda item: item[1])
    lowest = min(items, key=lambda item: item[1])
    return highest, lowest


def plot(entries: dict[str, float], missing: list[str], extremes: Optional[Tuple[Tuple[str, float], Tuple[str, float]]]) -> None:
    traces = sorted(entries)
    ipcs = [entries[t] for t in traces]

    plt.figure(figsize=(max(8, len(traces) * 0.7), 5))
    colors = []
    highest_name = highest_val = None
    lowest_name = lowest_val = None
    if extremes is not None:
        highest, lowest = extremes
        highest_name, highest_val = highest
        lowest_name, lowest_val = lowest
    for trace in traces:
        if highest_name and trace == highest_name:
            colors.append("#fd8d3c")
        elif lowest_name and trace == lowest_name:
            colors.append("#9e9ac8")
        else:
            colors.append("#6baed6")
    bars = plt.bar(traces, ipcs, color=colors)
    plt.ylabel("Cumulative IPC")
    plt.xticks(rotation=45, ha="right")

    for bar, val in zip(bars, ipcs):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            f"{val:.3f}",
            ha="center",
            va="bottom",
            fontsize=8,
        )

    if missing:
        note = "Missing IPC for: " + ", ".join(missing)
        plt.figtext(0.99, 0.01, note, ha="right", va="bottom", fontsize=8, color="red")
    if extremes is not None:
        highest = extremes[0]
        lowest = extremes[1]
        summary = (
            f"Highest IPC: {highest[0]} ({highest[1]:.3f}) | "
            f"Lowest IPC: {lowest[0]} ({lowest[1]:.3f})"
        )
        plt.figtext(0.01, 0.01, summary, ha="left", va="bottom", fontsize=8)

    plt.tight_layout()
    plt.savefig(OUTPUT_PNG, dpi=200)


def main() -> None:
    entries, missing = collect_ipc()
    write_csv(entries)
    extremes = find_extremes(entries) if entries else None
    plot(entries, missing, extremes)

    print(f"Wrote {len(entries)} IPC values to {OUTPUT_CSV}")
    if missing:
        print("Logs missing IPC data:", ", ".join(missing))
    if extremes is not None:
        highest, lowest = extremes
        print(f"Highest IPC: {highest[0]} ({highest[1]:.4f})")
        print(f"Lowest IPC: {lowest[0]} ({lowest[1]:.4f})")


if __name__ == "__main__":
    main()
