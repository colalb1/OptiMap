#include <gtest/gtest.h>

#include "hashmap.hpp"

TEST(AtTest, At) {
    optimap::HashMap<int, int> map;
    optimap::HashMap<int, int> const& cmap = map;

    EXPECT_THROW(map.at(123), std::out_of_range);
    EXPECT_THROW(cmap.at(123), std::out_of_range);

    map[123] = 333;
    EXPECT_EQ(map.at(123), 333);
    EXPECT_EQ(cmap.at(123), 333);

    EXPECT_THROW(map.at(0), std::out_of_range);
    EXPECT_THROW(cmap.at(0), std::out_of_range);
}
