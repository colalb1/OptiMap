#include "hashmap.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <vector>

// This benchmark is designed to trigger "accidentally quadratic" behavior
// in hashmaps, which often occurs due to specific insertion patterns causing
// massive numbers of collisions.
// See: https://www.tumblr.com/accidentallyquadratic/153545455987/rust-hash-iteration-reinsertion
static void AccidentallyQuadratic(benchmark::State& state)
{
    // Use a deterministic random number generator with a fixed seed
    // to ensure the sequence of keys is the same for every run.
    std::mt19937_64 rng(12345);
    using M = optimap::HashMap<int, int>;

    // Pre-generate the random numbers to avoid measuring RNG overhead
    // inside the benchmark loop.
    std::vector<int> data(10'000'000);
    for (size_t n = 0; n < 10'000'000; ++n)
    {
        data[n] = static_cast<int>(rng());
    }

    // The main benchmark loop, managed by the google/benchmark library.
    for (auto _ : state)
    {
        // Pause timing to exclude map creation from the measurement.
        state.PauseTiming();
        M map;
        state.ResumeTiming();

        // This is the core operation being measured: inserting 10 million
        // elements into the hashmap. This is the section that was originally
        // timing out.
        for (const auto& val : data)
        {
            // Use operator[] to insert, which default-constructs the value (to 0 for int).
            map[val] = 0;
        }
    }
}

// Register the function as a benchmark
BENCHMARK(AccidentallyQuadratic);
