#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// Platform-specific intrinsics
#if (defined(__x86_64__) || defined(__i386))
#if defined(__GNUC__) || defined(__clang__)
#include <emmintrin.h>  // SSE2
#include <immintrin.h>  // AES-NI and SSE intrinsics
#include <smmintrin.h>
#include <wmmintrin.h>  // For AESENC/AESKEYGEN
#define GXHASH_HAVE_AES_INTRINSICS 1
#endif
#endif

namespace gxhash {
namespace detail {

// Helpers
static inline uint64_t rotl64(uint64_t x, int r) noexcept { return (x << r) | (x >> (64 - r)); }

static inline uint64_t fetch_u64_unaligned(const void* p) noexcept {
    uint64_t x;
    std::memcpy(&x, p, sizeof(x));
    return x;
}

static inline uint32_t fetch_u32_unaligned(const void* p) noexcept {
    uint32_t x;
    std::memcpy(&x, p, sizeof(x));
    return x;
}

static inline uint64_t final_avalanche(uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static inline uint64_t mix64(uint64_t a, uint64_t b) noexcept {
    uint64_t z = a ^ b;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    return z;
}

// Runtime AES detection (x86)
static inline bool cpu_supports_aes() noexcept {
#if (defined(__x86_64__) || defined(__i386))
#if defined(__GNUC__) || defined(__clang__)
#if defined(__has_builtin)
#if __has_builtin(__builtin_cpu_supports)
    return __builtin_cpu_supports("aes");
#else
    return false;
#endif
#else
    return __builtin_cpu_supports("aes");
#endif
#else
    return false;
#endif
#else
    return false;
#endif
}

}  // namespace detail

// gxhash64 implementation
inline uint64_t gxhash64(const void* data, size_t len, uint64_t seed = 0) noexcept {
    const size_t orig_len = len;  // Save original length for finalization

    // Defensive handling of null pointer with non-zero length
    if (data == nullptr) {
        if (orig_len == 0) {
            // empty input \implies deterministic result derived from seed
            return detail::final_avalanche(seed ^ 0x9e3779b97f4a7c15ULL);
        }

        // Invalid input: data==nullptr but len>0. In debug builds assert;
        // in release builds return a deterministic mix so we avoid UB/crash.
        assert(false && "gxhash64: data == nullptr but len > 0 (invalid)");
        return detail::final_avalanche(seed ^
                                       (static_cast<uint64_t>(orig_len) * 0x9e3779b97f4a7c15ULL));
    }

    const uint8_t* ptr = static_cast<const uint8_t*>(data);

    // AES-NI accelerated path (if compiled & available)
#if defined(GXHASH_HAVE_AES_INTRINSICS)
    static bool has_aes = detail::cpu_supports_aes();
    if (has_aes) {
        const uint64_t C1 = 0x9e3779b97f4a7c15ULL;
        const uint64_t C2 = 0xc6a4a7935bd1e995ULL;
        __m128i acc =
            _mm_set_epi64x(static_cast<long long>(seed ^ C1), static_cast<long long>((~seed) ^ C2));

        const __m128i RK1 = _mm_set_epi64x(0x243f6a8885a308d3ULL, 0x13198a2e03707344ULL);
        const __m128i RK2 = _mm_set_epi64x(0xa4093822299f31d0ULL, 0x082efa98ec4e6c89ULL);
        const __m128i RK3 = _mm_set_epi64x(0x452821e638d01377ULL, 0xbe5466cf34e90c6cULL);

        size_t remaining = len;
        const uint8_t* p = ptr;

        while (remaining >= 16) {
            __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
            acc = _mm_xor_si128(acc, block);
            acc = _mm_aesenc_si128(acc, RK1);
            acc = _mm_aesenc_si128(acc, RK2);
            acc = _mm_aesenc_si128(acc, RK3);

            p += 16;

            remaining -= 16;
        }

        if (remaining > 0) {
            alignas(16) uint8_t tail[16];
            std::memset(tail, 0, sizeof(tail));
            std::memcpy(tail, p, remaining);

            __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tail));

            acc = _mm_xor_si128(acc, block);
            acc = _mm_aesenc_si128(acc, RK2);
            acc = _mm_aesenc_si128(acc, RK3);
        }

