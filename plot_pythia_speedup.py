import math
import pandas as pd
import matplotlib.pyplot as plt

def load_table(path):
    df = pd.read_csv(path, header=None, names=["trace","ipc"])
    df["trace"] = df["trace"].str.replace(".log","", regex=False)
    return df.set_index("trace")["ipc"].to_dict()

def geomean(values):
    return math.exp(sum(math.log(v) for v in values) / len(values))

def plot_speedup(baseline, prefetcher, title, out_png):
    traces = sorted(baseline.keys())
    speedup = []
    labels = []
    for trace in traces:
        if trace not in prefetcher:
            continue
        labels.append(trace)
        speedup.append(prefetcher[trace] / baseline[trace])
    labels.append("GEOMEAN")
    speedup.append(geomean(speedup[:-1]))
    fig, ax = plt.subplots(figsize=(12, 6))
    bars = ax.bar(labels, speedup, color=["#1f77b4"] * (len(labels)-1) + ["#ff7f0e"])
    for bar, value in zip(bars, speedup):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height()+0.01, f"{value:.3f}",
                ha="center", va="bottom", fontsize=9)
    ax.set_ylabel("Speedup vs. no-prefetcher")
    ax.set_title(title)
    ax.set_xticklabels(labels, rotation=45, ha="right")
    ax.set_ylim(0, max(speedup) * 1.15)
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)

if __name__ == "__main__":
    baseline_full = load_table("results_gap_nopref/ipc_combined.csv")
    baseline_limit = load_table("results_gap_limitBW_nopref/tables/limitbw_ipc.csv")
    pythia_full = load_table("results_gap_fullBW_pythia/tables/pythia_fullbw_ipc.csv")
    pythia_limit = load_table("results_gap_limitBW_pythia/tables/pythia_limitbw_ipc.csv")

    plot_speedup(baseline_full, pythia_full,
                 "Task 4 Bonus: Pythia speedup (full bandwidth)", "pythia_fullbw_speedup.png")
    plot_speedup(baseline_limit, pythia_limit,
                 "Task 4 Bonus: Pythia speedup (limited bandwidth)", "pythia_limitbw_speedup.png")
