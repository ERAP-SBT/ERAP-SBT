#pragma once

#include <ir/ir.h>
#include <lifter/program.h>

namespace lifter::RV64 {

/* maximum number of riscv64 instructions a basicblock can have while lifting */
constexpr size_t BASIC_BLOCK_MAX_INSTRUCTIONS = 10000;

#if 0
/* amount of static variables: zero register (x0) + 31 general purpose registers (x1-x31) + 32 floating point registers (f0-f31) + memory token + fcsr*/
constexpr size_t COUNT_STATIC_VARS = 66;
#endif

/* amount of static variables: zero register (x0) + 31 general purpose registers (x1-x31) + memory token*/
constexpr size_t COUNT_STATIC_VARS = 33;

class Lifter {
  public:
    IR *ir;

    explicit Lifter(IR *ir) : ir(ir), dummy() {}

    void lift(Program *prog);

    // Register index for constant zero "register" (RISC-V default: 0)
    static constexpr size_t ZERO_IDX = 0;

    static constexpr size_t START_IDX_FLOATING_POINT_STATICS = 32;
#if 0
    // Index of memory token in <reg_map> register mapping
    static constexpr size_t MEM_IDX = 64;
#endif

    // Index of memory token in <reg_map> register mapping
    static constexpr size_t MEM_IDX = 32;

#if 0
    // Index of the fcsr register in <reg_map> register mapping
    static constexpr size_t FCSR_IDX = 65;
#endif

    // Depth of jump address backtracking
    static constexpr int MAX_ADDRESS_SEARCH_DEPTH = 10;

    // lift all data points from the load program header
    static constexpr bool LIFT_ALL_LOAD = true;

    // {0}: not used
    // {1, ..., 31}: RV-Registers
    // {32, ..., 63}: FP-Registers
    // {64}: the last valid memory token
    // {65}: fcsr
    using reg_map = std::array<SSAVar *, COUNT_STATIC_VARS>;

    // currently used for unresolved jumps
    BasicBlock *dummy;

    static void parse_instruction(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    [[nodiscard]] BasicBlock *get_bb(uint64_t addr) const;

    void lift_rec(Program *prog, Function *func, uint64_t start_addr, std::optional<size_t> addr_idx, BasicBlock *curr_bb);

    static void lift_invalid(BasicBlock *bb, uint64_t ip);

    static void lift_shift_shared(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size, SSAVar *shift_val);

    static void lift_shift(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size);

    static void lift_shift_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Instruction instruction_type, Type op_size);

    static void lift_slt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool is_unsigned, bool with_immediate);

    static void lift_fence(BasicBlock *bb, const RV64Inst &instr, uint64_t ip);

    static void lift_auipc(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip);

    static void lift_lui(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip);

    static void lift_jal(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void lift_jalr(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void lift_branch(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void lift_ecall(BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr);

    static void lift_load(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size, bool sign_extend);

    static void lift_store(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    static void lift_arithmetical_logical(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    static void lift_arithmetical_logical_immediate(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    static void lift_mul(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instr_type, const Type type);

    static void lift_div(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool _signed, bool remainder, const Type in_type);

    // atomics
    static void lift_amo_load_reserve(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_store_conditional(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_add(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_swap(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_xor(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_or(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_and(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);
    static void lift_amo_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, Type op_size, Instruction instr_type);
    static void lift_amo_min(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size, bool _signed);
    static void lift_amo_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size, bool _signed);

    // ziscr and ziscr helpers
    static void lift_csr_read_write(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    static void lift_csr_read_set(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    static void lift_csr_read_clear(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, bool with_immediate);

    static SSAVar *get_csr(reg_map &mapping, uint32_t csr_identifier);

    static void write_csr(reg_map &mapping, SSAVar *new_csr, uint32_t csr_identifier);

    // floating points

    static void lift_float_div(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    static void lift_float_sqrt(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    static void lift_float_min_max(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    static void lift_float_fma(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    static void lift_float_integer_conversion(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to, bool _signed);

    static void lift_float_sign_injection(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    static void lift_float_move(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type from, const Type to);

    static void lift_float_comparison(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Instruction instruction_type, const Type op_size);

    static void lift_fclass(BasicBlock *bb, const RV64Inst &instr, reg_map &mapping, uint64_t ip, const Type op_size);

    // helpers for lifting and code reduction
    static SSAVar *load_immediate(BasicBlock *bb, int64_t imm, uint64_t ip, bool binary_relative);

    static SSAVar *load_immediate(BasicBlock *bb, int32_t imm, uint64_t ip, bool binary_relative);

    static SSAVar *shrink_var(BasicBlock *bb, SSAVar *var, uint64_t ip, const Type target_size);

    static std::optional<uint64_t> backtrace_jmp_addr(CfOp *cfop, BasicBlock *bb);

    static std::optional<int64_t> get_var_value(SSAVar *var, BasicBlock *bb, std::vector<SSAVar *> &parsed_vars);

    static std::optional<SSAVar *> get_last_static_assignment(size_t idx, BasicBlock *bb);

    void split_basic_block(BasicBlock *bb, uint64_t addr, ELF64File *elf_base) const;

    static void load_input_vars(BasicBlock *bb, Operation *op, std::vector<int64_t> &resolved_vars, std::vector<SSAVar *> &parsed_vars);

    static std::optional<SSAVar *> convert_type(BasicBlock *bb, uint64_t ip, SSAVar *var, const Type desired_type);

    static void print_invalid_op_size(const Instruction instructionType, const RV64Inst &instr);

    static std::string str_decode_instr(const FrvInst *instr);

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
    static SSAVar *get_from_mapping(BasicBlock *bb, reg_map &mapping, uint64_t reg_id, uint64_t ip, bool is_floating_point_register = false);

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
    static void write_to_mapping(reg_map &mapping, SSAVar *var, uint64_t reg_id, bool is_floating_point_register = false);

    /**
     * Zero extends all f32 variables in the {@link reg_map mapping}. Mainly used to correct the mapping before assigning it to control flow operations.
     *
     * @param bb The current basic block.
     * @param mapping The mapping to work on.
     * @param ip The current instruction pointer for setting the right address of newly created variables.
     */
    static void zero_extend_all_f32(BasicBlock *bb, reg_map &mapping, uint64_t ip);

    void postprocess();
};
} // namespace lifter::RV64
