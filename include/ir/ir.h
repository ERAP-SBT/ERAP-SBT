#pragma once

#include "function.h"
#include <memory>
#include <vector>

class IR
{
    private:
    std::vector<std::shared_ptr<Function>> functions;
    // TODO: Add statics

    public:
    IR();
    ~IR();

    void add_function(const std::shared_ptr<Function> &);

    inline const std::vector<std::shared_ptr<Function>> &get_functions() const { return functions; }
};