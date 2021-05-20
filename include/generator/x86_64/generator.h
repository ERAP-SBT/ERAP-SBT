#pragma once

#include "ir/ir.h"

namespace generator::x86_64
{
    struct Generator
    {
        IR *ir;

        Generator(IR *ir) : ir(ir) { }

        void compile();

        protected:
        void compile_statics();
        void compile_blocks();
        void compile_block(BasicBlock *);
        void compile_entry();

        void compile_cf_args(BasicBlock *, const CfOp &);
        void compile_static_output(BasicBlock *);
    };
}  // namespace generator::x86_64
