#include <gtest/gtest.h>

#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "hashmap.hpp"

// Basic functionality tests
TEST(HashMapTest, InsertAndFind) {
    optimap::HashMap<int, std::string> map;
    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_TRUE(map.insert(2, "two"));
    EXPECT_EQ(map.size(), 2);

    auto val1 = map.find(1);
    ASSERT_NE(val1, map.end());
    EXPECT_EQ(val1->second, "one");
    ASSERT_NE(val1, map.end());
    EXPECT_EQ(val1->second, "one");

    auto val2 = map.find(2);
    ASSERT_NE(val2, map.end());
    EXPECT_EQ(val2->second, "two");
}

TEST(HashMapTest, FindNonExistent) {
    optimap::HashMap<int, std::string> map;
    map.insert(1, "one");
    auto val = map.find(3);
    EXPECT_EQ(val, map.end());
}

TEST(HashMapTest, InsertDuplicate) {
    optimap::HashMap<int, std::string> map;
    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_FALSE(map.insert(1, "uno"));  // Should return false for duplicate
    EXPECT_EQ(map.size(), 1);

    auto val = map.find(1);
    ASSERT_NE(val, map.end());
    EXPECT_EQ(val->second, "one");  // Value should not be updated
}

TEST(HashMapTest, Erase) {
    optimap::HashMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");
    EXPECT_EQ(map.size(), 2);

    EXPECT_TRUE(map.erase(1));
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map.find(1), map.end());
    ASSERT_NE(map.find(2), map.end());  // Make sure other keys are unaffected

    EXPECT_FALSE(map.erase(1));  // Erasing again should fail
    EXPECT_EQ(map.size(), 1);
}

TEST(HashMapTest, StringKeys) {
    optimap::HashMap<std::string, int> map;
    EXPECT_TRUE(map.insert("alpha", 1));
    EXPECT_TRUE(map.insert("beta", 2));

    auto val = map.find("alpha");
    ASSERT_NE(val, map.end());
    EXPECT_EQ(val->second, 1);
    EXPECT_EQ(map.find("gamma"), map.end());
}

TEST(HashMapTest, EmptyMapOperations) {
    optimap::HashMap<int, int> map;
    EXPECT_EQ(map.size(), 0);
    EXPECT_EQ(map.find(100), map.end());
    EXPECT_FALSE(map.erase(100));
}

// Tests for resizing behavior
TEST(ResizeTest, TriggerResize) {
    optimap::HashMap<int, int> map(16);  // Initial capacity 16
    EXPECT_EQ(map.capacity(), 16);

    // Load factor is 0.875, so 14 elements will fill it. 15th will trigger resize.
    for (int i = 0; i < 15; ++i) {
        map.insert(i, i * 10);
    }

    EXPECT_EQ(map.size(), 15);
    EXPECT_EQ(map.capacity(), 32);  // Should have doubled

    // Verify all old elements are still present
    for (int i = 0; i < 14; ++i) {
        auto val = map.find(i);
        ASSERT_NE(val, map.end());
        EXPECT_EQ(val->second, i * 10);
    }
}

TEST(ResizeTest, InsertMany) {
    optimap::HashMap<int, int> map;
    const int num_elements = 1000;

    for (int i = 0; i < num_elements; ++i) {
        map.insert(i, i);
    }

    EXPECT_EQ(map.size(), num_elements);
    for (int i = 0; i < num_elements; ++i) {
        auto val = map.find(i);
    ASSERT_NE(val, map.end());
    EXPECT_EQ(val->second, i);
    }
}

// Custom hash struct to force collisions
struct CollisionHash {
    size_t operator()(int key) const {
        // All keys will have the same h1 value for power-of-2 capacities
        return key & 0x0F;
    }
};

TEST(CollisionTest, InsertAndFindWithCollisions) {
    optimap::HashMap<int, std::string, CollisionHash> map(16);
    // These keys will all hash to the same initial slot index
    map.insert(1, "one");
    map.insert(17, "seventeen");     // 17 & 15 == 1
    map.insert(33, "thirty-three");  // 33 & 15 == 1

    EXPECT_EQ(map.size(), 3);

    auto val1 = map.find(1);
    ASSERT_NE(val1, map.end());
    EXPECT_EQ(val1->second, "one");

    auto val17 = map.find(17);
    ASSERT_NE(val17, map.end());
    EXPECT_EQ(val17->second, "seventeen");

    auto val33 = map.find(33);
    ASSERT_NE(val33, map.end());
    EXPECT_EQ(val33->second, "thirty-three");
}

