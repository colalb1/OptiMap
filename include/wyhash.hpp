#pragma once

#include <array>
#include <bitset>
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

// This is a C++ implementation of wyhash, based on the original C source
// https://github.com/wangyi-fudan/wyhash

namespace wyhash
{

    // Default secret values used by wyhash
    static const uint64_t _wyp[4] =
            {0xa0761d6478bd642f, 0xe7037ed1a0b428db, 0x8ebc6af09c88c6e3, 0x589965cc75374cc3};

    // Multiplies two 64-bit integers and returns the XOR of the high and low 128-bit parts
    // This is the core mixing operation of wyhash
    __attribute__((always_inline)) static inline uint64_t _wymix(uint64_t A, uint64_t B)
    {
#if defined(__SIZEOF_INT128__)
        __uint128_t r = (__uint128_t)A * B;
        return (uint64_t)r ^ (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
        uint64_t hi;
        uint64_t lo = _umul128(A, B, &hi);
        return lo ^ hi;
#else
        // Slower fallback for other compilers/platforms.
        // Note: This implementation is likely incorrect as it doesn't produce a 128-bit
        // intermediate result.
        uint64_t ha = A >> 32, la = (uint32_t)A;
        uint64_t hb = B >> 32, lb = (uint32_t)B;
        uint64_t rh = ha * hb, rm = ha * lb + la * hb, rl = la * lb;
        return (rm << 32) + rl + rh;
#endif
    }

    // Reads 8 bytes from a pointer
    __attribute__((always_inline)) static inline uint64_t _wyr8(const uint8_t* p)
    {
        uint64_t v;
        memcpy(&v, p, 8);
        return v;
    }

    // Reads 4 bytes from a pointer
    __attribute__((always_inline)) static inline uint64_t _wyr4(const uint8_t* p)
    {
        uint32_t v;
        memcpy(&v, p, 4);
        return v;
    }

    // Main wyhash function
    static inline uint64_t
    wyhash(const void* key, size_t len, uint64_t seed, const uint64_t* secret)
    {
        const uint8_t* p = (const uint8_t*)key;
        seed ^= _wymix(seed ^ secret[0], secret[1]);
        uint64_t a, b;

        if (len <= 16)
        {
            if (len >= 4)
            {
                a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
                b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
            }
            else if (len > 0)
            {
                a = ((uint64_t)p[0] << 16) | ((uint64_t)p[len >> 1] << 8) | p[len - 1];
                b = 0;
            }
            else
            {
                a = b = 0;
            }
        }
        else
        {
            size_t i = len;

            if (i > 48)
            {
                uint64_t see1 = seed, see2 = seed;
                do
                {
                    seed = _wymix(_wyr8(p) ^ secret[1], _wyr8(p + 8) ^ seed);
                    see1 = _wymix(_wyr8(p + 16) ^ secret[2], _wyr8(p + 24) ^ see1);
                    see2 = _wymix(_wyr8(p + 32) ^ secret[3], _wyr8(p + 40) ^ see2);
                    p += 48;
                    i -= 48;
                } while (i > 48);
                seed ^= see1 ^ see2;
            }
            while (i > 16)
            {
                seed = _wymix(_wyr8(p) ^ secret[1], _wyr8(p + 8) ^ seed);
                i -= 16;
                p += 16;
            }
            a = _wyr8(p + i - 16);
            b = _wyr8(p + i - 8);
        }
        return _wymix(secret[1] ^ len, _wymix(a ^ secret[1], b ^ seed));
    }

} // namespace wyhash

template <typename T> struct WyHash;

// Utility function to combine hash values
template <class T> inline void hash_combine(std::size_t& seed, const T& v)
{
    WyHash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// A high-quality 64-bit integer mixer to improve distribution for integral types
// Based on the finalizer from MurmurHash3/splitmix64
struct WyIntegerMixer
{
    size_t operator()(uint64_t x) const noexcept
    {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }
};

// The primary template for WyHash, handling fundamental types with `if constexpr`
template <typename T> struct WyHash
{
    size_t operator()(const T& key) const noexcept
    {
        if constexpr (std::is_integral_v<T>)
        {
            // For integral types, use the integer mixer for better bit distribution
            return WyIntegerMixer{}(static_cast<uint64_t>(key));
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            // For floating-point types, hash their byte representation.
            // Handle -0.0 to hash to the same value as 0.0.
            if (key == 0.0)
            {
                return WyHash<int>{}(0);
            }
            return wyhash::wyhash(&key, sizeof(T), 0, wyhash::_wyp);
        }
        else
        {
            // Default implementation for other types.
            // Note: This is safe for simple, trivially copyable types but may not
            // produce meaningful hashes for complex types with pointers or padding.
            static_assert(
                    std::is_trivially_copyable_v<T>,
                    "Default WyHash requires a trivially copyable type."
            );
            return wyhash::wyhash(&key, sizeof(T), 0, wyhash::_wyp);
        }
    }
};

// Pointers

template <typename T> struct WyHash<T*>
{
    size_t operator()(T* ptr) const noexcept
    {
        return WyIntegerMixer{}(reinterpret_cast<uintptr_t>(ptr));
    }
};

template <> struct WyHash<std::nullptr_t>
{
    size_t operator()(std::nullptr_t) const noexcept
    {
        return 0;
    }
};

template <typename T, typename Deleter> struct WyHash<std::unique_ptr<T, Deleter>>
{
    size_t operator()(const std::unique_ptr<T, Deleter>& ptr) const noexcept
    {
        return WyHash<T*>{}(ptr.get());
    }
};

template <typename T> struct WyHash<std::shared_ptr<T>>
{
    size_t operator()(const std::shared_ptr<T>& ptr) const noexcept
    {
        return WyHash<T*>{}(ptr.get());
    }
};

// Strings

template <> struct WyHash<std::string>
{
    size_t operator()(const std::string& key) const noexcept
    {
        return wyhash::wyhash(key.data(), key.size(), 0, wyhash::_wyp);
    }
};

template <> struct WyHash<std::string_view>
{
    size_t operator()(const std::string_view& key) const noexcept
    {
        return wyhash::wyhash(key.data(), key.size(), 0, wyhash::_wyp);
    }
};

template <> struct WyHash<const char*>
{
    size_t operator()(const char* key) const noexcept
    {
        return wyhash::wyhash(key, std::strlen(key), 0, wyhash::_wyp);
    }
};

// Container and wrappers

template <typename T1, typename T2> struct WyHash<std::pair<T1, T2>>
{
    size_t operator()(const std::pair<T1, T2>& p) const noexcept
    {
        size_t seed = 0;
        hash_combine(seed, p.first);
        hash_combine(seed, p.second);
        return seed;
    }
};

template <typename... Types> struct WyHash<std::tuple<Types...>>
{
    size_t operator()(const std::tuple<Types...>& t) const noexcept
    {
        size_t seed = 0;
        std::apply(
                [&](const auto&... args) {
                    (hash_combine(seed, args), ...);
                },
                t
        );
        return seed;
    }
};

template <typename T, typename Alloc> struct WyHash<std::vector<T, Alloc>>
{
    size_t operator()(const std::vector<T, Alloc>& vec) const noexcept
    {
        size_t seed = 0;
        for (const auto& elem : vec)
        {
            hash_combine(seed, elem);
        }
        return seed;
    }
};

template <typename T, size_t N> struct WyHash<std::array<T, N>>
{
    size_t operator()(const std::array<T, N>& arr) const noexcept
    {
        size_t seed = 0;
        for (const auto& elem : arr)
        {
            hash_combine(seed, elem);
        }
        return seed;
    }
};

template <typename T> struct WyHash<std::optional<T>>
{
    size_t operator()(const std::optional<T>& opt) const noexcept
    {
        if (opt)
        {
            return WyHash<T>{}(*opt);
        }
        return 0;
    }
};

template <typename... Types> struct WyHash<std::variant<Types...>>
{
    size_t operator()(const std::variant<Types...>& v) const noexcept
    {
        return std::visit(
                [](const auto& value) {
                    return WyHash<std::decay_t<decltype(value)>>{}(value);
                },
                v
        );
    }
};

// Other STL types

template <> struct WyHash<std::thread::id>
{
    size_t operator()(const std::thread::id& id) const noexcept
    {
        return std::hash<std::thread::id>{}(id);
    }
};

template <size_t N> struct WyHash<std::bitset<N>>
{
    size_t operator()(const std::bitset<N>& bs) const noexcept
    {
        const std::string s = bs.to_string();
        return WyHash<std::string>{}(s);
    }
};