        // Store accumulator to bytes and extract two 64-bit lanes
        alignas(16) uint8_t acc_bytes[16];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(acc_bytes), acc);
        uint64_t lo = detail::fetch_u64_unaligned(acc_bytes);
        uint64_t hi = detail::fetch_u64_unaligned(acc_bytes + 8);

        uint64_t folded = hi ^ lo ^ seed ^ (static_cast<uint64_t>(orig_len) << 3);
        return detail::final_avalanche(folded);
    }
#endif  // GXHASH_HAVE_AES_INTRINSICS

    // Portable fallback
    uint64_t state = seed ^ 0x9e3779b97f4a7c15ULL;
    const uint64_t MUL1 = 0x9ddfea08eb382d69ULL;

    // Use local pointer & local remaining to not touch original len variable
    const uint8_t* p = ptr;
    size_t remaining = len;

    while (remaining >= 16) {
        uint64_t a = detail::fetch_u64_unaligned(p);
        uint64_t b = detail::fetch_u64_unaligned(p + 8);

        state += a * MUL1;
        uint64_t m = detail::mix64(a ^ (detail::rotl64(b, 23) + (state ^ (state >> 41))),
                                   b ^ (state + 0x9e3779b97f4a7c15ULL));
        state ^= m;
        state = detail::rotl64(state, 27) * 0x3C79AC492BA7B653ULL;

        p += 16;
        remaining -= 16;
    }

    if (remaining >= 8) {
        uint64_t a = detail::fetch_u64_unaligned(p);

        state += a ^ 0x9e3779b97f4a7c15ULL;
        state = detail::mix64(state, a);

        p += 8;
        remaining -= 8;
    }

    if (remaining >= 4) {
        uint32_t a32 = detail::fetch_u32_unaligned(p);

        state += static_cast<uint64_t>(a32) * 0x85ebca6bULL;
        state = detail::mix64(state, static_cast<uint64_t>(a32));

        p += 4;
        remaining -= 4;
    }

    if (remaining > 0) {
        uint64_t tail = 0;
        for (size_t i = 0; i < remaining; ++i) {
            tail |= (uint64_t(p[i]) << (i * 8));
        }

        state += tail * 0x27d4eb2f165667c5ULL;
        state = detail::mix64(state, tail);
    }

    state ^= (static_cast<uint64_t>(seed) << 7);
    state += (static_cast<uint64_t>(orig_len) << 3);  // Use original length
    return detail::final_avalanche(state);
}

// 128-bit GxHash function returns a 128-bit hash as two uint64_t halves.
inline std::pair<uint64_t, uint64_t> gxhash128(const void* data, size_t len,
                                               uint64_t seed = 0) noexcept {
    // For simplicity, reuse two independent 64-bit hashes with different seeds
    uint64_t h1 = gxhash64(data, len, seed);
    uint64_t h2 = gxhash64(data, len, seed ^ 0x9e3779b97f4a7c15ULL);
    return {h1, h2};
}

