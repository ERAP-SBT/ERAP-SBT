#pragma once

#include "ref.h"
#include "type.h"

#include <memory>
#include <string>
#include <variant>

// forward declaration
struct Operation;
struct IR;

struct SSAVar : Refable {

    struct ImmInfo {
        int64_t val;

        /* true: val is relative to ir->base_addr
         * false: val is absolute
         */
        bool binary_relative = false;
    };

    struct LifterInfo {
        // the address at which the variable was assigned a value, where 0 := "unknown".
        uint64_t assign_addr = 0;
        // if the variable is `from_static`, this variable holds its actual index. Otherwise, this variable stores the potential static mapper index / register number.
        // The lifter uses this information mainly for basic block splitting and retro-fitting jumps into parsed basic blocks.
        size_t static_id = SIZE_MAX;
    };

    struct GeneratorInfoX64 {
        // TODO: add ability to alias var which includes immediates
        // so that for example downcasts dont need extra space in registers and stack
        union {
            size_t reg_idx = 0;
            size_t static_idx;
            size_t loc_info;
        };
        size_t stack_slot = 0;

        // TODO: allow a variable to be in a register and static and don't save it to the stack if it is?
        enum LOCATION : uint8_t { NOT_CALCULATED, STACK_FRAME, STATIC, REGISTER, FP_REGISTER };
        LOCATION location = NOT_CALCULATED;
        bool saved_in_stack = false; // a variable can be in a register and saved in the stack frame
        bool already_generated = false;
        bool allocated_to_input = false;

        size_t last_use_time = 0;
        std::vector<size_t> uses = {};
    };

    size_t id;
    Type type;

    // immediate, static idx, op
    std::variant<std::monostate, ImmInfo, size_t, std::unique_ptr<Operation>> info;

    // Lifter-specific information which can be cleared afterwards
    std::variant<std::monostate, LifterInfo> lifter_info;
    // TODO: merge with lifter_info
    GeneratorInfoX64 gen_info;

    SSAVar(const size_t id, const Type type) : id(id), type(type), info(std::monostate{}) {}
    SSAVar(const size_t id, const Type type, const size_t static_idx) : id(id), type(type), info(static_idx), lifter_info(LifterInfo{0, static_idx}) {}
    SSAVar(const size_t id, const int64_t imm, const bool binary_relative = false) : id(id), type(Type::imm), info(ImmInfo{imm, binary_relative}) {}

    void set_op(std::unique_ptr<Operation> &&ptr);

    constexpr bool is_immediate() const { return std::holds_alternative<ImmInfo>(info); }
    ImmInfo &get_immediate() { return std::get<ImmInfo>(info); }
    const ImmInfo &get_immediate() const { return std::get<ImmInfo>(info); }

    constexpr bool is_operation() const { return std::holds_alternative<std::unique_ptr<Operation>>(info); }
    Operation &get_operation() { return *std::get<std::unique_ptr<Operation>>(info); }
    const Operation &get_operation() const { return *std::get<std::unique_ptr<Operation>>(info); }
    Operation *maybe_get_operation() { return is_operation() ? &get_operation() : nullptr; }
    const Operation *maybe_get_operation() const { return is_operation() ? &get_operation() : nullptr; }

    constexpr bool is_static() const { return std::holds_alternative<size_t>(info); }
    size_t get_static() const { return std::get<size_t>(info); }

    constexpr bool is_uninitialized() const { return std::holds_alternative<std::monostate>(info); }

    void print(std::ostream &, const IR *) const;
    void print_type_name(std::ostream &, const IR *) const;
};

/*
 * A static mapper is not a variable and thus not an ssa-variable.
 */
struct StaticMapper {
    const size_t id;
    const Type type;

    StaticMapper(size_t id, Type type) : id(id), type(type) {}

    void print(std::ostream &) const;
};
