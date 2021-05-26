#include "lifter/lifter.h"

#include "gtest/gtest.h"

namespace lifter {

class Test1 : public ::testing::Test {};

TEST_F(Test1, test1) { EXPECT_EQ(foo(), true) << "foo() not implemented correctly"; }

} // namespace lifter

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
