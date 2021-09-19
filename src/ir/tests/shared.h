#pragma once

#include "ir/ir.h"

#include "gtest/gtest.h"

inline void assert_valid(const IR &ir) {
    std::vector<std::string> messages;
    if (!ir.verify(messages)) {
        for (const auto &msg : messages) {
            std::cerr << msg << '\n';
        }
        GTEST_FAIL() << "IR is not valid";
    }
}
