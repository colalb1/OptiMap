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

    # Separate insertion benchmarks from others
    df_insert = df[df["benchmark_name"].str.contains("Insert")]
    df_others = df[~df["benchmark_name"].str.contains("Insert")]

    # Process and plot insertion benchmarks
    if not df_insert.empty:
        df_insert = (
            df_insert.groupby(["name", "benchmark_name", "key_count"])
            .apply(lambda x: x.sort_values("cpu_time").iloc[2:-2])
            .reset_index(drop=True)
        )
        df_insert_agg = (
            df_insert.groupby(["benchmark_name", "key_count"])["cpu_time"]
            .mean()
            .reset_index()
        )
        for key_count in df_insert_agg["key_count"].unique():
            plt.figure(figsize=(10, 6))
            for lib_name in df_insert_agg["benchmark_name"].unique():
                subset = df_insert_agg[
                    (df_insert_agg["benchmark_name"] == lib_name)
                    & (df_insert_agg["key_count"] == key_count)
                ]
                plt.plot(
                    subset["key_count"],
                    subset["cpu_time"],
                    marker="o",
                    linestyle="-",
                    label=lib_name,
                )
            plt.title(f"Insertion Benchmark ({key_count} keys)")
            plt.xlabel("Number of Entries")
            plt.ylabel("Cumulative CPU Time (ns)")
            plt.legend()
            plt.grid(True)
            plt.savefig(f"plots/Insertion_Benchmark_{key_count}_keys.png")
            plt.close()

    # Process and plot other benchmarks
    if not df_others.empty:
        df_others = (
            df_others.groupby(["name", "benchmark_name", "key_count"])
            .apply(lambda x: x.sort_values("cpu_time").iloc[2:-2])
            .reset_index(drop=True)
        )
        df_others_agg = (
            df_others.groupby(["benchmark_name", "key_count"])["cpu_time"]
            .mean()
            .reset_index()
        )
        for benchmark_name in df_others_agg["benchmark_name"].unique():
            plt.figure(figsize=(10, 6))
            for lib_name in df_others_agg["benchmark_name"].unique():
                if benchmark_name in lib_name:
                    subset = df_others_agg[df_others_agg["benchmark_name"] == lib_name]
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
    if not os.path.exists("plots"):
        os.makedirs("plots")
    csv_files = sys.argv[1:]
    plot_benchmarks(csv_files)
