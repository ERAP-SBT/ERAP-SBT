#include <gtest/gtest.h>

class Test1 : public ::testing::Test {};

#if 0
TEST_F(Test1, test1_fail) { EXPECT_EQ(true, false) << "foo"; }
#endif

TEST_F(Test1, test1_success) { EXPECT_EQ(true, true) << "bar"; }

void unused_function() { /* for coverage tests */
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
