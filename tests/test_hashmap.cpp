#include <gtest/gtest.h>

#include "hashmap.hpp"

TEST(HashMapTest, InsertAndLookup) {
    optimap::HashMap<int, std::string> map;

    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_TRUE(map.insert(2, "two"));

    auto val1 = map.find(1);
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), "one");

    auto val2 = map.find(2);
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), "two");

    auto val3 = map.find(3);
    EXPECT_FALSE(val3.has_value());
}

TEST(HashMapTest, OverwriteValue) {
    optimap::HashMap<int, std::string> map;
    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_FALSE(map.insert(1, "uno"));  // Returns false if present

    auto val = map.find(1);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "one");
}

TEST(HashMapTest, RemoveKey) {
    optimap::HashMap<int, std::string> map;
    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_TRUE(map.erase(1));

    auto val = map.find(1);
    EXPECT_FALSE(val.has_value());

    EXPECT_FALSE(map.erase(1));  // already erased
}
