#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

// SSE/AVX intrinsics for SIMD
#include <immintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

template <typename T, size_t Alignment>
struct AlignedAllocator {
    using value_type = T;
    static constexpr size_t alignment = Alignment;

    AlignedAllocator() = default;

    template <class U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    T* allocate(size_t n) {
        if (n > std::numeric_limits<size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }
#if defined(_MSC_VER)
        void* ptr = _aligned_malloc(n * sizeof(T), alignment);
        if (!ptr) {
            throw std::bad_alloc();
        }
#else
        void* ptr = std::aligned_alloc(alignment, n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
#endif
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, size_t) noexcept {
#if defined(_MSC_VER)
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
};

namespace optimap {

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashMap {
   private:
    // A group of 16 control bytes, the size of one SSE register.
    static constexpr size_t kGroupWidth = 16;

    // Helper for iterating over matches from a SIMD bitmask.
    struct BitMask {
        uint32_t mask;
        explicit BitMask(uint32_t m) : mask(m) {}

        bool has_next() const { return mask != 0; }

        int next() {
#if defined(_MSC_VER)
            unsigned long i;
            _BitScanForward(&i, mask);
#else
            int i = __builtin_ctz(mask);
#endif
            // Clear lowest set bit
            mask &= (mask - 1);
            return i;
        }
    };

    // Represents control group bytes loaded into a SIMD register
    struct Group {
        __m128i ctrl;

        explicit Group(const int8_t* p) {
            ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
        }

        // Returns a bitmask of slots matching h2
        BitMask match_h2(int8_t hash) const {
            return BitMask(_mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, _mm_set1_epi8(hash))));
        }

        // Returns a bitmask of slots that are empty or deleted
        BitMask match_empty_or_deleted() const { return BitMask(_mm_movemask_epi8(ctrl)); }
    };

   public:
    explicit HashMap(size_t capacity = 16) : m_size(0) {
        size_t initial_capacity = next_power_of_2(capacity < kGroupWidth ? kGroupWidth : capacity);
        m_ctrl.assign(initial_capacity + kGroupWidth - 1, kEmpty);
        m_buckets.resize(initial_capacity);
    }

    bool insert(const Key& key, const Value& value) {
        if (m_size >= capacity() * 0.875) {
            resize_and_rehash();
        }

        const auto result = find_slot(key);

        if (result.found) {
            return false;  // Duplicate key
        }

        // If probe sequence too long, move to overflow table
        if (result.probe_distance > kGroupWidth) {
            if (!m_overflow) {
                m_overflow = std::make_unique<HashMap>();
            }
            // Mark end of the primary probe chain with overflow marker
            m_ctrl[result.index] = kOverflow;
            // Update sentinel
            m_ctrl[result.index + capacity()] = kOverflow;

            return m_overflow->insert(std::move(key), std::move(value));
        }

        // Else, insert into the primary table (per usual)
        const int8_t hash2_val = h2(Hash{}(key));

        m_buckets[result.index] = {std::move(key), std::move(value)};
        m_ctrl[result.index] = hash2_val;
        m_ctrl[result.index + capacity()] = hash2_val;  // Update sentinel
        m_size++;

        return true;
    }

    std::optional<Value> find(const Key& key) const {
        const auto result = find_slot(key);

        if (result.found) {
            return m_buckets[result.index].value;
        }

        // If probe ends at overflow marker, search overflow table
        if (m_ctrl[result.index] == kOverflow && m_overflow) {
            return m_overflow->find(key);
        }

        return std::nullopt;
    }

    bool erase(const Key& key) {
        // First try erasing from overflow table
        if (m_overflow && m_overflow->erase(key)) {
            return true;
        }

        const auto result = find_slot(key);

        if (!result.found) {
            return false;
        }

        m_ctrl[result.index] = kDeleted;
        m_ctrl[result.index + capacity()] = kDeleted;  // Update sentinel
        m_size--;

        return true;
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_buckets.size(); }

    struct Entry {
        Key key;
        Value value;
    };

    template <bool IsConst>
    class iterator_impl {
        // implement
    };

    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    iterator begin() {
        // implement
    }

    iterator end() {
        // implement
    }

    const_iterator begin() const {
        // implement
    }

    const_iterator end() const {
        // implement
    }

   private:
    // Control bytes mark the state of a slot
    // Negative values are special states; positive values (0-127) are h2 hashes
    static constexpr int8_t kEmpty = -128;   // 0b10000000
    static constexpr int8_t kDeleted = -2;   // 0b11111110
    static constexpr int8_t kOverflow = -3;  // 0b11111101

    struct FindResult {
        size_t index;
        bool found;
        size_t probe_distance;
    };

    FindResult find_slot(const Key& key) const {
#if defined(__SSE2__) || (defined(_M_X64) || defined(_M_IX86))
        // SIMD fast path
        const size_t full_hash = Hash{}(key);
        const int8_t hash2_val = h2(full_hash);

        size_t probe_start_index = h1(full_hash);

        std::optional<size_t> first_deleted_slot;

        for (size_t offset = 0;; offset += kGroupWidth) {
            const size_t current_index = (probe_start_index + offset) & (capacity() - 1);
            Group group(&m_ctrl[current_index]);

            // Check for potential h2 matches in the group
            for (auto mask = group.match_h2(hash2_val); mask.has_next();) {
                const size_t i = (current_index + mask.next()) & (capacity() - 1);

                if (m_buckets[i].key == key) {
                    return {i, true, offset};
                }
            }

            // Check for an empty slot to terminate the probe
            auto empty_mask = group.match_empty_or_deleted();

            // Not all slots are full
            if (empty_mask.mask != 0xFFFF) {
                for (auto mask = empty_mask; mask.has_next();) {
                    size_t bit_pos = mask.next();
                    const size_t i = (current_index + bit_pos) & (capacity() - 1);

                    if (m_ctrl[i] == kEmpty || m_ctrl[i] == kOverflow) {
                        return {first_deleted_slot.value_or(i), false, offset + bit_pos};
                    }
                    if (!first_deleted_slot.has_value() && m_ctrl[i] == kDeleted) {
                        first_deleted_slot = i;
                    }
                }
                return {first_deleted_slot.value(), false, offset};
            }
        }
#else
        // Scalar fallback path
        const size_t full_hash = Hash{}(key);
        const int8_t hash2_val = h2(full_hash);
        size_t probe_start_index = h1(full_hash);

        std::optional<size_t> first_deleted_slot;

        for (size_t offset = 0;; ++offset) {
            const size_t index = (probe_start_index + offset) & (capacity() - 1);
            const int8_t ctrl_byte = m_ctrl[index];

            if (ctrl_byte == hash2_val && m_buckets[index].key == key) {
                return {index, true, offset};
            }
            if (ctrl_byte == kEmpty || ctrl_byte == kOverflow) {
                return {first_deleted_slot.value_or(index), false, offset};
            }
            if (!first_deleted_slot.has_value() && ctrl_byte == kDeleted) {
                first_deleted_slot = index;
            }
        }
#endif
    }

    static size_t next_power_of_2(size_t n) {
        if (n == 0) {
            return 1;
        }

        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        n++;

        return n;
    }

    void resize_and_rehash() {
        size_t new_capacity = (capacity() == 0) ? kGroupWidth : capacity() * 2;

        // Custom allocator
        using CtrlVec = std::vector<int8_t, AlignedAllocator<int8_t, kCacheLineSize>>;
        using BucketVec = std::vector<Entry, AlignedAllocator<Entry, kCacheLineSize>>;

        // Temporarily store old data
        CtrlVec old_ctrl = std::move(m_ctrl);
        BucketVec old_buckets = std::move(m_buckets);
        std::unique_ptr<HashMap> old_overflow = std::move(m_overflow);

        // Reset and resize this map
        m_buckets.assign(new_capacity, {});
        m_ctrl.assign(new_capacity + kGroupWidth - 1, kEmpty);
        m_size = 0;
        m_overflow = nullptr;  // Clear overflow

        // Re-insert elements from the old primary table
        for (size_t i = 0; i < old_buckets.size(); ++i) {
            // If slot was occupied
            if (old_ctrl[i] >= 0) {
                insert(std::move(old_buckets[i].key), std::move(old_buckets[i].value));
            }
        }

        // Re-insert elements from the old overflow table
        if (old_overflow) {
            for (size_t i = 0; i < old_overflow->capacity(); ++i) {
                if (old_overflow->m_ctrl[i] >= 0) {
                    insert(std::move(old_overflow->m_buckets[i].key),
                           std::move(old_overflow->m_buckets[i].value));
                }
            }
        }
    }

    inline size_t h1(size_t hash) const { return hash & (capacity() - 1); }

    static inline int8_t h2(size_t hash) {
        return static_cast<int8_t>(hash >> (sizeof(size_t) * 8 - 7));
    }

    // Members
    std::vector<int8_t> m_ctrl;
    std::vector<Entry> m_buckets;
    size_t m_size;
    std::unique_ptr<HashMap> m_overflow;
};

}  // namespace optimap