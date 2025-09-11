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

FOR /L %%N IN (1,1,5) DO (
    echo Running benchmark iteration %%N...

    echo Running Insert benchmarks...
    %BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Insert.*" --benchmark_min_time=0.01s > benchmark_results/results_insert_%%N.csv

    echo Running Erase benchmarks...
    %BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Erase.*" --benchmark_min_time=0.01s > benchmark_results/results_erase_%%N.csv

    echo Running Replace benchmarks...
    %BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Replace.*" --benchmark_min_time=0.01s > benchmark_results/results_replace_%%N.csv

    echo Running Lookup benchmarks...
    %BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Lookup.*" --benchmark_min_time=0.01s > benchmark_results/results_lookup_%%N.csv

    echo Running Iterate benchmarks...
    %BENCH_EXE% --benchmark_format=csv --benchmark_filter=".*Iterate.*" --benchmark_min_time=0.01s > benchmark_results/results_iterate_%%N.csv
)

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
