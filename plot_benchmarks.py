import os

import matplotlib.pyplot as plt
import polars as pl
import seaborn as sns

# Open the plots:
# ```shell
# open analysis/plots
# ```


def main():
    # Create a directory to save the plots
    output_dir = "analysis/plots"
    os.makedirs(output_dir, exist_ok=True)

    # Define the schema for the data
    schema = {
        "map_name": pl.Utf8,
        "hash_name": pl.Utf8,
        "benchmark": pl.Utf8,
        "test_num": pl.Utf8,
        "test_name": pl.Utf8,
        "num_operations": pl.Utf8,  # Read as string to handle errors
        "nanoseconds_per_op": pl.Utf8,  # Read as string to handle errors
        "memory_usage_mb": pl.Utf8,  # Read as string to handle errors
    }

    # Read the data file line by line to handle errors
    with open("data/all_new.txt", "r") as f:
        lines = f.readlines()

    # Filter out error lines
    lines = [line for line in lines if "ERROR" not in line and "TIMEOUT" not in line]

    # Create a new cleaned data file
    cleaned_data_path = "data/all_new_cleaned.txt"
    with open(cleaned_data_path, "w") as f:
        f.writelines(lines)

    # Read and clean the data
    try:
        df = pl.read_csv(
            cleaned_data_path,
            separator=";",
            has_header=False,
            new_columns=list(schema.keys()),
            schema_overrides=schema,
            ignore_errors=True,
        )
    except Exception as e:
        print(f"Error reading the data file: {e}")
        return

    # Convert columns to numeric, coercing errors to null
    df = df.with_columns(
        [
            pl.col("num_operations").str.strip_chars().cast(pl.Int64, strict=False),
            pl.col("nanoseconds_per_op")
            .str.strip_chars()
            .cast(pl.Float64, strict=False),
            pl.col("memory_usage_mb").str.strip_chars().cast(pl.Float64, strict=False),
        ]
    )

    # Clean up map and hash names by removing quotes and stripping whitespace
    df = df.with_columns(
        [
            pl.col("map_name")
            .str.replace_all('"', "")
            .str.strip_chars()
            .alias("map_name"),
            pl.col("hash_name")
            .str.replace_all('"', "")
            .str.strip_chars()
            .alias("hash_name"),
            pl.col("benchmark")
            .str.replace_all('"', "")
            .str.strip_chars()
            .alias("benchmark"),
        ]
    )

    # Create a combined column for easier plotting
    df = df.with_columns(
        (pl.col("map_name") + " (" + pl.col("hash_name") + ")").alias("map_with_hash")
    )

    benchmarks = [
        "Copy",
        "CtorDtorEmptyMap",
        "CtorDtorSingleEntryMap",
        "InsertHugeInt",
        "IterateIntegers",
        "RandomDistinct2",
        "RandomFind_200",
        "RandomFind_2000",
        "RandomFind_500000",
        "RandomFindString",
        "RandomFindString_1000000",
        "RandomInsertErase",
        "RandomInsertEraseStrings",
    ]

    for bench in benchmarks:
        print(f"Generating plot for {bench}...")
        bench_df = df.filter(pl.col("benchmark") == bench)

        if bench_df.height == 0:
            print(f"No data found for benchmark: {bench}")
            continue

        plt.figure(figsize=(12, 8))

        # --- Performance Plot ---
        plt.figure(figsize=(12, 8))

        agg_df_perf = (
            bench_df.group_by("map_with_hash")
            .agg(pl.mean("nanoseconds_per_op").alias("mean_nanoseconds_per_op"))
            .sort("mean_nanoseconds_per_op")
        )

        sns.barplot(
            x="mean_nanoseconds_per_op",
            y="map_with_hash",
            data=agg_df_perf.to_pandas(),
            palette="viridis",
        )

        plt.title(f"Performance Comparison for {bench}")
        plt.xlabel("Mean Nanoseconds per Operation (lower is better)")
        plt.ylabel("Map Type and Hash Function")
        plt.tight_layout()

        ax = plt.gca()
        for label in ax.get_yticklabels():
            if "optimap::HashMap" in label.get_text():
                label.set_weight("bold")

        plot_path = os.path.join(output_dir, f"{bench}_performance.png")
        plt.savefig(plot_path)
        plt.close()
        print(f"Saved plot to {plot_path}")

        # --- Memory Usage Plot ---
        plt.figure(figsize=(12, 8))

        agg_df_mem = (
            bench_df.group_by("map_with_hash")
            .agg(pl.mean("memory_usage_mb").alias("mean_memory_usage_mb"))
            .sort("mean_memory_usage_mb")
        )

        sns.barplot(
            x="mean_memory_usage_mb",
            y="map_with_hash",
            data=agg_df_mem.to_pandas(),
            palette="plasma",
        )

        plt.title(f"Memory Usage for {bench}")
        plt.xlabel("Mean Memory Usage (MB)")
        plt.ylabel("Map Type and Hash Function")
        plt.tight_layout()

        ax = plt.gca()
        for label in ax.get_yticklabels():
            if "optimap::HashMap" in label.get_text():
                label.set_weight("bold")

        plot_path = os.path.join(output_dir, f"{bench}_memory.png")
        plt.savefig(plot_path)
        plt.close()
        print(f"Saved plot to {plot_path}")


if __name__ == "__main__":
    main()
