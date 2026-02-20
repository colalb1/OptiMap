#include <benchmark/benchmark.h>

#include <numeric>
#include <vector>

// Benchmark for array sum using std::accumulate
static void BM_ArraySum(benchmark::State& state) {
    // Benchmark setup: big vector
    const int array_size = 10000;
    std::vector<int> vec(array_size, 1);

    // Summation benchmark
    for (auto _ : state) {
        int sum = std::accumulate(vec.begin(), vec.end(), 0);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ArraySum);

// Benchmark for array sum w/different sizes
static void BM_ArraySumVariableSize(benchmark::State& state) {
    // Get state size
    const int array_size = state.range(0);
    std::vector<int> vec(array_size, 1);

    // Summation bench
    for (auto _ : state) {
        int sum = std::accumulate(vec.begin(), vec.end(), 0);
        benchmark::DoNotOptimize(sum);
    }

    // Report the number of processed items
    state.SetItemsProcessed(state.iterations() * array_size);
}
BENCHMARK(BM_ArraySumVariableSize)->RangeMultiplier(2)->Range(1024, 1024 * 1024);
