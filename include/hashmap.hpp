#pragma once
#include <functional>
#include <optional>
#include <vector>

namespace optimap {

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashMap {
   public:
    HashMap(size_t capacity = 16) : m_buckets(capacity) {}

    bool insert(const Key& key, const Value& value) {
        size_t index = Hash{}(key) % m_buckets.size();
        
        for (size_t i = 0; i < m_buckets.size(); ++i) {
            size_t idx = (index + i) % m_buckets.size();

            if (!m_buckets[idx].occupied) {
                m_buckets[idx] = {key, value, true, 0};
                ++m_size;
                return true;
            }
            if (m_buckets[idx].key == key) {
                return false;  // duplicate
            }
        }
        return false;  // full
    }

    std::optional<Value> find(const Key& key) const {
        size_t index = Hash{}(key) % m_buckets.size();
        for (size_t i = 0; i < m_buckets.size(); ++i) {
            size_t idx = (index + i) % m_buckets.size();
            if (!m_buckets[idx].occupied) return std::nullopt;
            if (m_buckets[idx].key == key) return m_buckets[idx].value;
        }
        return std::nullopt;
    }

    size_t size() const { return m_size; }

   private:
    struct Entry {
        Key key;
        Value value;
        bool occupied = false;
        int8_t distance_from_desired = -1;
    };

    std::vector<Entry> m_buckets;
    size_t m_size = 0;
};

}  // namespace optimap
