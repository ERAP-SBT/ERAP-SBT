#include <frvdec.h>
#include <lifter/lifter.h>

using namespace lifter::RV64;

std::string str_decode_instr(FrvInst *instr) {
    char str[16];
    frv_format(instr, 16, str);
    return std::string(str);
}

void print_param_error(const Instruction &instructionType, RV64Inst &instr) { std::cerr << "Encountered " << instructionType << " instruction with invalid parameters: " << &instr.instr << "\n"; }

void print_invalid_op_size(const Instruction &instructionType, RV64Inst &instr) {
    std::cerr << "Encountered " << instructionType << " instruction with operand size: " << str_decode_instr(&instr.instr) << "\n";
}

void Lifter::lift(Program *prog) {
    uint64_t start_addr = prog->elf_base->header.e_entry;

    Function *curr_fun = ir->add_func();

    for (size_t i = 0; i < prog->elf_base->program_headers.size(); i++) {
        if (prog->elf_base->program_headers.at(i).p_type == PT_LOAD) {
            for (Elf64_Shdr *sec : prog->elf_base->segment_section_map.at(i)) {
                prog->load_section(sec);
            }
        }
    }

    liftRec(prog, curr_fun, start_addr, nullptr);
}

BasicBlock *Lifter::get_bb(uint64_t addr) const {
    for (auto &bb_ptr : ir->basic_blocks) {
        if (bb_ptr->virt_start_addr == addr) {
            return bb_ptr.get();
        }
    }
    return nullptr;
}

BasicBlock *Lifter::liftRec(Program *prog, Function *func, uint64_t start_addr, BasicBlock *pred) {
    if (pred != nullptr) {
        BasicBlock *bb = get_bb(start_addr);
        if (bb != nullptr) {
            bb->predecessors.push_back(pred);
            return bb;
        }
    }

    // TODO: get this from the static mapping of pred
    reg_map mapping{};
    std::unique_ptr<BasicBlock> bb = std::make_unique<BasicBlock>(ir, next_bb_id++, start_addr);
    func->add_block(bb.get());

    if (pred != nullptr) {
        pred->successors.push_back(bb.get());
    }

    // TODO: get this from the previous basic block
    if (mapping.at(32) == nullptr) {
        mapping.at(32) = bb->add_var(Type::mt);
    }

    size_t start_i = std::find(prog->addrs.begin(), prog->addrs.end(), start_addr) - prog->addrs.begin();
    // for now, we stop after 10000 instructions / data elements
    for (size_t i = start_i; i < start_i + 10000 && i < prog->addrs.size(); i++) {
        if (prog->data.at(i).index() == 1) {
            RV64Inst instr = std::get<RV64Inst>(prog->data.at(i));
            parse_instruction(instr, bb.get(), mapping);
            // TODO: detect control flow change
        }
    }

    return bb.get();
}

// TODO: create function which splits a BasicBlock. Used when jumping into an existing basic block.
// TODO: Start a new basic block with each defined symbol