// Combine a seed with a value's hash (like boost)
static inline void hash_combine(std::size_t& seed, std::size_t value) noexcept {
    // A widely used 64-bit mix constant
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

// Fundamental templates
template <typename T>
struct GxHash {
    std::size_t operator()(const T& key) const noexcept {
        if constexpr (std::is_integral_v<T>) {
            // Hash integer by treating its bytes as data
            return static_cast<std::size_t>(gxhash64(&key, sizeof(T), 0));
        } else if constexpr (std::is_floating_point_v<T>) {
            // For floats: treat -0.0 same as 0.0
            if (key == 0.0) {
                return GxHash<int>{}(0);
            }
            return static_cast<std::size_t>(gxhash64(&key, sizeof(T), 0));
        } else {
            static_assert(std::is_trivially_copyable_v<T>,
                          "GxHash requires trivially copyable types");
            return static_cast<std::size_t>(gxhash64(&key, sizeof(T), 0));
        }
    }
};

// Pointers
template <typename T>
struct GxHash<T*> {
    std::size_t operator()(T* ptr) const noexcept {
        // Hash the address value
        uint64_t addr = reinterpret_cast<uintptr_t>(ptr);
        return static_cast<std::size_t>(gxhash64(&addr, sizeof(addr), 0));
    }
};

template <>
struct GxHash<std::nullptr_t> {
    std::size_t operator()(std::nullptr_t) const noexcept { return 0; }
};

template <typename T, typename Deleter>
struct GxHash<std::unique_ptr<T, Deleter>> {
    std::size_t operator()(const std::unique_ptr<T, Deleter>& p) const noexcept {
        return GxHash<T*>{}(p.get());
    }
};

template <typename T>
struct GxHash<std::shared_ptr<T>> {
    std::size_t operator()(const std::shared_ptr<T>& p) const noexcept {
        return GxHash<T*>{}(p.get());
    }
};

// Strings
template <>
struct GxHash<std::string> {
    std::size_t operator()(const std::string& s) const noexcept {
        return static_cast<std::size_t>(gxhash64(s.data(), s.size(), 0));
    }
};

template <>
struct GxHash<std::string_view> {
    std::size_t operator()(const std::string_view& sv) const noexcept {
        return static_cast<std::size_t>(gxhash64(sv.data(), sv.size(), 0));
    }
};

template <>
struct GxHash<const char*> {
    std::size_t operator()(const char* s) const noexcept {
        return static_cast<std::size_t>(gxhash64(s, std::strlen(s), 0));
    }
};

// Pair and tuple
template <typename T1, typename T2>
struct GxHash<std::pair<T1, T2>> {
    std::size_t operator()(const std::pair<T1, T2>& p) const noexcept {
        std::size_t seed = 0;
        hash_combine(seed, GxHash<T1>{}(p.first));
        hash_combine(seed, GxHash<T2>{}(p.second));
        return seed;
    }
};

template <typename... Types>
struct GxHash<std::tuple<Types...>> {
    std::size_t operator()(const std::tuple<Types...>& t) const noexcept {
        std::size_t seed = 0;
        std::apply(
            [&](auto&&... elems) {
                ((hash_combine(seed, GxHash<std::decay_t<decltype(elems)>>{}(elems))), ...);
            },
            t);
        return seed;
    }
};

// Contiguous containers
template <typename T, typename Alloc>
struct GxHash<std::vector<T, Alloc>> {
    std::size_t operator()(const std::vector<T, Alloc>& vec) const noexcept {
        std::size_t seed = 0;
        for (const auto& elem : vec) {
            hash_combine(seed, GxHash<T>{}(elem));
        }
        return seed;
    }
};

template <typename T, std::size_t N>
struct GxHash<std::array<T, N>> {
    std::size_t operator()(const std::array<T, N>& arr) const noexcept {
        std::size_t seed = 0;
        for (const auto& elem : arr) {
            hash_combine(seed, GxHash<T>{}(elem));
        }
        return seed;
    }
};

// Optional and variant
template <typename T>
struct GxHash<std::optional<T>> {
    std::size_t operator()(const std::optional<T>& o) const noexcept {
        if (o) return GxHash<T>{}(*o);
        return 0;
    }
};

template <typename... Types>
struct GxHash<std::variant<Types...>> {
    std::size_t operator()(const std::variant<Types...>& v) const noexcept {
        return std::visit(
            [](auto&& value) { return GxHash<std::decay_t<decltype(value)>>{}(value); }, v);
    }
};

// Other STL types
template <>
struct GxHash<std::thread::id> {
    std::size_t operator()(const std::thread::id& id) const noexcept {
        return std::hash<std::thread::id>{}(id);
    }
};

template <size_t N>
struct GxHash<std::bitset<N>> {
    std::size_t operator()(const std::bitset<N>& bs) const noexcept {
        // Convert bitset to string of '0'/'1' and hash
        std::string s = bs.to_string();
        return GxHash<std::string>{}(s);
    }
};

}  // namespace gxhash
