#pragma once

#include "function.h"
#include <memory>
#include <vector>

class IR
{
    private:
    std::vector<std::shared_ptr<Function>> functions;
    std::vector<std::shared_ptr<StaticMapper>> statics;

    public:
    IR();
    ~IR();

    void add_function(const std::shared_ptr<Function> &);
    void add_statics(const std::shared_ptr<StaticMapper> &);

    // Getters
    inline const std::vector<std::shared_ptr<Function>> &get_functions() const { return functions; }
    inline const std::vector<std::shared_ptr<StaticMapper>> &get_statics() const { return statics; }
};