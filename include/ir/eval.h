#pragma once

#include "ir/instruction.h"
#include "ir/type.h"

#include <utility>

uint64_t eval_unary_op(Instruction insn, Type type, uint64_t value);
uint64_t eval_binary_op(Instruction insn, Type type, uint64_t a, uint64_t b);
uint64_t eval_morphing_op(Instruction insn, Type from, Type to, uint64_t raw);
std::pair<uint64_t, uint64_t> eval_div(Instruction insn, Type type, uint64_t a, uint64_t b);

bool typed_equal(Type type, uint64_t a, uint64_t b);
uint64_t typed_narrow(Type type, uint64_t value);