TEST(CollisionTest, EraseWithCollisions) {
    optimap::HashMap<int, std::string, CollisionHash> map(16);
    map.insert(1, "one");
    map.insert(17, "seventeen");
    map.insert(33, "thirty-three");

    // Erase the middle element in the probe chain
    EXPECT_TRUE(map.erase(17));
    EXPECT_EQ(map.size(), 2);

    // The others should still be findable
    auto val1 = map.find(1);
    ASSERT_NE(val1, map.end());
    EXPECT_EQ(val1->second, "one");

    auto val33 = map.find(33);
    ASSERT_NE(val33, map.end());
    EXPECT_EQ(val33->second, "thirty-three");
}

// A hash function to force a long probe chain and trigger the overflow mechanism
struct LongProbeHash {
    size_t operator()(int key) const {
        // Force h1 to be 0 for all keys
        // h2 will be different based on the key
        size_t h2_part = static_cast<size_t>((key % 127) + 1) << (sizeof(size_t) * 8 - 7);
        return h2_part;
    }
};

TEST(OverflowTest, InsertIntoOverflow) {
    // kGroupWidth is 16. The 17th insert with the same h1 should trigger overflow.
    optimap::HashMap<int, int, LongProbeHash> map(16);

    for (int i = 0; i < 17; ++i) {
        map.insert(i, i * 10);
    }

    // Size should be 17 (16 in primary, 1 in overflow)
    EXPECT_EQ(map.size(), 17);

    // Check that all elements are findable
    for (int i = 0; i < 17; ++i) {
        auto val = map.find(i);
        ASSERT_NE(val, map.end()) << "Failed to find key " << i;
        EXPECT_EQ(val->second, i * 10);
    }
}

TEST(OverflowTest, EraseFromOverflow) {
    optimap::HashMap<int, int, LongProbeHash> map(16);
    for (int i = 0; i < 18; ++i) {
        map.insert(i, i);
    }
    EXPECT_EQ(map.size(), 18);

    // Erase an element that is in the overflow table
    EXPECT_TRUE(map.erase(17));
    EXPECT_EQ(map.size(), 17);
    EXPECT_EQ(map.find(17), map.end());

    // Make sure another overflow element is still there
    auto val16 = map.find(16);
    ASSERT_NE(val16, map.end());
    EXPECT_EQ(val16->second, 16);
}

TEST(OverflowTest, ResizeWithOverflow) {
    optimap::HashMap<int, int, LongProbeHash> map(16);
    // 16 in primary, 4 in overflow
    for (int i = 0; i < 20; ++i) {
        map.insert(i, i);
    }
    EXPECT_EQ(map.size(), 20);

    // Trigger a resize. This should rehash all elements from primary and overflow
    // into the new, larger primary table.
    map.insert(100, 100);
    EXPECT_EQ(map.capacity(), 32);

    // All 21 elements should be present
    for (int i = 0; i < 20; ++i) {
        auto val = map.find(i);
        ASSERT_NE(val, map.end());
        EXPECT_EQ(val->second, i);
    }
    ASSERT_NE(map.find(100), map.end());
}

// Iterator tests
TEST(IteratorTest, EmptyMap) {
    optimap::HashMap<int, int> map;
    EXPECT_EQ(map.begin(), map.end());
}

TEST(IteratorTest, BasicIteration) {
    optimap::HashMap<int, int> map;
    std::set<int> expected_keys = {10, 20, 30};
    map.insert(10, 1);
    map.insert(20, 2);
    map.insert(30, 3);

    std::set<int> found_keys;
    for (const auto& entry : map) {
        found_keys.insert(entry.first);
    }
    EXPECT_EQ(found_keys, expected_keys);
}

TEST(IteratorTest, ConstIteration) {
    optimap::HashMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);

    const auto& const_map = map;
    int sum = 0;
    for (const auto& entry : const_map) {
        sum += entry.second;
    }
    EXPECT_EQ(sum, 3);
    EXPECT_EQ(const_map.cbegin(), const_map.begin());
}

