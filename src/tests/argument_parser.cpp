#include "argument_parser.h"

#include <array>
#include <gtest/gtest.h>

TEST(ArgumentParser, test_positional_arguments_only) {
    std::array argv = {"positional1", "test_file", "more-pos"};
    Args args(argv.begin(), argv.end());

    EXPECT_EQ(args.positional.size(), 3);
    for (size_t i = 0; i < argv.size(); i++)
        EXPECT_EQ(args.positional[i], argv[i]);

    EXPECT_TRUE(args.arguments.empty());
}

TEST(ArgumentParser, test_arguments_only) {
    std::array argv = {"--no-value", "--key=value", "--empty="};
    Args args(argv.begin(), argv.end());

    EXPECT_EQ(args.arguments.size(), 3);

    EXPECT_TRUE(args.has_argument("no-value"));
    EXPECT_EQ(args.get_argument("no-value"), "") << "Argument without value should return empty string";

    EXPECT_TRUE(args.has_argument("key"));
    EXPECT_EQ(args.get_argument("key"), "value") << "Argument with value should return its value";

    EXPECT_TRUE(args.has_argument("empty"));
    EXPECT_EQ(args.get_argument("empty"), "") << "Argument with empty value should return empty string";

    EXPECT_TRUE(args.positional.empty());
}

TEST(ArgumentParser, test_mixed) {
    std::array argv = {"pos1", "--arg1", "pos2", "--arg2=value2"};
    Args args(argv.begin(), argv.end());

    EXPECT_EQ(args.arguments.size(), 2);

    EXPECT_TRUE(args.has_argument("arg1"));
    EXPECT_EQ(args.get_argument("arg1"), "");

    EXPECT_TRUE(args.has_argument("arg2"));
    EXPECT_EQ(args.get_argument("arg2"), "value2");

    EXPECT_EQ(args.positional.size(), 2);
    EXPECT_EQ(args.positional[0], "pos1");
    EXPECT_EQ(args.positional[1], "pos2");
}
