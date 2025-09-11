#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "hashmap.hpp"

// MurmurHash3 finalizer
static inline uint32_t murmur3_32_finalizer(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static inline uint64_t murmur3_64_finalizer(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

// FNV-1a hash for strings
struct Fnv1a {
    size_t operator()(const std::string& s) const {
        size_t hash = 0xcbf29ce484222325;
        for (char c : s) {
            hash ^= c;
            hash *= 0x100000001b3;
        }
        return hash;
    }
};

struct Murmur3_32 {
    size_t operator()(uint32_t k) const { return murmur3_32_finalizer(k); }
};

struct Murmur3_64 {
    size_t operator()(uint64_t k) const { return murmur3_64_finalizer(k); }
};

// Benchmark fixture for 32-bit integer key, 32-bit value
class Int32Int32Fixture : public benchmark::Fixture {
   public:
    void SetUp(const ::benchmark::State& state) {
        size_t num_keys = static_cast<size_t>(state.range(0));
        keys.resize(num_keys);
        non_existing_keys.resize(1000);

        // Fill keys with sequential values
        for (size_t i = 0; i < num_keys; ++i) {
            keys[i] = static_cast<uint32_t>(i);
        }

        // Fill non-existing keys
        for (size_t i = 0; i < 1000; ++i) {
            non_existing_keys[i] = static_cast<uint32_t>(num_keys + i);
        }

        // Use a fixed seed for reproducibility and to avoid Windows random_device issues
        std::mt19937 rng(42);
        std::shuffle(keys.begin(), keys.end(), rng);
        std::shuffle(non_existing_keys.begin(), non_existing_keys.end(), rng);
    }

    std::vector<uint32_t> keys;
    std::vector<uint32_t> non_existing_keys;
};

// Total time to insert N nonexisting keys
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
        for (int i = 0; i < state.range(0); ++i) {
            map.insert(keys[i], keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], keys[i]});
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], keys[i]});
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_Insert)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_Insert)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_Insert)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_EraseExisting)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], keys[i]);
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_EraseExisting)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], keys[i]});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_EraseExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], keys[i]});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_EraseExisting)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to replace 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_ReplaceExisting)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_ReplaceExisting)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], keys[i]});
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_ReplaceExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], keys[i]});
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_EraseNonExisting)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_EraseNonExisting)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_EraseNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_LookupExisting)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_LookupExisting)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_LookupExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_LookupNonExisting)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_LookupNonExisting)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_LookupNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to iterate over 5,000 keys with N keys in the table
BENCHMARK_DEFINE_F(Int32Int32Fixture, OptiMap_Iterate)(benchmark::State& state) {
    optimap::HashMap<uint32_t, uint32_t, Murmur3_32> map;
    for (uint32_t key : keys) {
        map.insert(key, key);
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, StdUnorderedMap_Iterate)(benchmark::State& state) {
    std::unordered_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int32Int32Fixture, AbslFlatHashMap_Iterate)(benchmark::State& state) {
    absl::flat_hash_map<uint32_t, uint32_t, Murmur3_32> map;
    map.max_load_factor(0.875);
    for (uint32_t key : keys) {
        map.insert({key, key});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int32Int32Fixture, OptiMap_Iterate)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, StdUnorderedMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int32Int32Fixture, AbslFlatHashMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);

// ----------------------------------------------------------------------------

// Benchmark fixture for 64-bit integer key, 448-bit value
struct Value448 {
    std::array<uint8_t, 56> data;
};

class Int64Value448Fixture : public benchmark::Fixture {
   public:
    void SetUp(const ::benchmark::State& state) {
        size_t num_keys = static_cast<size_t>(state.range(0));
        keys.resize(num_keys);
        non_existing_keys.resize(1000);

        // Fill keys with sequential values
        for (size_t i = 0; i < num_keys; ++i) {
            keys[i] = static_cast<uint64_t>(i);
        }

        // Fill non-existing keys
        for (size_t i = 0; i < 1000; ++i) {
            non_existing_keys[i] = static_cast<uint64_t>(num_keys + i);
        }

        // Use a fixed seed for reproducibility
        std::mt19937 rng(42);
        std::shuffle(keys.begin(), keys.end(), rng);
        std::shuffle(non_existing_keys.begin(), non_existing_keys.end(), rng);
    }

    std::vector<uint64_t> keys;
    std::vector<uint64_t> non_existing_keys;
};

// Total time to insert N nonexisting keys
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
        for (int i = 0; i < state.range(0); ++i) {
            map.insert(keys[i], Value448{});
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, StdUnorderedMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        std::unordered_map<uint64_t, Value448, Murmur3_64> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], Value448{}});
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, AbslFlatHashMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], Value448{}});
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_Insert)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_Insert)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_Insert)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_EraseExisting)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], Value448{});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, StdUnorderedMap_EraseExisting)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], Value448{}});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, AbslFlatHashMap_EraseExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], Value448{}});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to replace 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_ReplaceExisting)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], Value448{});
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, StdUnorderedMap_ReplaceExisting)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], Value448{}});
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, AbslFlatHashMap_ReplaceExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], Value448{}});
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_EraseNonExisting)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture,
                   StdUnorderedMap_EraseNonExisting)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture,
                   AbslFlatHashMap_EraseNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_LookupExisting)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, StdUnorderedMap_LookupExisting)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, AbslFlatHashMap_LookupExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_LookupNonExisting)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture,
                   StdUnorderedMap_LookupNonExisting)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture,
                   AbslFlatHashMap_LookupNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to iterate over 5,000 keys with N keys in the table
