#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace optimap {

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashMap {
   public:
    explicit HashMap(size_t capacity = 16) : m_size(0) {
        size_t initial_capacity = next_power_of_2(capacity);
        m_ctrl.assign(initial_capacity, kEmpty);
        m_buckets.resize(initial_capacity);
    }

    bool insert(const Key& key, const Value& value) {
        if (m_size >= capacity() * 0.875) {
            resize_and_rehash();
        }

        const size_t full_hash = Hash{}(key);
        size_t index = h1(full_hash);
        const int8_t hash2_val = h2(full_hash);

        std::optional<size_t> insert_slot;

        for (size_t i = 0; i < capacity(); ++i) {
            size_t probe_index = (index + i) & (capacity() - 1);
            int8_t ctrl_byte = m_ctrl[probe_index];

            // Slot is kEmpty or kDeleted
            if (ctrl_byte < 0) {
                if (!insert_slot.has_value()) {
                    insert_slot = probe_index;
                }
                if (ctrl_byte == kEmpty) {
                    // Found end of probe chain, key is not a duplicate
                    break;
                }
            } else if (ctrl_byte == hash2_val && m_buckets[probe_index].key == key) {
                // Duplicate key found
                return false;  
            }
        }

        if (insert_slot.has_value()) {
            m_buckets[*insert_slot] = {key, value};
            m_ctrl[*insert_slot] = hash2_val;
            m_size++;
            return true;
        }

        // Should be unreachable if resizing works
        throw std::runtime_error("HashMap is full and cannot insert.");
    }

    std::optional<Value> find(const Key& key) const {
        const size_t full_hash = Hash{}(key);
        size_t index = h1(full_hash);
        const int8_t hash2_val = h2(full_hash);

        for (size_t i = 0; i < capacity(); ++i) {
            size_t probe_index = (index + i) & (capacity() - 1);
            int8_t ctrl_byte = m_ctrl[probe_index];

            // End of probe chain
            if (ctrl_byte == kEmpty) {
                return std::nullopt;  
            }
            if (ctrl_byte == hash2_val && m_buckets[probe_index].key == key) {
                return m_buckets[probe_index].value;
            }
        }
        return std::nullopt;
    }

    bool erase(const Key& key) {
        const size_t full_hash = Hash{}(key);
        size_t index = h1(full_hash);
        const int8_t hash2_val = h2(full_hash);

        for (size_t i = 0; i < capacity(); ++i) {
            size_t probe_index = (index + i) & (capacity() - 1);
            int8_t ctrl_byte = m_ctrl[probe_index];

            if (ctrl_byte == kEmpty) {
                return false;  // End of probe chain
            }
            if (ctrl_byte == hash2_val && m_buckets[probe_index].key == key) {
                m_ctrl[probe_index] = kDeleted;
                m_size--;
                return true;
            }
        }
        return false;
    }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_ctrl.size(); }

   private:
    // Control bytes mark the state of a slot.
    // Negative values are special states; positive values (0-127) are h2 hashes.
    static constexpr int8_t kEmpty = -128;  // 0b10000000
    static constexpr int8_t kDeleted = -2;  // 0b11111110

    struct Entry {
        Key key;
        Value value;
    };

    // Helper methods

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
        size_t new_cap = (capacity() == 0) ? 16 : capacity() * 2;
        std::vector<int8_t> old_ctrl = std::move(m_ctrl);
        std::vector<Entry> old_buckets = std::move(m_buckets);

        m_ctrl.assign(new_cap, kEmpty);
        m_buckets.resize(new_cap);
        m_size = 0;

        for (size_t i = 0; i < old_ctrl.size(); ++i) {
            if (old_ctrl[i] >= 0) {  // If slot was occupied
                insert(old_buckets[i].key, old_buckets[i].value);
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
};

}  // namespace optimap