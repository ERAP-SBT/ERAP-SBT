#pragma once

#include <deque>
#include <ir/ir.h>
#include <lifter/program.h>
#include <unordered_set>

namespace lifter::RV64 {

/* amount of "normal" static variables: zero register (x0) + 31 general purpose registers (x1-x31) + memory token */
constexpr size_t COUNT_STATIC_VARS = 33;

/* amount of static floating point variables: 32 floating point registers (f0-f31) + fcsr*/
constexpr size_t COUNT_STATIC_FP_VARS = 33;

class Lifter {
  public:
    enum Optimization : uint32_t { OPT_CALL_RET = 1 << 0 };

    IR *ir;
    std::vector<bool> needs_bb_start;

    // currently used for unresolved jumps
    BasicBlock *dummy;

    const bool floating_point_support;

    const size_t count_used_static_vars;

    const bool interpreter_only;

    const uint32_t optimizations;

    explicit Lifter(IR *ir, bool floating_point_support = false, bool interpreter_only = false, uint32_t optimizations = 0)
        : ir(ir), dummy(), floating_point_support(floating_point_support), count_used_static_vars(COUNT_STATIC_VARS + (floating_point_support ? COUNT_STATIC_FP_VARS : 0)),
          interpreter_only(interpreter_only), optimizations(optimizations) {}

    void lift(Program *prog);

    // Register index for constant zero "register" (RISC-V default: 0)
    static constexpr size_t ZERO_IDX = 0;

    // Register index for return (or link-) register.
    static constexpr size_t LINK_IDX_1 = 1;
    static constexpr size_t LINK_IDX_2 = 5;

    // Index of memory token in <reg_map> register mapping
    static constexpr size_t MEM_IDX = 32;

    // Index of the first floating point value in <reg_map>
    static constexpr size_t START_IDX_FLOATING_POINT_STATICS = 33;

    // Index of the fcsr register in <reg_map> register mapping
    static constexpr size_t FCSR_IDX = 65;

    // Depth of jump address backtracking
    static constexpr int MAX_ADDRESS_SEARCH_DEPTH = 3;

    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32}: the last valid memory token
    // {33, ..., 64}: FP-Registers
    // {65}: fcsr
    using reg_map = std::array<SSAVar *, COUNT_STATIC_VARS + COUNT_STATIC_FP_VARS>;

    void parse_instruction(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    void add_statics() const {
        for (unsigned i = 0; i < 32; i++) {
            ir->add_static(Type::i64);
        }

        ir->add_static(Type::mt);

        if (floating_point_support) {
            for (unsigned i = 0; i < 32; i++) {
                ir->add_static(Type::f64);
            }

            ir->add_static(Type::i32);
        }
    }

    static void lift_invalid(BasicBlock *bb, uint64_t ip);

    void lift_shift_shared(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size, SSAVar *shift_val);

    void lift_shift(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size);

    void lift_shift_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size);