BENCHMARK_DEFINE_F(Int64Value448Fixture, OptiMap_Iterate)(benchmark::State& state) {
    optimap::HashMap<uint64_t, Value448, Murmur3_64> map;
    for (uint64_t key : keys) {
        map.insert(key, Value448{});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, StdUnorderedMap_Iterate)(benchmark::State& state) {
    std::unordered_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(Int64Value448Fixture, AbslFlatHashMap_Iterate)(benchmark::State& state) {
    absl::flat_hash_map<uint64_t, Value448, Murmur3_64> map;
    map.max_load_factor(0.875);
    for (uint64_t key : keys) {
        map.insert({key, Value448{}});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(Int64Value448Fixture, OptiMap_Iterate)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, StdUnorderedMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(Int64Value448Fixture, AbslFlatHashMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);

// ----------------------------------------------------------------------------

// Benchmark fixture for 16-char c-string key, 64-bit value
class String16Value64Fixture : public benchmark::Fixture {
   public:
    void SetUp(const ::benchmark::State& state) {
        size_t num_keys = static_cast<size_t>(state.range(0));
        keys.resize(num_keys);
        non_existing_keys.resize(1000);

        for (size_t i = 0; i < num_keys; ++i) {
            keys[i] = std::to_string(i);
            keys[i].resize(16, ' ');
        }

        for (size_t i = 0; i < 1000; ++i) {
            non_existing_keys[i] = std::to_string(num_keys + i);
            non_existing_keys[i].resize(16, ' ');
        }

        // Use a fixed seed for reproducibility
        std::mt19937 rng(42);
        std::shuffle(keys.begin(), keys.end(), rng);
        std::shuffle(non_existing_keys.begin(), non_existing_keys.end(), rng);
    }

    std::vector<std::string> keys;
    std::vector<std::string> non_existing_keys;
};

// Total time to insert N nonexisting keys
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        optimap::HashMap<std::string, uint64_t, Fnv1a> map;
        for (int i = 0; i < state.range(0); ++i) {
            map.insert(keys[i], 0);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, StdUnorderedMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        std::unordered_map<std::string, uint64_t, Fnv1a> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], 0});
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, AbslFlatHashMap_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
        map.max_load_factor(0.875);
        for (int i = 0; i < state.range(0); ++i) {
            map.insert({keys[i], 0});
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_Insert)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_Insert)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_Insert)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_EraseExisting)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], 0);
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, StdUnorderedMap_EraseExisting)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], 0});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, AbslFlatHashMap_EraseExisting)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(keys[i]);
        }
        state.PauseTiming();
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], 0});
        }
        state.ResumeTiming();
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_EraseExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to replace 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_ReplaceExisting)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert(keys[i], 0);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   StdUnorderedMap_ReplaceExisting)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], 0});
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   AbslFlatHashMap_ReplaceExisting)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.insert({keys[i], 0});
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_ReplaceExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to erase 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_EraseNonExisting)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   StdUnorderedMap_EraseNonExisting)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   AbslFlatHashMap_EraseNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.erase(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_EraseNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 existing keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_LookupExisting)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   StdUnorderedMap_LookupExisting)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   AbslFlatHashMap_LookupExisting)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_LookupExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to look up 1,000 nonexisting keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_LookupNonExisting)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   StdUnorderedMap_LookupNonExisting)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture,
                   AbslFlatHashMap_LookupNonExisting)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            map.find(non_existing_keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_LookupNonExisting)
    ->DenseRange(100000, 1000000, 100000);

// Time to iterate over 5,000 keys with N keys in the table
BENCHMARK_DEFINE_F(String16Value64Fixture, OptiMap_Iterate)(benchmark::State& state) {
    optimap::HashMap<std::string, uint64_t, Fnv1a> map;
    for (const auto& key : keys) {
        map.insert(key, 0);
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, StdUnorderedMap_Iterate)(benchmark::State& state) {
    std::unordered_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_DEFINE_F(String16Value64Fixture, AbslFlatHashMap_Iterate)(benchmark::State& state) {
    absl::flat_hash_map<std::string, uint64_t, Fnv1a> map;
    map.max_load_factor(0.875);
    for (const auto& key : keys) {
        map.insert({key, 0});
    }
    for (auto _ : state) {
        for (int i = 0; i < 5000; ++i) {
            map.find(keys[i]);
        }
    }
}

BENCHMARK_REGISTER_F(String16Value64Fixture, OptiMap_Iterate)->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, StdUnorderedMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);
BENCHMARK_REGISTER_F(String16Value64Fixture, AbslFlatHashMap_Iterate)
    ->DenseRange(100000, 1000000, 100000);

BENCHMARK_MAIN();
