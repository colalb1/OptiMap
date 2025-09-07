#!/bin/bash

# Build the benchmarks in release mode
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
cd ..

# Create a directory for the results
mkdir -p benchmark_results

# Run the benchmarks for each key count and save the results to a CSV file
for i in {1..14}
do
    ./build/OptiMapBenchmarks --benchmark_out=benchmark_results/results_200000_$i.csv --benchmark_out_format=csv --benchmark_filter=200000
    ./build/OptiMapBenchmarks --benchmark_out=benchmark_results/results_2000000_$i.csv --benchmark_out_format=csv --benchmark_filter=2000000
done

# Generate the plots
python3 plot_benchmarks.py benchmark_results/*.csv
