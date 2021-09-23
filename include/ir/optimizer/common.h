#pragma once

#include "ir/operation.h"
#include "ir/variable.h"

#include <map>

class VarRewriter {
  private:
    // old id -> new var
    std::map<size_t, SSAVar *> rewrites;

    void visit_refptr(RefPtr<SSAVar> &);

  public:
    void replace(SSAVar *old_var, SSAVar *new_var);

    void apply_to(Operation &);
    void apply_to(CfOp &);

    void apply_to(std::vector<CfOp> &ops) {
        for (auto &op : ops) {
            apply_to(op);
        }
    }
};

[[noreturn]] void panic_internal(const char *file, int line, const char *message);
#define panic(message) panic_internal(__FILE__, __LINE__, message)
#define unreachable() panic_internal(__FILE__, __LINE__, "Code path marked as unreachable was reached")