TEST(IteratorTest, IterationWithDeletions) {
    optimap::HashMap<int, int> map;
    for (int i = 0; i < 10; ++i) map.insert(i, i);

    map.erase(3);
    map.erase(7);

    std::set<int> found_keys;
    for (const auto& entry : map) {
        found_keys.insert(entry.first);
    }

    std::set<int> expected_keys = {0, 1, 2, 4, 5, 6, 8, 9};
    EXPECT_EQ(found_keys, expected_keys);
}

TEST(IteratorTest, IterationWithOverflow) {
    optimap::HashMap<int, int, LongProbeHash> map(16);
    std::set<int> expected_keys;
    // 16 in primary, 2 in overflow
    for (int i = 0; i < 18; ++i) {
        map.insert(i, i);
        expected_keys.insert(i);
    }

    std::set<int> found_keys;
    for (const auto& entry : map) {
        found_keys.insert(entry.first);
    }

    EXPECT_EQ(found_keys.size(), 18);
    EXPECT_EQ(found_keys, expected_keys);
}

// Rule of 5 and Move Semantics Tests
TEST(LifecycleTest, CopyConstructor) {
    optimap::HashMap<int, std::string, LongProbeHash> map1(16);
    for (int i = 0; i < 20; ++i) {
        map1.insert(i, "value" + std::to_string(i));
    }

    optimap::HashMap<int, std::string, LongProbeHash> map2 = map1;

    EXPECT_EQ(map1.size(), map2.size());
    for (int i = 0; i < 20; ++i) {
        auto val1 = map1.find(i);
        auto val2 = map2.find(i);
        ASSERT_NE(val1, map1.end());
        ASSERT_NE(val2, map2.end());
        EXPECT_EQ(val1->second, val2->second);
    }

    // Ensure it's a deep copy by modifying one
    map2.insert(100, "new");
    EXPECT_NE(map1.size(), map2.size());
    EXPECT_EQ(map1.find(100), map1.end());
}

TEST(LifecycleTest, CopyAssignment) {
    optimap::HashMap<int, int> map1;
    map1.insert(1, 10);
    map1.insert(2, 20);

    optimap::HashMap<int, int> map2;
    map2.insert(3, 30);
    map2 = map1;

    EXPECT_EQ(map1.size(), map2.size());
    EXPECT_NE(map2.find(1), map2.end());
    EXPECT_EQ(map2.find(3), map2.end());
}

TEST(LifecycleTest, MoveConstructor) {
    optimap::HashMap<int, int> map1;
    map1.insert(1, 1);
    optimap::HashMap<int, int> map2 = std::move(map1);

    EXPECT_NE(map2.find(1), map2.end());
    // It's hard to assert the state of map1 other than it should be valid.
    // Let's just check if it's empty, which is a common state after move.
    EXPECT_EQ(map1.size(), 0);
}

TEST(LifecycleTest, MoveAssignment) {
    optimap::HashMap<int, int> map1;
    map1.insert(1, 1);
    optimap::HashMap<int, int> map2;
    map2.insert(2, 2);
    map2 = std::move(map1);

    EXPECT_NE(map2.find(1), map2.end());
    EXPECT_EQ(map2.find(2), map2.end());
    EXPECT_EQ(map1.size(), 0);
}

// Test with move-only types
struct MoveOnly {
    int val;
    explicit MoveOnly(int v = 0) : val(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    // Need operator== for tests to work
    bool operator==(const MoveOnly& other) const { return val == other.val; }
};

// Hash specialization for MoveOnly
namespace std {
template <>
struct hash<MoveOnly> {
    size_t operator()(const MoveOnly& m) const { return std::hash<int>()(m.val); }
};
}  // namespace std

TEST(MoveSemanticsTest, MoveOnlyValue) {
    optimap::HashMap<int, MoveOnly> map;
    map.insert(1, MoveOnly(100));
    map.emplace(2, MoveOnly(200));

    auto val = map.find(1);
    ASSERT_NE(val, map.end());
    EXPECT_EQ(val->second.val, 100);
}
