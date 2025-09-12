#include <gtest/gtest.h>

#include "hashmap.hpp"

TEST(ContainsTest, Contains) {
    optimap::HashMap<uint64_t, uint64_t> map;

    EXPECT_FALSE(map.contains(0));
    EXPECT_FALSE(map.contains(123));
    map[123];
    EXPECT_FALSE(map.contains(0));
    EXPECT_TRUE(map.contains(123));
    map.clear();
    EXPECT_FALSE(map.contains(0));
    EXPECT_FALSE(map.contains(123));
}