    void lift_slt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool is_unsigned, bool with_immediate);

    void lift_fence(BasicBlock *bb, const RV64Inst &instr, uint64_t ip);

    void lift_auipc(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip);

    void lift_lui(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip);

    void lift_jal(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    void lift_jalr(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    void lift_branch(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    void lift_ecall(BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    void lift_load(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size, bool sign_extend);

    void lift_store(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    void lift_arithmetical_logical(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    void lift_arithmetical_logical_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    void lift_mul(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instr_type, const Type type);

    void lift_div(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool _signed, bool remainder, const Type in_type);

    // atomics
    /**
     * Adds operations and variables to the {@link BasicBlock} bb which perform the load from the memory at address in rs1 to rd. If the {@link Type op_size} is {@link Type::i32}, the load value is
     * sign_extended and written to the corresponding place in the mapping. But as a 32bit operation will be performed on this value, the value is returned as 32bit value. This saves some sign extend
     * and cast instructions in the IR. If the operation size is {@link Type::i64}, the no sign extension is needed and therefore the loaded value is written to the mapping as he is returned by this
     * method. This explanation is valid in the context of amo instructions and therefore there are less applications of this method in other contexts.
     *
     * @param bb The current basic block.
     * @param instr The decoded instruction.
     * @param mapping The current mapping.
     * @param ip The current instruction pointer.
     * @param op_size The operation size of the instruction, either {@link Type::i32} or {@link Type::i64}.
     * @return SSAVar* The returned
     */
    SSAVar *load_rs1_to_rd(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type op_size);
    void store_val_to_rs1(BasicBlock *bb, const RV64Inst &instr, Lifter::reg_map &mapping, uint64_t ip, const Type &op_size, SSAVar *value);
    void lift_amo_load_reserve(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    void lift_amo_store_conditional(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    void lift_amo_swap(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    void lift_amo_binary_op(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    // ziscr and ziscr helpers
    void lift_csr_read_write(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    void lift_csr_read_set(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    void lift_csr_read_clear(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    SSAVar *get_csr(BasicBlock *bb, reg_map &mapping, const uint64_t ip, uint32_t csr_identifier);

    void write_csr(BasicBlock *bb, reg_map &mapping, SSAVar *new_value, const uint64_t ip, uint32_t csr_identifier);

    // floating points

    std::variant<std::monostate, RoundingMode, RefPtr<SSAVar>> parse_rounding_mode(BasicBlock *bb, reg_map &mapping, const uint64_t ip, const uint8_t riscv_rm);
    void lift_float_two_operands(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);
    void lift_float_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    void lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);
    void lift_float_integer_conversion(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to, bool _signed);
    void lift_float_sign_injection(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    void lift_float_move(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to);
    void lift_float_comparison(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);
    void lift_fclass(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    // helpers for lifting and code reduction
    SSAVar *load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative);

    SSAVar *load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative);

    SSAVar *shrink_var(BasicBlock *bb, SSAVar *var, uint64_t ip, const Type target_size);

    BasicBlock *split_basic_block(BasicBlock *bb, uint64_t addr, ELF64File *elf_base) const;

    std::optional<SSAVar *> convert_type(BasicBlock *bb, uint64_t ip, SSAVar *var, const Type desired_type);

    void print_invalid_op_size(const Instruction instructionType, const RV64Inst &instr);

    std::string str_decode_instr(const FrvInst *instr);

    // helpers for backtracking
    std::unordered_set<int64_t> backtrace_jmp_addrs(CfOp *cfop, BasicBlock *bb);
    std::unordered_set<int64_t> get_var_values(const std::vector<SSAVar *> &start_vars, BasicBlock *bb, std::vector<SSAVar *> &parsed_vars);
    std::unordered_set<SSAVar *> get_last_static_assignments(size_t idx, BasicBlock *bb);
    std::vector<std::array<int64_t, 4>> load_input_vars(BasicBlock *bb, Operation *op, std::vector<SSAVar *> &parsed_vars);
    void register_jump_address(BasicBlock *jump_bb, uint64_t jmp_addr, ELF64File *elf_base);
    void process_ijumps(std::vector<CfOp *> &unprocessed_ijumps, ELF64File *elf_base);

    bool is_jump_table_jump(const BasicBlock *bb, CfOp &cfOp, const RV64Inst &instr, const Program *prog);

    /**
     * Returns the corresponding value from the mapping: If `is_floating_point_register == true` the slots for the floating points are accessed, if not the slots for the general purpose/integer
     * registers are accessed. As identifer the register index as used by the RISC-V manual and frvdec is used. If the x0-Register is specified (`reg_id = 0 && is_floating_point_register == false`) a
     * 0 immediate is created in the current basic block.
     *
     * @param bb The current {@link BasicBlock basic block}.
     * @param mapping The {@link reg_map mapping} to read from.
     * @param reg_id The identifier (id) of the register as used by RISC-V manual and frvdec.
     * @param ip The current instruction pointer (ip), used for variable creation. (see handling of x0-Register)
     * @param is_floating_point_register Defines from which slots should be read: either general purpose/integer register or floating point slots. The default value is false.
     * @return SSAVar * A pointer to the variable stored at the given position in the mapping.
     */
    SSAVar *get_from_mapping(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, bool is_floating_point_register = false) const;

    /**
     * Writes the given {@link SSAVar variable} to the {@link reg_map mapping}. Calls with `reg_id = 0 && is_floating_point_register == false` are ignored due to this are writes the unused slot in the
     * mapping for the x0-Register. With `is_floating_point_register == true` the variable is written to the slots for the floating point registers which are currently stored with a defined @link
     * START_IDX_FLOATING_POINT_STATICS offset @endlink after the integer register variables.
     *
     * @param mapping The {@link reg_map mapping} which should be modified.
     * @param var The variable to write to the mapping.
     * @param reg_id The identifier (id) of the register as used by RISC-V manual and frvdec.
     * @param is_floating_point_register Defines whether the variable should be written to the slots for general puropse/integer register or to the slots for floating point register. The default value
     * is false.
     */
    void write_to_mapping(reg_map &mapping, SSAVar *var, uint64_t reg_id, bool is_floating_point_register = false);

    /**
     * Zero extends all f32 variables in the {@link reg_map mapping}. Mainly used to correct the mapping before assigning it to control flow operations.
     *
     * @param bb The current basic block.
     * @param mapping The mapping to work on.
     * @param ip The current instruction pointer for setting the right address of newly created variables.
     */
    void zero_extend_all_f32(BasicBlock *bb, reg_map &mapping, uint64_t ip) const;

    SSAVar *get_from_mapping_and_shrink(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, const Type expected_type) const;

    // return true if entered number corresponds to a link register
    static bool is_link_reg(size_t reg_idx);

    void postprocess(Program *prog);
};
} // namespace lifter::RV64
