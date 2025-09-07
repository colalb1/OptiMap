import os
import sys

import matplotlib.pyplot as plt
import pandas as pd


def plot_benchmarks(csv_files):
    """
    Reads benchmark data from CSV files, processes it, and generates plots.
    """
    # Read and concatenate all CSV files
    df = pd.concat([pd.read_csv(f) for f in csv_files])

    # Extract benchmark name and key count
    df["benchmark_name"] = df["name"].apply(lambda x: x.split("/")[0])
    df["key_count"] = df["name"].apply(lambda x: int(x.split("/")[1]))

    # Calculate the average of the middle 10 runs
    df = (
        df.groupby(["name", "benchmark_name", "key_count"])
        .apply(lambda x: x.sort_values("cpu_time").iloc[2:-2])
        .reset_index(drop=True)
    )
    df_agg = (
        df.groupby(["benchmark_name", "key_count"])["cpu_time"].mean().reset_index()
    )

    # Create a directory for the plots
    if not os.path.exists("plots"):
        os.makedirs("plots")

    # Generate a plot for each benchmark
    for benchmark_name in df_agg["benchmark_name"].unique():
        plt.figure(figsize=(10, 6))
        for lib_name in df_agg["benchmark_name"].unique():
            if benchmark_name in lib_name:
                subset = df_agg[df_agg["benchmark_name"] == lib_name]
                plt.plot(
                    subset["key_count"],
                    subset["cpu_time"],
                    marker="o",
                    linestyle="-",
                    label=lib_name,
                )
        plt.title(benchmark_name)
        plt.xlabel("Number of Entries")
        plt.ylabel("CPU Time (ns)")
        plt.legend()
        plt.grid(True)
        plt.savefig(f"plots/{benchmark_name}.png")
        plt.close()


if __name__ == "__main__":
    csv_files = sys.argv[1:]
    plot_benchmarks(csv_files)