void Lifter::parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping) {
    switch (instr.instr.mnem) {
        //        case FRV_INVALID:
        //            liftInvalid(bb);
        //            break;
    case FRV_LB:
        lift_load(bb, instr, mapping, Type::i8, true);
        break;
    case FRV_LH:
        lift_load(bb, instr, mapping, Type::i16, true);
        break;
    case FRV_LW:
        lift_load(bb, instr, mapping, Type::i32, true);
        break;
    case FRV_LD:
        lift_load(bb, instr, mapping, Type::i64, true);
        break;
    case FRV_LBU:
        lift_load(bb, instr, mapping, Type::i8, false);
        break;
    case FRV_LHU:
        lift_load(bb, instr, mapping, Type::i16, false);
        break;
    case FRV_LWU:
        lift_load(bb, instr, mapping, Type::i32, false);
        break;
    case FRV_SB:
        lift_store(bb, instr, mapping, Type::i8);
        break;
    case FRV_SH:
        lift_store(bb, instr, mapping, Type::i16);
        break;
    case FRV_SW:
        lift_store(bb, instr, mapping, Type::i32);
        break;
    case FRV_SD:
        lift_store(bb, instr, mapping, Type::i64);
        break;
    case FRV_ADD:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::add, Type::i64);
        break;
    case FRV_ADDW:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::add, Type::i32);
        break;
    case FRV_ADDI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, Instruction::add, Type::i64);
        break;
    case FRV_ADDIW:
        lift_arithmetical_logical_immediate(bb, instr, mapping, Instruction::add, Type::i32);
        break;
    case FRV_SUB:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::sub, Type::i64);
        break;
    case FRV_SUBW:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::sub, Type::i32);
        break;
    case FRV_AND:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::_and, Type::i64);
        break;
    case FRV_OR:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::_or, Type::i64);
        break;
    case FRV_XOR:
        lift_arithmetical_logical(bb, instr, mapping, Instruction::_xor, Type::i64);
        break;
    case FRV_ANDI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, Instruction::_and, Type::i64);
        break;
    case FRV_ORI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, Instruction::_or, Type::i64);
        break;
    case FRV_XORI:
        lift_arithmetical_logical_immediate(bb, instr, mapping, Instruction::_xor, Type::i64);
        break;
    case FRV_SLL:
        lift_shift(bb, instr, mapping, Instruction::shl, Type::i64);
        break;
    case FRV_SLLW:
        lift_shift(bb, instr, mapping, Instruction::shl, Type::i32);
        break;
    case FRV_SLLI:
        lift_shift_immediate(bb, instr, mapping, Instruction::shl, Type::i64);
        break;
    case FRV_SLLIW:
        lift_shift_immediate(bb, instr, mapping, Instruction::shl, Type::i32);
        break;
    case FRV_SRL:
        lift_shift(bb, instr, mapping, Instruction::shr, Type::i64);
        break;
    case FRV_SRLW:
        lift_shift(bb, instr, mapping, Instruction::shr, Type::i32);
        break;
    case FRV_SRLI:
        lift_shift_immediate(bb, instr, mapping, Instruction::shr, Type::i64);
        break;
    case FRV_SRLIW:
        lift_shift_immediate(bb, instr, mapping, Instruction::shr, Type::i32);
        break;
    case FRV_SRA:
        lift_shift(bb, instr, mapping, Instruction::sar, Type::i64);
        break;
    case FRV_SRAW:
        lift_shift(bb, instr, mapping, Instruction::sar, Type::i32);
        break;
    case FRV_SRAI:
        lift_shift_immediate(bb, instr, mapping, Instruction::sar, Type::i64);
        break;
    case FRV_SRAIW:
        lift_shift_immediate(bb, instr, mapping, Instruction::sar, Type::i32);
        break;
        //        case FRV_SLTI:
        //            liftSLTI(bb, instr, mapping);
        //            break;
        //        case FRV_SLTIU:
        //            liftSLTIU(bb, instr, mapping);
        //            break;
        //        case FRV_SLT:
        //            liftSLT(bb, instr, mapping);
        //            break;
        //        case FRV_SLTU:
        //            liftSLTU(bb, instr, mapping);
        //            break;
        //        case FRV_FENCE:
        //            liftFENCE(bb, mapping);
        //            break;
        //        case FRV_FENCEI:
        //            liftFENCEI(bb, mapping);
        //            break;
        //        case FRV_AUIPC:
        //            liftAUIPC(bb, mapping);
        //            break;
        //        case FRV_LUI:
        //            liftLUI(bb, mapping);
        //            break;
        //        case FRV_JAL:
        //            liftJAL(bb, mapping);
        //            break;
        //        case FRV_JALR:
        //            liftJALR(bb, mapping);
        //            break;
    case FRV_BEQ:
    case FRV_BNE:
    case FRV_BLT:
    case FRV_BGE:
    case FRV_BLTU:
        //        case FRV_BGEU:
        //            liftBranch(bb, mapping);
        //            break;
        //        case FRV_ECALL:
        //            liftECALL(bb, mapping);
        //            break;
    default:
        char instr_str[16];
        frv_format(&instr.instr, 16, instr_str);
        std::cerr << "Encountered invalid instruction during lifting: " << instr_str << "\n";
    }
}

void Lifter::lift_load(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Type &op_size, bool sign_extend) {
    // lift offset calculation
    offset_adding(bb, instr, mapping);

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::load);

    SSAVar *result_memory_token = bb->add_var(Type::mt);

    operation->set_inputs(mapping.at(instr.instr.rs1), mapping.at(32));
    operation->set_outputs(destination, result_memory_token);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation and new memory token back to mapping
    mapping.at(instr.instr.rd) = destination;
    mapping.at(32) = result_memory_token;

    // extend the loaded data to 64bit
    extend_reg(instr.instr.rd, bb, mapping, sign_extend);
}

