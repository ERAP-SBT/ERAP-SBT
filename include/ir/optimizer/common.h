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
