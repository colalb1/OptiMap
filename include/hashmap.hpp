#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace optimap {

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashMap {
   public:
    explicit HashMap(size_t capacity = 16) {
        m_buckets.resize(capacity);
        m_ctrl.assign(capacity, kEmpty);
    }

    bool insert(const Key& key, const Value& value) {
        // LOAD FACTOR GOES HERE!!! (.875)

        const size_t full_hash = Hash{}(key);
        const int8_t hash2 = h2(full_hash);
        size_t index = h1(full_hash);

        // Sentinel
        size_t first_tombstone = -1;

        for (size_t i = 0; i < m_buckets.size(); ++i) {
            size_t current_idx = (index + i) & (m_buckets.size() - 1);
            int8_t ctrl_byte = m_ctrl[current_idx];

            // Slot is kEmpty or kDeleted
            if (ctrl_byte < 0) {
                // If this is first available slot, store
                // Prefer insertion into earliest tombstone for short chain
                if (first_tombstone == -1) {
                    first_tombstone = current_idx;
                }
                // If real empty slot, key not in table \implies stop searching for duplicates
                if (ctrl_byte == kEmpty) {
                    break;
                }
            } else if (ctrl_byte == hash2) {
                // The 7-bit-potential-duplicate hash matches
                // Full (expensive) key comparison
                if (m_buckets[current_idx].key == key) {
                    // Duplicate
                    return false;
                }
            }
        }

        // After duplicate check, use available slot if found
        if (first_tombstone != -1) {
            m_buckets[first_tombstone] = {key, value};
            m_ctrl[first_tombstone] = hash2;
            ++m_size;

            return true;
        }

        // Table full
        return false;
    }

    std::optional<Value> find(const Key& key) const {
        const size_t full_hash = Hash{}(key);
        const int8_t hash2 = h2(full_hash);
        size_t index = h1(full_hash);

        for (size_t i = 0; i < m_buckets.size(); ++i) {
            size_t current_idx = (index + i) & (m_buckets.size() - 1);
            int8_t ctrl_byte = m_ctrl[current_idx];

            // Fast path, check control byte
            if (ctrl_byte == kEmpty) {
                // Empty slot breaks the probe chain \implies key not here
                return std::nullopt;
            }
            if (ctrl_byte == hash2) {
                // The 7-bit hash matches \then check ACTUAL key
                if (m_buckets[current_idx].key == key) {
                    return m_buckets[current_idx].value;
                }
            }
            // If ctrl_byte is kDeleted or a mismatched H2, continue probing.
        }

        // Ended probing without match
        return std::nullopt;  
    }

    bool erase(const Key& key) {
        const size_t full_hash = Hash{}(key);
        const int8_t hash2 = h2(full_hash);
        size_t index = h1(full_hash);

        for (size_t i = 0; i < m_buckets.size(); ++i) {
            size_t current_idx = (index + i) & (m_buckets.size() - 1);
            int8_t ctrl_byte = m_ctrl[current_idx];

            if (ctrl_byte == kEmpty) {
                // Empty slot breaks probe chain (no key)
                return false;
            }
            if (ctrl_byte == hash2) {
                // Potential match found \then full key comparison
                if (m_buckets[current_idx].key == key) {
                    // Tombstone
                    m_ctrl[current_idx] = kDeleted;
                    --m_size;

                    // Old data in m_buckets[current_idx] remains & now ignored.
                    return true;
                }
            }
        }

        // Key not found
        return false;  
    }

    size_t size() const { return m_size; }

   private:
    // The Entry struct is now a plain data holder
    // Metadata is separate
    struct Entry {
        Key key;
        Value value;
    };

    // Control bytes hold slot metadata
    // Negative values for special states allows the positive range (0-127) for 7-bit hash (H2) storage.
    static constexpr int8_t kEmpty = -128;  // 0b10000000 \then Slot empty and never used
    static constexpr int8_t kDeleted = -2;  // 0b11111110 \then Tombstone

    // Parallel, small, contiguous array for metadata
    std::vector<int8_t> m_ctrl;
    std::vector<Entry> m_buckets;
    size_t m_size = 0;

    // Helper to get H1 hash, for initial bucket index
    // Assumes capacity = 2^x \implies modulo can be a fast bitwise AND
    inline size_t h1(size_t hash) const { return hash & (m_buckets.size() - 1); }

    // Helper to get H2 hash aka the top 7 bits
    // Stored in the control byte to quickly filter non-matching keys
    static inline int8_t h2(size_t hash) {
        return static_cast<int8_t>(hash >> (sizeof(size_t) * 8 - 7));
    }
};

}  // namespace optimap
