#!/usr/bin/env python3
"""Extract CPU 0 cumulative IPCs from limit bandwidth no-prefetch logs."""

from pathlib import Path
import re

LOG_DIR = Path("results_gap_limitBW_nopref/logs")
PATTERN = re.compile(r"CPU 0 cumulative IPC:\s*([0-9.]+)")


def extract_ipc(log_path: Path) -> float | None:
    text = log_path.read_text(errors="ignore")
    matches = PATTERN.findall(text)
    if not matches:
        return None
    return float(matches[-1])


def main() -> None:
    if not LOG_DIR.exists():
        raise SystemExit(f"{LOG_DIR} does not exist")

    rows = []
    for log in sorted(LOG_DIR.glob("*.log")):
        trace_name = log.name
        ipc = extract_ipc(log)
        rows.append((trace_name, ipc))

    print(f"{'trace':<36} | {'ipc':>7}")
    print("-" * 48)
    for trace, ipc in rows:
        print(f"{trace:<36} | {ipc if ipc is not None else 'missing':>7}")


if __name__ == "__main__":
    main()
