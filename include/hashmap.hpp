#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "gxhash.hpp"

#if defined(__SSE2__) || (defined(_M_X64) || defined(_M_IX86))
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

template <typename T, size_t Alignment>
struct AlignedAllocator {
    using value_type = T;
    static constexpr size_t alignment = Alignment;

    template <class U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() = default;

    template <class U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    // Equality operators
    bool operator==(const AlignedAllocator&) const noexcept { return true; }
    bool operator!=(const AlignedAllocator&) const noexcept { return false; }

    T* allocate(size_t n) {
        if (n > std::numeric_limits<size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        size_t bytes = n * sizeof(T);

#if defined(_MSC_VER)
        void* ptr = _aligned_malloc(bytes, alignment);
        if (!ptr) {
            throw std::bad_alloc();
        }
#else
        // std::aligned_alloc requires size to be a multiple of alignment
        // Round up to nearest multiple of alignment
        size_t aligned_bytes = ((bytes + alignment - 1) / alignment) * alignment;
        void* ptr = std::aligned_alloc(alignment, aligned_bytes);
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

using namespace gxhash;

template <typename Key, typename Value, typename Hash = GxHash<Key>>
class HashMap {
   public:
    struct Entry {
        Key first;
        Value second;

        bool operator==(const Entry& other) const {
            return first == other.first && second == other.second;
        }
    };

   private:
    // A group of 16 control bytes, the size of one SSE register.
    static constexpr size_t kGroupWidth = 16;

    // Defines the CPU cache line size (64 bytes) for memory alignment
    // Improves performance by preventing cache splits
    static constexpr size_t kCacheLineSize = 64;

    // Control bytes mark the state of a slot
    // Negative values are special states; positive values (0-127) are h2 hashes
    static constexpr int8_t kEmpty = -128;  // 0b10000000
    static constexpr int8_t kDeleted = -2;  // 0b11111110

    struct FindResult {
        size_t index;
        bool found;
    };

#if defined(__SSE2__) || (defined(_M_X64) || defined(_M_IX86))
    // Helper for iterating over matches from a SIMD bitmask.
    struct BitMask {
        uint32_t mask;

        explicit BitMask(uint32_t m) : mask(m) {}

        // Returns true if there are any bits set
        explicit operator bool() const { return mask != 0; }

        // Returns the index of the lowest set bit
        // Assumes the mask is not empty
        int next() {
#if defined(_MSC_VER)
            unsigned long i;
            _BitScanForward(&i, mask);
            return i;
#else
            return __builtin_ctz(mask);
#endif
        }

        // Advances to the next bit, clearing the one that was just processed
        void advance() { mask &= (mask - 1); }
    };

    // Represents control group bytes loaded into a SIMD register
    struct Group {
        __m128i ctrl;

        explicit Group(const int8_t* p) {
            // Use an unaligned load
            ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
        }

        // Returns a bitmask of slots that match the given H2 hash
        BitMask match_h2(int8_t hash) const {
            return BitMask(_mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, _mm_set1_epi8(hash))));
        }

        // Returns a bitmask of slots that are empty
        // The termination condition for a probe sequence
        BitMask match_empty() const {
            return BitMask(_mm_movemask_epi8(_mm_cmpeq_epi8(ctrl, _mm_set1_epi8(kEmpty))));
        }

        // Returns a bitmask of slots that are empty OR deleted
        // This is used to find a suitable slot for insertion
        BitMask match_empty_or_deleted() const {
            // Any control byte with the MSB set is either empty or deleted
            return BitMask(_mm_movemask_epi8(ctrl));
        }
    };
#else
    // Helper for iterating over matches without SIMD.
    struct BitMask {
        uint32_t mask;

        explicit BitMask(uint32_t m) : mask(m) {}

        explicit operator bool() const { return mask != 0; }

        int next() { return __builtin_ctz(mask); }

        void advance() { mask &= (mask - 1); }
    };

    // Fallback implementation of Group for non-SSE2 builds.
    struct Group {
        const int8_t* ctrl;

        explicit Group(const int8_t* p) : ctrl(p) {}

        BitMask match_h2(int8_t hash) const {
            uint32_t mask = 0;
            for (int i = 0; i < kGroupWidth; ++i) {
                if (ctrl[i] == hash) {
                    mask |= (1 << i);
                }
            }
            return BitMask(mask);
        }

        BitMask match_empty() const {
            uint32_t mask = 0;
            for (int i = 0; i < kGroupWidth; ++i) {
                if (ctrl[i] == kEmpty) {
                    mask |= (1 << i);
                }
            }
            return BitMask(mask);
        }

        BitMask match_empty_or_deleted() const {
            uint32_t mask = 0;
            for (int i = 0; i < kGroupWidth; ++i) {
                if (ctrl[i] < 0) {
                    mask |= (1 << i);
                }
            }
            return BitMask(mask);
        }
    };
#endif

    FindResult find_slot(const Key& key) const {
        if (capacity() == 0) {
            return {0, false};
        }
        
        const size_t full_hash = Hash{}(key);
        const int8_t hash2_val = h2(full_hash);

        size_t probe_start_index = h1(full_hash);

        // Will hold the index of the first deleted slot
        std::optional<size_t> first_deleted_slot;

        for (size_t offset = 0;; offset += kGroupWidth) {
            // The index calculation happens once per group
            const size_t group_start_index = (probe_start_index + offset) & (capacity() - 1);

#if defined(__SSE2__) || (defined(_M_X64) || defined(_M_IX86))
            Group group(&m_ctrl[group_start_index]);

            // Check for a direct match
            for (auto mask = group.match_h2(hash2_val); mask.has_next();) {
                const size_t index = (group_start_index + mask.next()) & (capacity() - 1);
                // The h2 hash matched, now we do the expensive full key comparison.
                if (m_buckets[index].first == key) {
                    return {index, true};
                }
            }

            // Check for an empty slot to terminate the search
            // Empty slot = the key is definitely not in the map
            auto empty_mask = group.match_empty();
            if (empty_mask.has_next()) {
                const size_t empty_index =
                    (group_start_index + empty_mask.next()) & (capacity() - 1);
                // Return the deleted slot if we found one,
                // otherwise return this empty slot. This is the insertion point
                return {first_deleted_slot.value_or(empty_index), false};
            }

            // Look for a deleted slot to remember
            // This avoids repeatedly searching for deleted slots in every group
            if (!first_deleted_slot.has_value()) {
                auto deleted_mask = group.match_empty_or_deleted();
                if (deleted_mask.has_next()) {
                    const size_t index =
                        (group_start_index + deleted_mask.next()) & (capacity() - 1);
                    if (m_ctrl[index] == kDeleted) {
                        first_deleted_slot = index;
                    }
                }
            }
#else
            // Scalar fallback path (same logic, just one by one)
            for (size_t i = 0; i < kGroupWidth; ++i) {
                const size_t index = (group_start_index + i) & (capacity() - 1);
                const int8_t ctrl_byte = m_ctrl[index];

                if (ctrl_byte == hash2_val && m_buckets[index].first == key) {
                    return {index, true};
                }

                if (ctrl_byte == kEmpty) {
                    return {first_deleted_slot.value_or(index), false};
                }

                if (!first_deleted_slot.has_value() && ctrl_byte == kDeleted) {
                    first_deleted_slot = index;
                }
            }
#endif
        }
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
        try {
            size_t new_capacity = (capacity() == 0) ? kGroupWidth : capacity() * 2;

            auto old_ctrl = std::move(m_ctrl);
            auto old_buckets = std::move(m_buckets);
            auto old_size = m_size;

            // Allocate new storage and initialize control bytes to kEmpty
            m_ctrl.assign(new_capacity * 2, kEmpty);
            m_buckets.resize(new_capacity);
            m_size = 0;  // Size is 0 during re-insertion

            // Iterate through old elements for direct re-insertion
            for (size_t i = 0; i < old_buckets.size(); ++i) {
                // Skip empty or deleted slots
                if (old_ctrl[i] >= 0) {
                    const auto& key = old_buckets[i].first;

                    // Recalculate hash for the new table size
                    const size_t full_hash = Hash{}(key);
                    size_t probe_start_index = h1(full_hash);

                    // Probe for an empty slot linearly
                    for (size_t offset = 0;; offset += kGroupWidth) {
                        const size_t group_start_index =
                            (probe_start_index + offset) & (capacity() - 1);
                        Group group(&m_ctrl[group_start_index]);

                        // Find the first empty slot in the group
                        if (auto empty_mask = group.match_empty()) {
                            const size_t empty_index =
                                (group_start_index + empty_mask.next()) & (capacity() - 1);
                            const int8_t hash2_val = h2(full_hash);

                            // Directly place the element and its control byte
                            m_buckets[empty_index] = std::move(old_buckets[i]);
                            m_ctrl[empty_index] = hash2_val;

                            // Update the sentinel for faster lookups
                            size_t sentinel_index = empty_index + capacity();
                            if (sentinel_index < m_ctrl.size()) {
                                m_ctrl[sentinel_index] = hash2_val;
                            }
                            break;  // Move to next element
                        }
                    }
                }
            }
            // Restore original size
            m_size = old_size;
        } catch (const std::exception& e) {
            throw std::runtime_error("HashMap resize failed: " + std::string(e.what()));
        }
    }

    inline size_t h1(size_t hash) const { return hash & (capacity() - 1); }

    static inline int8_t h2(size_t hash) {
        return static_cast<int8_t>(hash >> (sizeof(size_t) * 8 - 7));
    }

    // Members
    std::vector<int8_t, AlignedAllocator<int8_t, kCacheLineSize>> m_ctrl;
    mutable std::vector<Entry, AlignedAllocator<Entry, kCacheLineSize>> m_buckets;
    size_t m_size;

   public:
    explicit HashMap(size_t capacity = 0) : m_size(0) {
        if (capacity > 0) {
            // Ensure capacity is at least kGroupWidth and a power of 2
            size_t initial_capacity =
                next_power_of_2(capacity < kGroupWidth ? kGroupWidth : capacity);

            try {
                // Allocate control bytes with enough space for sentinels
                m_ctrl.resize(initial_capacity * 2);
                std::fill(m_ctrl.begin(), m_ctrl.end(), kEmpty);

                // Allocate buckets
                m_buckets.resize(initial_capacity);
            } catch (const std::bad_alloc& e) {
                // Handle allocation failure
                throw std::runtime_error("HashMap initialization failed: " + std::string(e.what()));
            }
        }
    }

    // Copy/move constructors and assignment operators
    HashMap(const HashMap& other) = default;
    HashMap& operator=(const HashMap& other) = default;
    HashMap(HashMap&& other) noexcept
        : m_ctrl(std::move(other.m_ctrl)),
          m_buckets(std::move(other.m_buckets)),
          m_size(other.m_size) {
        other.m_size = 0;
    }
    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            m_ctrl = std::move(other.m_ctrl);
            m_buckets = std::move(other.m_buckets);
            m_size = other.m_size;
            other.m_size = 0;
        }
        return *this;
    }

    template <typename K, typename V>
    bool emplace(K&& key, V&& value) {
        if (capacity() == 0 || m_size >= capacity() * 0.875) {
            resize_and_rehash();
        }

        const auto result = find_slot(key);

        if (result.found) {
            return false;  // Duplicate key
        }

        // Insert into the primary table
        const int8_t hash2_val = h2(Hash{}(key));

        // Create a new Entry with the key and value
        m_buckets[result.index] = {std::forward<K>(key), std::forward<V>(value)};

        // Update control bytes
        m_ctrl[result.index] = hash2_val;

        // Update sentinel if within bounds
        size_t sentinel_index = result.index + capacity();
        if (sentinel_index < m_ctrl.size()) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        m_size++;

        return true;
    }

    bool insert(const Key& key, const Value& value) { return emplace(key, value); }

    // Overload for r-values to enable move semantics
    bool insert(Key&& key, Value&& value) { return emplace(std::move(key), std::move(value)); }

    // Forward declaration
    template <bool IsConst>
    class iterator_impl;

    // Aliases
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    class node_type {
       public:
        using key_type = Key;
        using mapped_type = Value;

        node_type() = default;

        explicit node_type(Entry&& entry) : m_entry(std::move(entry)) {}

        bool empty() const noexcept { return !m_entry.has_value(); }
        explicit operator bool() const noexcept { return m_entry.has_value(); }

        key_type& key() { return m_entry->first; }

        mapped_type& mapped() { return m_entry->second; }

       private:
        friend class HashMap;
        std::optional<Entry> m_entry;
    };

    iterator find(const Key& key) {
        const auto result = find_slot(key);

        if (result.found) {
            return iterator(this, result.index);
        }

        return end();
    }

    const_iterator find(const Key& key) const {
        const auto result = find_slot(key);

        if (result.found) {
            return const_iterator(this, result.index);
        }

        return end();
    }

    iterator erase(iterator it) {
        if (it == end()) {
            return end();
        }
        erase(it->first);
        return ++it;
    }

    iterator erase(const_iterator it) {
        if (it == end()) {
            return end();
        }
        erase(it->first);
        // This is tricky, as we need to return a non-const iterator.
        // For now, we'll just return end(). A better implementation
        // would be to find the next element and return an iterator to it.
        return end();
    }

    bool erase(const Key& key) {
        const auto result = find_slot(key);

        if (result.found) {
            m_ctrl[result.index] = kDeleted;

            // Update sentinel if within bounds
            size_t sentinel_index = result.index + capacity();

            if (sentinel_index < m_ctrl.size()) {
                m_ctrl[sentinel_index] = kDeleted;
            }

            m_size--;

            return true;
        }

        return false;
    }

    node_type extract(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            return node_type{};
        }
        return extract(it);
    }

    node_type extract(const_iterator it) {
        if (it == end()) {
            return node_type{};
        }

        node_type node(std::move(const_cast<Entry&>(*it)));
        erase(it);
        return node;
    }

    void insert(node_type&& node) {
        if (!node.empty()) {
            emplace(std::move(node.key()), std::move(node.mapped()));
        }
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_buckets.size(); }

    void clear() {
        // Reset all control bytes to kEmpty
        std::fill(m_ctrl.begin(), m_ctrl.end(), kEmpty);

        // Clear the buckets
        m_buckets.clear();
        m_buckets.resize(capacity());

        m_size = 0;
    }

    iterator begin() {
        iterator it(this, 0);

        // To first valid elt
        it.find_next_valid();
        return it;
    }

    iterator end() { return iterator(this, capacity()); }

    const_iterator begin() const {
        const_iterator it(this, 0);

        // To first valid elt
        it.find_next_valid();
        return it;
    }

    const_iterator end() const { return const_iterator(this, capacity()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    bool contains(const Key& key) const { return find(key) != end(); }

    Value& at(const Key& key) {
        const auto result = find_slot(key);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        throw std::out_of_range("Key not found in HashMap");
    }

    const Value& at(const Key& key) const {
        const auto result = find_slot(key);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        throw std::out_of_range("Key not found in HashMap");
    }

    Value& operator[](const Key& key) {
        // Reuse find_slot to get the correct index for insertion or retrieval
        if (capacity() == 0 || m_size >= capacity() * 0.875) {
            resize_and_rehash();
        }

        const auto result = find_slot(key);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        // Key not found, insert new element at returned slot
        const int8_t hash2_val = h2(Hash{}(key));

        m_buckets[result.index] = {key, Value{}};
        m_ctrl[result.index] = hash2_val;

        // Update sentinel
        size_t sentinel_index = result.index + capacity();

        if (sentinel_index < m_ctrl.size()) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        m_size++;
        return m_buckets[result.index].second;
    }

    Value& operator[](Key&& key) {
        // Reuse find_slot to get the correct index for insertion or retrieval
        if (capacity() == 0 || m_size >= capacity() * 0.875) {
            resize_and_rehash();
        }

        const auto result = find_slot(key);
        if (result.found) {
            return m_buckets[result.index].second;
        }

        // Key not found, insert a new element at the returned slot
        const int8_t hash2_val = h2(Hash{}(key));

        m_buckets[result.index] = {std::move(key), Value{}};
        m_ctrl[result.index] = hash2_val;

        // Update sentinel
        size_t sentinel_index = result.index + capacity();
        if (sentinel_index < m_ctrl.size()) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        m_size++;
        return m_buckets[result.index].second;
    }

    // For mutable and constant iterators
    template <bool IsConst>
    class iterator_impl {
       public:
        // Conditional types based on constant status
        using map_ptr = std::conditional_t<IsConst, const HashMap*, HashMap*>;
        using reference = std::conditional_t<IsConst, const Entry&, Entry&>;
        using pointer = std::conditional_t<IsConst, const Entry*, Entry*>;

        iterator_impl() : m_map(nullptr), m_index(0) {}
        iterator_impl(map_ptr map, size_t index) : m_map(map), m_index(index) {}

        // Copy constructor needed for unique_ptr member
        iterator_impl(const iterator_impl& other) = default;

        // Copy assignment
        iterator_impl& operator=(const iterator_impl& other) = default;

        // Mutable iterator converted to const_iterator
        operator iterator_impl<true>() const { return iterator_impl<true>(m_map, m_index); }

        reference operator*() const { return m_map->m_buckets[m_index]; }

        pointer operator->() const { return &operator*(); }

        iterator_impl& operator++() {
            m_index++;

            // Skip empty & deleted slots
            find_next_valid();
            return *this;
        }

        iterator_impl operator++(int) {
            iterator_impl tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const iterator_impl& a, const iterator_impl& b) {
            return a.m_map == b.m_map && a.m_index == b.m_index;
        }

        friend bool operator!=(const iterator_impl& a, const iterator_impl& b) { return !(a == b); }

       private:
        friend class HashMap;
        void find_next_valid() {
            // Search primary table for next occupied slot where ctrl byte >= 0
            while (m_index < m_map->capacity()) {
                if (m_map->m_ctrl[m_index] >= 0) {
                    return;  // Found a valid slot
                }
                m_index++;
            }
        }

        map_ptr m_map;   // Pointer to map being iterated
        size_t m_index;  // Current index in primary bucket vector
    };
};

}  // namespace optimap
