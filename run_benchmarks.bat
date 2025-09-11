@echo off

REM Build the benchmarks in release mode
echo Building benchmarks...
cd build
cmake --build . --config Release --target OptiMapBenchmarks
if %errorlevel% neq 0 (
    echo Build failed.
    exit /b %errorlevel%
)
cd ..

REM Clean and create a directory for the results
if exist benchmark_results (
    rmdir /s /q benchmark_results
)
mkdir benchmark_results

REM Define the path to the executable
set BENCH_EXE=.\build\Release\OptiMapBenchmarks.exe

REM Check if the executable exists
if not exist %BENCH_EXE% (
    echo Benchmark executable not found at %BENCH_EXE%
    exit /b 1
)

REM Run the benchmarks for each key count and save the results to a CSV file
echo Running benchmarks...

REM Run Insert benchmarks with different sizes
echo Running Insert benchmarks with 200,000 elements...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Insert/200000$" --benchmark_min_time=0.01s > benchmark_results/results_insert_200000.csv

echo Running Insert benchmarks with 2,000,000 elements...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Insert/2000000$" --benchmark_min_time=0.01s > benchmark_results/results_insert_2000000.csv

REM For 20M elements, use even shorter min time as it's very large
echo Running Insert benchmarks with 20,000,000 elements (this may take a while)...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Insert/20000000$" --benchmark_min_time=0.001s > benchmark_results/results_insert_20000000.csv

REM Run other benchmarks (Erase, Replace, Lookup, Iterate)
echo Running Erase benchmarks...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Erase.*" --benchmark_min_time=0.01s > benchmark_results/results_erase.csv

echo Running Replace benchmarks...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Replace.*" --benchmark_min_time=0.01s > benchmark_results/results_replace.csv

echo Running Lookup benchmarks...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Lookup.*" --benchmark_min_time=0.01s > benchmark_results/results_lookup.csv

echo Running Iterate benchmarks...
%BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Iterate.*" --benchmark_min_time=0.01s > benchmark_results/results_iterate.csv

echo.
echo Benchmark analysis complete!
echo Results saved in benchmark_results directory:
dir /B benchmark_results\*.csv

REM Optionally generate plots if Python is available
where python >nul 2>nul
if %errorlevel% equ 0 (
    echo.
    echo Generating plots...
    python plot_benchmarks.py benchmark_results/*.csv
    echo Plots are available in the 'plots' directory.
) else (
    echo.
    echo Python not found in PATH. Skipping plot generation.
)
