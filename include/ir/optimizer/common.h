#pragma once

#include "ir/operation.h"
#include "ir/variable.h"

#include <map>

namespace optimizer {
class VarRewriter {
  private:
    // old id -> new var
    std::map<size_t, SSAVar *> rewrites;

    void visit_refptr(RefPtr<SSAVar> &) const;

  public:
    void replace(SSAVar *old_var, SSAVar *new_var);

    void apply_to(Operation &) const;
    void apply_to(CfOp &) const;

    void apply_to(std::vector<CfOp> &ops) const {
        for (auto &op : ops) {
            apply_to(op);
        }
    }

    void clear() { rewrites.clear(); }
};

enum Optimization : uint32_t {
    OPT_DCE = 1 << 0,
    OPT_CONST_FOLDING = 1 << 1,
    OPT_DEDUP = 1 << 2,
};

constexpr uint32_t OPT_FLAGS_ALL = 0xFFFFFFFF;

[[noreturn]] void panic_internal(const char *file, int line, const char *message);
#define panic(message) ::optimizer::panic_internal(__FILE__, __LINE__, message)
#define unreachable() ::optimizer::panic_internal(__FILE__, __LINE__, "Code path marked as unreachable was reached")

} // namespace optimizer
