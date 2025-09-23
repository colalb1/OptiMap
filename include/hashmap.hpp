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
    // 16 control bytes = size of a SIMD register. Allows
    // efficient, parallel operations on multiple slots simultaneously
    static constexpr size_t kGroupWidth = 16;

    // Aligns the hash map's internal storage to cache line boundary. Prevents
    // single group of control bytes from splitting across two cache lines, significantly degrading
    // performance
    static constexpr size_t kCacheLineSize = 64;

    // Control bytes are used to mark the state of each slot in the map.
    // Negative values indicate special empty or deleted states.
    // Positive values (0-127) store the h2 hash (top 7 bits of full hash)
    static constexpr int8_t kEmpty = -128;  // 0b10000000
    static constexpr int8_t kDeleted = -2;  // 0b11111110

    struct FindResult {
        size_t index;
        bool found;
    };

#if defined(__SSE2__) || (defined(_M_X64) || defined(_M_IX86))
    // A wrapper around a SIMD bitmask. Provides iterator-like interface
    // for efficiently finding the set bits, corresponding to matching slots
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

        // Returns the index of the lowest set bit
        // Assumes the mask is not empty
        static int ctzll(uint64_t n) {
#if defined(_MSC_VER) && defined(_M_X64)
            unsigned long i;
            _BitScanForward64(&i, n);
            return i;
#else
            return __builtin_ctzll(n);
#endif
        }
    };

    // Represents a group of 16 control bytes loaded into a SIMD register.
    // This allows for parallel matching of h2 hashes and special states.
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

        static int ctzll(uint64_t n) {
#if defined(_MSC_VER) && defined(_M_X64)
            unsigned long i;
            _BitScanForward64(&i, n);
            return i;
#else
            return __builtin_ctzll(n);
#endif
        }
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

    // Core lookup function. SIMD-accelerated linear probing used to find
    // correct slot for a key. Takes pre-computed hash to avoid
    // redundant calculations
    FindResult find_impl(const Key& key, size_t full_hash) const {
        if (capacity() == 0) {
            return {0, false};
        }

        const int8_t hash2_val = h2(full_hash);
        size_t probe_start_index = h1(full_hash);
        std::optional<size_t> first_deleted_slot;

        for (size_t offset = 0;; offset += kGroupWidth) {
            const size_t group_start_index = (probe_start_index + offset) & (capacity() - 1);
            Group group(&m_ctrl[group_start_index]);

            // Combine match operations for efficiency
            auto match_h2_mask = group.match_h2(hash2_val);
            auto match_empty_mask = group.match_empty();

            for (; match_h2_mask; match_h2_mask.advance()) {
                const size_t index = (group_start_index + match_h2_mask.next()) & (capacity() - 1);
                if (m_buckets[index].first == key) [[likely]] {
                    return {index, true};
                }
            }

            if (match_empty_mask) {
                const size_t empty_index =
                    (group_start_index + match_empty_mask.next()) & (capacity() - 1);
                return {first_deleted_slot.value_or(empty_index), false};
            }

            if (!first_deleted_slot) {
                auto match_deleted_mask = group.match_empty_or_deleted();
                if (match_deleted_mask) {
                    const size_t index =
                        (group_start_index + match_deleted_mask.next()) & (capacity() - 1);
                    if (m_ctrl[index] == kDeleted) {
                        first_deleted_slot = index;
                    }
                }
            }
        }
    }

    static constexpr size_t next_power_of_2(size_t n) {
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
        size_t new_capacity = (m_capacity == 0) ? kGroupWidth : m_capacity * 2;

        int8_t* old_ctrl = m_ctrl;
        Entry* old_buckets = m_buckets;
        size_t old_capacity = m_capacity;

        allocate_and_initialize(new_capacity);

        for (size_t i = 0; i < old_capacity; ++i) {
            if (old_ctrl[i] >= 0) {
                const auto& key = old_buckets[i].first;
                const size_t full_hash = Hash{}(key);
                size_t probe_start_index = h1(full_hash);

                for (size_t offset = 0;; offset += kGroupWidth) {
                    const size_t group_start_index =
                        (probe_start_index + offset) & (m_capacity - 1);
                    Group group(&m_ctrl[group_start_index]);

                    if (auto empty_mask = group.match_empty()) {
                        const size_t empty_index =
                            (group_start_index + empty_mask.next()) & (m_capacity - 1);
                        const int8_t hash2_val = h2(full_hash);

                        new (&m_buckets[empty_index]) Entry(std::move(old_buckets[i]));
                        m_ctrl[empty_index] = hash2_val;

                        size_t sentinel_index = empty_index + m_capacity;
                        if (sentinel_index < m_capacity + kGroupWidth) {
                            m_ctrl[sentinel_index] = hash2_val;
                        }

                        const size_t group_index = empty_index / kGroupWidth;
                        m_group_mask[group_index / 64] |= (UINT64_C(1) << (group_index % 64));
                        break;
                    }
                }
            }
        }

        if (old_ctrl) {
            AlignedAllocator<char, kCacheLineSize>().deallocate(reinterpret_cast<char*>(old_ctrl),
                                                                0);
        }
    }

    inline size_t h1(size_t hash) const { return hash & (capacity() - 1); }

    static inline int8_t h2(size_t hash) {
        return static_cast<int8_t>(hash >> (sizeof(size_t) * 8 - 7));
    }

    // Hash map data is stored in a single contiguous memory block to
    // improve cache locality and reduce allocation overhead. The block is
    // partitioned into three sections:
    //
    // m_ctrl: array of control bytes
    // m_buckets: array of key-value pairs (Entry)
    // m_group_mask: bitmask used to quickly skip over empty groups during iteration
    int8_t* m_ctrl = nullptr;
    Entry* m_buckets = nullptr;
    uint64_t* m_group_mask = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;

    void allocate_and_initialize(size_t new_capacity) {
        if (new_capacity == 0) {
            return;
        }

        size_t ctrl_bytes = new_capacity + kGroupWidth;
        size_t buckets_bytes = new_capacity * sizeof(Entry);
        size_t group_mask_bytes = (new_capacity / kGroupWidth + 63) / 64 * sizeof(uint64_t);

        size_t total_bytes = ctrl_bytes + buckets_bytes + group_mask_bytes;

        void* allocation = AlignedAllocator<char, kCacheLineSize>().allocate(total_bytes);

        m_ctrl = static_cast<int8_t*>(allocation);
        m_buckets = reinterpret_cast<Entry*>(static_cast<char*>(allocation) + ctrl_bytes);
        m_group_mask = reinterpret_cast<uint64_t*>(static_cast<char*>(allocation) + ctrl_bytes +
                                                   buckets_bytes);

        std::fill(m_ctrl, m_ctrl + ctrl_bytes, kEmpty);
        std::fill(m_group_mask, m_group_mask + (new_capacity / kGroupWidth + 63) / 64, 0);

        m_capacity = new_capacity;
    }

    void destroy_and_deallocate() {
        if (m_ctrl) {
            for (size_t i = 0; i < m_capacity; ++i) {
                if (m_ctrl[i] >= 0) {
                    m_buckets[i].~Entry();
                }
            }
            AlignedAllocator<char, kCacheLineSize>().deallocate(reinterpret_cast<char*>(m_ctrl), 0);
            m_ctrl = nullptr;
            m_buckets = nullptr;
            m_group_mask = nullptr;
            m_capacity = 0;
            m_size = 0;
        }
    }

   public:
    explicit HashMap(size_t capacity = 0) {
        if (capacity > 0) {
            size_t initial_capacity =
                next_power_of_2(capacity < kGroupWidth ? kGroupWidth : capacity);
            allocate_and_initialize(initial_capacity);
        }
    }

    ~HashMap() { destroy_and_deallocate(); }

    // Copy/move constructors and assignment operators
    HashMap(const HashMap& other) {
        if (other.m_capacity > 0) {
            allocate_and_initialize(other.m_capacity);
            m_size = other.m_size;
            std::copy(other.m_ctrl, other.m_ctrl + other.m_capacity + kGroupWidth, m_ctrl);
            std::copy(other.m_group_mask,
                      other.m_group_mask + (other.m_capacity / kGroupWidth + 63) / 64,
                      m_group_mask);
            for (size_t i = 0; i < other.m_capacity; ++i) {
                if (other.m_ctrl[i] >= 0) {
                    new (&m_buckets[i]) Entry(other.m_buckets[i]);
                }
            }
        }
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            destroy_and_deallocate();
            if (other.m_capacity > 0) {
                allocate_and_initialize(other.m_capacity);
                m_size = other.m_size;
                std::copy(other.m_ctrl, other.m_ctrl + other.m_capacity + kGroupWidth, m_ctrl);
                std::copy(other.m_group_mask,
                          other.m_group_mask + (other.m_capacity / kGroupWidth + 63) / 64,
                          m_group_mask);
                for (size_t i = 0; i < other.m_capacity; ++i) {
                    if (other.m_ctrl[i] >= 0) {
                        new (&m_buckets[i]) Entry(other.m_buckets[i]);
                    }
                }
            }
        }
        return *this;
    }

    HashMap(HashMap&& other) noexcept
        : m_ctrl(other.m_ctrl),
          m_buckets(other.m_buckets),
          m_group_mask(other.m_group_mask),
          m_size(other.m_size),
          m_capacity(other.m_capacity) {
        other.m_ctrl = nullptr;
        other.m_buckets = nullptr;
        other.m_group_mask = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            destroy_and_deallocate();
            m_ctrl = other.m_ctrl;
            m_buckets = other.m_buckets;
            m_group_mask = other.m_group_mask;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_ctrl = nullptr;
            other.m_buckets = nullptr;
            other.m_group_mask = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    template <typename K, typename V>
    bool emplace(K&& key, V&& value) {
        if (capacity() == 0 || m_size >= capacity() * 0.875) [[unlikely]] {
            resize_and_rehash();
        }

        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) {
            return false;  // Duplicate key
        }

        // Insert into the primary table
        const int8_t hash2_val = h2(full_hash);

        // Create a new Entry with the key and value
        m_buckets[result.index] = {std::forward<K>(key), std::forward<V>(value)};

        // Update control bytes
        m_ctrl[result.index] = hash2_val;

        // Update sentinel if within bounds
        size_t sentinel_index = result.index + capacity();
        if (sentinel_index < capacity() + kGroupWidth) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        const size_t group_index = result.index / kGroupWidth;
        m_group_mask[group_index / 64] |= (UINT64_C(1) << (group_index % 64));

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
        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) {
            return iterator(this, result.index);
        }

        return end();
    }

    const_iterator find(const Key& key) const {
        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

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
        // Need to return a non-const iterator. Just return end() for now. Better implementation
        // would find the next element and return an iterator to it. I'm dumb and lazy so that's not
        // getting fixed right now. Hopefully I remember to come back to this.
        return end();
    }

    bool erase(const Key& key) {
        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) [[likely]] {
            m_ctrl[result.index] = kDeleted;

            // Update sentinel if within bounds
            size_t sentinel_index = result.index + capacity();

            if (sentinel_index < capacity() + kGroupWidth) {
                m_ctrl[sentinel_index] = kDeleted;
            }

            m_size--;

            // Check if the group is now empty and clear the bit if so
            const size_t group_index = result.index / kGroupWidth;
            const size_t group_start_index = group_index * kGroupWidth;

            Group group(&m_ctrl[group_start_index]);

            if ((~group.match_empty_or_deleted().mask & 0xFFFF) == 0) {
                m_group_mask[group_index / 64] &= ~(UINT64_C(1) << (group_index % 64));
            }

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
    size_t capacity() const { return m_capacity; }

    void clear() {
        destroy_and_deallocate();
        allocate_and_initialize(m_capacity);
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
        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        throw std::out_of_range("Key not found in HashMap");
    }

    const Value& at(const Key& key) const {
        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        throw std::out_of_range("Key not found in HashMap");
    }

    Value& operator[](const Key& key) {
        // Reuse find_impl to get the correct index for insertion or retrieval
        if (capacity() == 0 || m_size >= capacity() * 0.875) [[unlikely]] {
            resize_and_rehash();
        }

        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);

        if (result.found) {
            return m_buckets[result.index].second;
        }

        // Key not found, insert new element at returned slot
        const int8_t hash2_val = h2(full_hash);

        m_buckets[result.index] = {key, Value{}};
        m_ctrl[result.index] = hash2_val;

        // Update sentinel
        size_t sentinel_index = result.index + capacity();

        if (sentinel_index < capacity() + kGroupWidth) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        const size_t group_index = result.index / kGroupWidth;
        m_group_mask[group_index / 64] |= (UINT64_C(1) << (group_index % 64));

        m_size++;
        return m_buckets[result.index].second;
    }

    Value& operator[](Key&& key) {
        // Reuse find_impl to get the correct index for insertion or retrieval
        if (capacity() == 0 || m_size >= capacity() * 0.875) [[unlikely]] {
            resize_and_rehash();
        }

        const size_t full_hash = Hash{}(key);
        const auto result = find_impl(key, full_hash);
        if (result.found) {
            return m_buckets[result.index].second;
        }

        // Key not found, insert a new element at the returned slot
        const int8_t hash2_val = h2(full_hash);

        m_buckets[result.index] = {std::move(key), Value{}};
        m_ctrl[result.index] = hash2_val;

        // Update sentinel
        size_t sentinel_index = result.index + capacity();
        if (sentinel_index < capacity() + kGroupWidth) {
            m_ctrl[sentinel_index] = hash2_val;
        }

        const size_t group_index = result.index / kGroupWidth;
        m_group_mask[group_index / 64] |= (UINT64_C(1) << (group_index % 64));

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
            if (m_index >= m_map->capacity()) {
                return;
            }

            size_t group_index = m_index / kGroupWidth;
            const size_t capacity_in_groups = m_map->capacity() / kGroupWidth;

            // Check current group from m_index onwards
            Group group(&m_map->m_ctrl[group_index * kGroupWidth]);
            // Get mask of occupied slots
            uint32_t occupied_mask = ~group.match_empty_or_deleted().mask;
            // Mask out bits before current index
            occupied_mask &= (0xFFFFFFFF << (m_index % kGroupWidth));

            if (occupied_mask) {
                m_index = group_index * kGroupWidth + BitMask(occupied_mask).next();
                return;
            }

            // Search subsequent groups using group mask
            group_index++;
            
            if (group_index >= capacity_in_groups) {
                m_index = m_map->capacity();
                return;
            }

            size_t mask_word_index = group_index / 64;

            // Process first (potentially partial) mask word
            uint64_t mask_word = m_map->m_group_mask[mask_word_index];
            mask_word &= (~UINT64_C(0)) << (group_index % 64);

            while (true) {
                if (mask_word != 0) {
                    // Found a non-empty group in the current word
                    group_index = mask_word_index * 64 + BitMask::ctzll(mask_word);
                    Group first_group(&m_map->m_ctrl[group_index * kGroupWidth]);

                    occupied_mask = ~first_group.match_empty_or_deleted().mask;
                    m_index = group_index * kGroupWidth + BitMask(occupied_mask).next();

                    return;
                }

                // Move to next mask word
                mask_word_index++;
                const size_t num_mask_words = (capacity_in_groups + 63) / 64;
                
                if (mask_word_index >= num_mask_words) {
                    // No more groups
                    m_index = m_map->capacity();

                    return;
                }
                mask_word = m_map->m_group_mask[mask_word_index];
            }
        }

        map_ptr m_map;   // Pointer to map being iterated
        size_t m_index;  // Current index in primary bucket vector
    };
};

}  // namespace optimap