void Lifter::lift_store(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Type &op_size) {
    // lift offset calculation
    offset_adding(bb, instr, mapping);

    // cast variable to store to operand size
    shrink_reg(instr.instr.rs2, bb, mapping, op_size);

    // create memory_token
    SSAVar *result_memory_token = bb->add_var(Type::mt);

    // create the store operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::store);

    // set in- and outputs
    operation->set_inputs(mapping.at(instr.instr.rs1), mapping.at(instr.instr.rs2), mapping.at(32));
    operation->set_outputs(result_memory_token);

    // set operation
    result_memory_token->set_op(std::move(operation));

    // write memory_token back
    mapping.at(32) = result_memory_token;
}

void Lifter::lift_shift(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Instruction &instruction_type, const Type &op_size) {
    // prepare for shift, only use lower 5bits

    SSAVar *mask = bb->add_var_imm(0x1F);

    // cast immediate from 64bit to 32bit if instruction has 32bit size
    if (op_size == Type::i32) {
        SSAVar *casted_mask = bb->add_var(Type::i32);
        std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::cast);
        operation->set_inputs(mask);
        operation->set_outputs(mask);
        casted_mask->set_op(std::move(operation));
        mask = casted_mask;
    }

    // create new variable with the result of the masking
    SSAVar *masked_count_shifts = bb->add_var(op_size);
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::_and);
    operation->set_inputs(mapping.at(instr.instr.rs2), mask);
    operation->set_outputs(masked_count_shifts);
    mapping.at(instr.instr.rs2) = masked_count_shifts;

    lift_arithmetical_logical(bb, instr, mapping, instruction_type, op_size);
}

void Lifter::lift_shift_immediate(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Instruction &instruction_type, const Type &op_size) {
    // masking the operand
    instr.instr.imm = instr.instr.imm & 0x1F;
    lift_arithmetical_logical_immediate(bb, instr, mapping, instruction_type, op_size);
}

void Lifter::lift_arithmetical_logical(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Instruction &instruction_type, const Type &op_size) {
    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size);

    // create the shl operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    SSAVar *source_one = mapping.at(instr.instr.rs1);
    SSAVar *source_two = mapping.at(instr.instr.rs2);

    // TODO: Solve this operand size miss match, maybe through type conversion?
    // test for invalid operand sizes
    if (source_one->type != op_size || source_two->type != op_size) {
        print_invalid_op_size(instruction_type, instr);
    }

    // set operation in- and outputs
    operation->set_inputs(source_one, source_two);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = destination;
}

void Lifter::lift_arithmetical_logical_immediate(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Instruction &instruction_type, const Type &op_size) {
    // create immediate var (before destination!)
    SSAVar *immediate = bb->add_var_imm(instr.instr.imm);

    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(op_size);

    // create the shl operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(instruction_type);

    SSAVar *source_one = mapping.at(instr.instr.rs1);

    // TODO: Solve this operand size miss match, maybe through type conversion?
    // test for invalid operand sizes
    if (source_one->type != op_size) {
        print_invalid_op_size(instruction_type, instr);
    }

    // set operation in- and outputs
    operation->set_inputs(source_one, immediate);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = destination;
}

void Lifter::extend_reg(size_t reg, BasicBlock *bb, reg_map &mapping, bool sign_extend) {
    // create SSAVariable for the destination operand
    SSAVar *destination = bb->add_var(Type::i64);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(sign_extend ? Instruction::sign_extend : Instruction::zero_extend);

    SSAVar *source_one = mapping.at(reg);

    operation->set_inputs(source_one);
    operation->set_outputs(destination);

    // assign the operation as variable of the destination
    destination->set_op(std::move(operation));

    // write SSAVar of the result of the operation back to mapping
    mapping.at(reg) = destination;
}

void Lifter::shrink_reg(size_t reg, BasicBlock *bb, reg_map &mapping, const Type &target_size) {
    // create casted variable
    SSAVar *destination = bb->add_var(target_size);

    // create cast operation
    std::unique_ptr<Operation> cast = std::make_unique<Operation>(Instruction::cast);

    // set in- and outputs
    cast->set_inputs(mapping.at(reg));
    cast->set_outputs(destination);

    destination->set_op(std::move(cast));

    // write register back to mapping
    mapping.at(reg) = destination;
}

void Lifter::offset_adding(BasicBlock *bb, RV64Inst &instr, reg_map &mapping) {
    SSAVar *address = bb->add_var(Type::i64);

    // create pseudo-instruction, that adds the offset to the first source register (the result is stored in the same register like the first source register)
    RV64Inst offset_calc_instr{FrvInst{FRV_ADDI, instr.instr.rs1, instr.instr.rs1, FRV_REG_INV, FRV_REG_INV, 0, instr.instr.imm}};

    // lift instruction
    lift_arithmetical_logical_immediate(bb, offset_calc_instr, mapping, Instruction::add, Type::i64);
}