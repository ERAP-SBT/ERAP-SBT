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
    dummy = ir->add_basic_block(0);
    uint64_t start_addr = prog->elf_base->header.e_entry;
    Function *curr_fun = ir->add_func();

    for (size_t i = 0; i < 32; i++) {
        ir->add_static(Type::i64);
    }
    // add the memory token as the last static slot
    ir->add_static(Type::mt);

    BasicBlock *first_bb = ir->add_basic_block(start_addr);
    liftRec(prog, curr_fun, start_addr, first_bb);
}

BasicBlock *Lifter::get_bb(uint64_t addr) const {
    for (auto &bb_ptr : ir->basic_blocks) {
        if (bb_ptr->virt_start_addr == addr) {
            return bb_ptr.get();
        }
    }
    return nullptr;
}

void Lifter::liftRec(Program *prog, Function *func, uint64_t start_addr, BasicBlock *curr_bb) {
    // for now: load the next instructions at the current symbol and assume always hit symbols or jump into sections which were already loaded
    for (size_t i = 0; i < prog->elf_base->symbols.size(); i++) {
        if (prog->elf_base->symbols.at(i).st_value == start_addr) {
#ifdef DEBUG
            std::stringstream str;
            str << "Continuing at symbol <" << prog->elf_base->symbol_names.at(i);
            str << "> at address <0x" << std::hex << start_addr << ">";
            DEBUG_LOG(str.str());
#endif
            prog->load_symbol_instrs(&prog->elf_base->symbols.at(i));
            break;
        }
    }

    // search for the address index in the program vectors
    auto it = std::find(prog->addrs.begin(), prog->addrs.end(), start_addr);
    if (it == prog->addrs.end()) {
        throw std::invalid_argument("Couldn't find parsed instructions at given address.");
    }
    size_t start_i = std::distance(prog->addrs.begin(), it);

    // init register mapping array
    reg_map mapping;

    // load ssa variables from static vars and fill mapping
    for (size_t i = 0; i < 33; i++) {
        mapping.at(i) = curr_bb->add_var_from_static(i);
    }

    // for now, we stop after 10000 instructions / data elements in one basic block
    for (size_t i = start_i; i < start_i + 10000 && i < prog->addrs.size(); i++) {
        if (prog->data.at(i).index() == 1) {
            RV64Inst instr = std::get<RV64Inst>(prog->data.at(i));
#ifdef DEBUG
            std::stringstream str;
            str << std::hex << prog->addrs.at(i) << ": " << str_decode_instr(&instr.instr);
            DEBUG_LOG(str.str());
#endif
            // the next_addr is used for CfOps which require a return address / address of the next instruction
            uint64_t next_addr;
            // test if the next address is outside of the already parsed addresses
            if (i < prog->addrs.size() - 1) {
                next_addr = prog->addrs.at(i + 1);
            } else {
                next_addr = prog->addrs.at(i) + 2;
            }
            parse_instruction(instr, curr_bb, mapping, prog->addrs.at(i), next_addr);
            if (!curr_bb->control_flow_ops.empty()) {
                curr_bb->virt_end_addr = prog->addrs.at(i);
                break;
            }
        }
    }

    for (CfOp &cfOp : curr_bb->control_flow_ops) {
        // TODO: test for 4 byte aligned address
        uint64_t jmp_addr = cfOp.jump_addr;
        BasicBlock *next_bb;

        if (jmp_addr == 0) {
            auto addr = backtrace_jmp_addr(&cfOp, curr_bb);
            std::cerr << "Encountered indirect jump / unknown jump target. Trying to guess the correct jump address...";
            if (!addr.has_value()) {
                std::cerr << " -> Address backtracking failed, skipping branch.\n";
                next_bb = dummy;
            } else {
                std::cerr << " -> Found a possible address: " << addr.value() << "\n";
                jmp_addr = addr.value();
                next_bb = get_bb(jmp_addr);
            }
        } else {
            next_bb = get_bb(jmp_addr);
        }

        bool bb_exists = false;
        if (next_bb == nullptr) {
            next_bb = ir->add_basic_block(jmp_addr);
            func->add_block(next_bb);
        } else {
            bb_exists = true;
            DEBUG_LOG("Encountered jump to basic block which was already parsed.");
        }

        curr_bb->successors.push_back(next_bb);
        next_bb->predecessors.push_back(curr_bb);
        cfOp.target = next_bb;
        cfOp.source = curr_bb;

        for (size_t i = 0; i < mapping.size(); i++) {
            auto var = mapping.at(i);
            if (var != nullptr) {
                cfOp.add_target_input(var);
                var->from_static = true;
                var->static_idx = i;
            }
        }

        if (bb_exists || next_bb == dummy) {
            continue;
        }
        liftRec(prog, func, jmp_addr, next_bb);
    }
}

// TODO: create function which splits a BasicBlock. Used when jumping into an existing basic block.
// TODO: Start a new basic block with each defined symbol

void Lifter::parse_instruction(RV64Inst instr, BasicBlock *bb, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    std::vector<std::pair<uint64_t, CfOp>> jump_goals;

    switch (instr.instr.mnem) {
    case FRV_INVALID:
        liftInvalid(bb, ip);
        break;
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
    case FRV_AUIPC:
        liftAUIPC(bb, instr, mapping, ip);
        break;
    case FRV_LUI:
        liftLUI(bb, instr, mapping);
        break;
    case FRV_JAL:
        liftJAL(bb, instr, mapping, ip, next_addr);
        break;
    case FRV_JALR:
        liftJALR(bb, instr, mapping, next_addr);
        break;
    case FRV_BEQ:
    case FRV_BNE:
    case FRV_BLT:
    case FRV_BGE:
    case FRV_BLTU:
    case FRV_BGEU:
        liftBranch(bb, instr, mapping, ip, next_addr);
        break;
    case FRV_ECALL:
        // this also includes the EBREAK
        liftECALL(bb, next_addr);
        break;
    default:
        char instr_str[16];
        frv_format(&instr.instr, 16, instr_str);
        std::cerr << "Encountered invalid instruction during lifting: " << instr_str << "\n";
    }
}

void Lifter::liftInvalid(BasicBlock *bb, uint64_t ip) {
    std::cerr << "Encountered invalid instruction during lifting. (BasicBlock #0x" << std::hex << bb->id << ", address <0x" << std::hex << ip << ">)\n";
}

void Lifter::lift_load(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Type &op_size, bool sign_extend) {
    // 1. load offset
    SSAVar *offset = load_immediate(bb, instr.instr.imm);
    // 3. add offset to rs1
    SSAVar *load_addr = bb->add_var(Type::i64);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(mapping.at(instr.instr.rs1), offset);
        add_op->set_outputs(load_addr);
        load_addr->set_op(std::move(add_op));
    }

    // create SSAVariable for the destination operand
    SSAVar *load_dest = bb->add_var(op_size);

    // create the load operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::load);

    operation->set_inputs(load_addr, mapping.at(MEM_IDX));
    operation->set_outputs(load_dest);

    // assign the operation as variable of the destination
    load_dest->set_op(std::move(operation));

    // last step: extend load_dest variable to 64 bit
    SSAVar *extended_result = bb->add_var(Type::i64);
    {
        auto extend_operation = std::make_unique<Operation>((sign_extend ? Instruction::sign_extend : Instruction::zero_extend));
        extend_operation->set_inputs(load_dest);
        extend_operation->set_outputs(extended_result);
        extended_result->set_op(std::move(extend_operation));
    }

    // write SSAVar of the result of the operation and new memory token back to mapping
    mapping.at(instr.instr.rd) = extended_result;
}

void Lifter::lift_store(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Type &op_size) {
    // 1. load offset
    SSAVar *offset = load_immediate(bb, instr.instr.imm);
    // 3. add offset to rs1
    SSAVar *store_addr = bb->add_var(Type::i64);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(mapping.at(instr.instr.rs1), offset);
        add_op->set_outputs(store_addr);
        store_addr->set_op(std::move(add_op));
    }

    // cast variable to store to operand size
    SSAVar *store_var = shrink_var(bb, mapping.at(instr.instr.rs2), op_size);

    // create memory_token
    SSAVar *result_memory_token = bb->add_var(Type::mt);

    // create the store operation
    std::unique_ptr<Operation> operation = std::make_unique<Operation>(Instruction::store);

    // set in- and outputs
    operation->set_inputs(store_var, store_addr, mapping.at(MEM_IDX));
    operation->set_outputs(result_memory_token);

    // set operation
    result_memory_token->set_op(std::move(operation));

    // write memory_token back
    mapping.at(MEM_IDX) = result_memory_token;
}

void Lifter::lift_shift(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, const Instruction &instruction_type, const Type &op_size) {
    // prepare for shift, only use lower 5bits

    SSAVar *mask;

    // cast immediate from 64bit to 32bit if instruction has 32bit size
    if (op_size == Type::i32) {
        mask = load_immediate(bb, (int32_t)0x1F);
    } else {
        mask = load_immediate(bb, (int64_t)0x1F);
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
    // create immediate var
    SSAVar *immediate;
    if (op_size == Type::i32) {
        immediate = load_immediate(bb, instr.instr.imm);
    } else {
        immediate = load_immediate(bb, (int64_t)instr.instr.imm);
    }

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

SSAVar *Lifter::shrink_var(BasicBlock *bb, SSAVar *var, const Type &target_size) {
    // create cast operation
    std::unique_ptr<Operation> cast = std::make_unique<Operation>(Instruction::cast);

    // set in- and outputs
    cast->set_inputs(var);

    // create casted variable
    SSAVar *destination = bb->add_var(target_size);
    cast->set_outputs(destination);
    destination->set_op(std::move(cast));

    return destination;
}

void Lifter::liftAUIPC(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip) {
    // 1. load the immediate
    SSAVar *immediate = load_immediate(bb, instr.instr.imm);
    // 2. load instruction pointer as immediate
    SSAVar *ip_immediate = load_immediate(bb, (int64_t)ip);

    // 3. add immediate to instruction pointer
    SSAVar *result = bb->add_var(Type::i64);
    {
        auto add_op = std::make_unique<Operation>(Instruction::add);
        add_op->set_inputs(ip_immediate, immediate);
        add_op->set_outputs(result);
        result->set_op(std::move(add_op));
    }
    // write SSAVar back to mapping
    mapping.at(instr.instr.rd) = result;
}

void Lifter::liftLUI(BasicBlock *bb, RV64Inst &instr, reg_map &mapping) {
    // create the immediate loading operation (with built-in sign extension)
    SSAVar *immediate = load_immediate(bb, (int64_t)instr.instr.imm);

    // write SSAVar back to mapping
    mapping.at(instr.instr.rd) = immediate;
}

SSAVar *Lifter::load_immediate(BasicBlock *bb, int32_t imm) {
    SSAVar *input_imm = bb->add_var_imm(imm);
    return input_imm;
}

SSAVar *Lifter::load_immediate(BasicBlock *bb, int64_t imm) {
    SSAVar *input_imm = bb->add_var_imm(imm);
    return input_imm;
}

void Lifter::liftJAL(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // 1. load the immediate from the instruction (with built-in sign extension)
    SSAVar *jmp_imm = load_immediate(bb, (int64_t)instr.instr.imm);

    // 2. the original immediate is encoded in multiples of 2 bytes, but frvdec already took of that for us.

    // 3. load IP
    SSAVar *ip_imm = load_immediate(bb, (int64_t)ip);

    // 4. add offset to ip
    SSAVar *sum = bb->add_var(Type::i64);
    {
        auto addition = std::make_unique<Operation>(Instruction::add);
        addition->set_inputs(ip_imm, jmp_imm);
        addition->set_outputs(sum);
        sum->set_op(std::move(addition));
    }

    // 5. load return address as another immediate
    SSAVar *return_addr = load_immediate(bb, (int64_t)next_addr);

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = return_addr;

    // 6. jump!
    // create the jump operation
    CfOp &cf_operation = bb->add_cf_op(CFCInstruction::jump, instr.instr.imm + ip);

    // set operation in- and outputs
    cf_operation.set_inputs(sum);
}

void Lifter::liftJALR(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t next_addr) {
    // the address is encoded as an immediate offset....
    // 1. load the immediate offset
    SSAVar *immediate = load_immediate(bb, (int64_t)instr.instr.imm);

    // 2. add the offset register (the jalR-specific part)
    SSAVar *offset_register = mapping.at(instr.instr.rs1);

    SSAVar *sum = bb->add_var(Type::i64);
    {
        auto addition = std::make_unique<Operation>(Instruction::add);
        addition->set_inputs(offset_register, immediate);
        addition->set_outputs(sum);
        sum->set_op(std::move(addition));
    }

    // 3. set lsb to zero (every valid rv64 instruction is at least 2 byte aligned)
    // 3.1 load bitmask
    SSAVar *bit_mask = load_immediate(bb, (int64_t)-2);
    // 3.2 apply mask
    SSAVar *jump_addr = bb->add_var(Type::i64);
    {
        auto and_op = std::make_unique<Operation>(Instruction::_and);
        and_op->set_inputs(sum, bit_mask);
        and_op->set_outputs(jump_addr);
        jump_addr->set_op(std::move(and_op));
    }

    // create the jump operation
    CfOp &cf_operation = bb->add_cf_op(CFCInstruction::ijump, (uint64_t)0);

    // set operation in- and outputs
    cf_operation.set_inputs(jump_addr);

    // the return value address is encoded as immediate
    SSAVar *return_immediate = load_immediate(bb, (int64_t)next_addr);

    // write SSAVar of the result of the operation back to mapping
    mapping.at(instr.instr.rd) = return_immediate;
}

void Lifter::liftBranch(BasicBlock *bb, RV64Inst &instr, reg_map &mapping, uint64_t ip, uint64_t next_addr) {
    // 1. load the immediate from the instruction
    SSAVar *jmp_imm = load_immediate(bb, (int64_t)instr.instr.imm);

    // 2. this immediate is originally encoded in multiples of 2 bytes, but is already converted by frvdec

    // 3. load IP
    SSAVar *ip_imm = load_immediate(bb, (int64_t)ip);

    // 4. add offset to ip
    SSAVar *jmp_addr = bb->add_var(Type::i64);
    {
        auto addition = std::make_unique<Operation>(Instruction::add);
        addition->set_inputs(ip_imm, jmp_imm);
        addition->set_outputs(jmp_addr);
        jmp_addr->set_op(std::move(addition));
    }
    using JInfo = CfOp::CJumpInfo;

    // ((rs1 == rs2) ? continue : jmp to addr) := true
    // ((rs1 == rs2) ? jmp to addr : continue) := false
    bool reverse_jumps = false;

    // parse the branching instruction -> jumps if the condition is true
    CfOp &c_jmp = bb->add_cf_op(CFCInstruction::cjump);

    switch (instr.instr.mnem) {
    case FRV_BEQ:
        // BEQ: (rs1 == rs2) ? jmp to addr : continue
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::eq}));
        break;
    case FRV_BNE:
        // BNE: (rs1 == rs2) ? continue : jmp to addr
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::eq}));
        reverse_jumps = true;
        break;
    case FRV_BLT:
        // BLT: (rs1 < rs2) ? jmp to addr : continue
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::lt}));
        break;
    case FRV_BGE:
        // BGE: (rs1 < rs2) ? continue : jmp to addr
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::lt}));
        reverse_jumps = true;
        break;
    case FRV_BLTU:
        // BLTU: (rs1 <u rs2) ? jmp to addr : continue
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::ltu}));
        break;
    case FRV_BGEU:
        // BGEU: (rs1 <u rs2) ? continue : jmp to addr
        c_jmp.info.emplace<JInfo>(JInfo({JInfo::CJumpType::ltu}));
        reverse_jumps = true;
        break;
    }

    uint64_t encoded_addr = (int64_t)(instr.instr.imm) + ip;
    SSAVar *next_addr_var = load_immediate(bb, (int64_t)next_addr);

    // stores the address which is used if the branch condition is false
    uint64_t uc_jmp_addr = reverse_jumps ? encoded_addr : next_addr;
    SSAVar *uc_jmp_addr_var = reverse_jumps ? jmp_addr : next_addr_var;

    // stores the address which is used if the branch condition is true
    uint64_t br_jmp_addr = reverse_jumps ? next_addr : encoded_addr;
    SSAVar *br_jmp_addr_var = reverse_jumps ? next_addr_var : jmp_addr;

    SSAVar *rs1 = mapping.at(instr.instr.rs1);
    SSAVar *rs2 = mapping.at(instr.instr.rs2);
    c_jmp.set_inputs(rs1, rs2, br_jmp_addr_var);
    c_jmp.jump_addr = br_jmp_addr;

    // Branch not taken -> like JAL, but doesn't write return address to register
    CfOp &continue_jmp = bb->add_cf_op(CFCInstruction::jump, uc_jmp_addr);
    continue_jmp.set_inputs(uc_jmp_addr_var);
}

void Lifter::liftECALL(BasicBlock *bb, uint64_t next_addr) {
    // the behavior of the ECALL instruction is system dependant. (= SYSCALL)
    // we give the syscall the address at which the program control flow continues (= next basic block)
    bb->add_cf_op(CFCInstruction::syscall, next_addr);
}

std::optional<SSAVar *> Lifter::get_last_static_assignment(size_t idx, BasicBlock *bb) {
    std::vector<SSAVar *> possible_preds;
    for (BasicBlock *pred : bb->predecessors) {
        for (const CfOp &cfo : pred->control_flow_ops) {
            for (SSAVar *ti : cfo.target_inputs) {
                if (ti->from_static && ti->static_idx == idx) {
                    possible_preds.push_back(ti);
                }
            }
        }
    }
    if (possible_preds.empty()) {
        DEBUG_LOG("No predecessors for static variable were found in predecessor list of basic block.");
        return std::nullopt;
    } else if (possible_preds.size() > 1) {
        DEBUG_LOG("Warning: found multiple possible statically mapped variables. Aborting.");
        return std::nullopt;
    }
    return possible_preds.at(0);
}

std::optional<int64_t> Lifter::get_var_value(SSAVar *var, BasicBlock *bb) {
    if (var->info.index() == 1) {
        return std::get<int64_t>(var->info);
    } else if (var->from_static) {
        auto opt_var = get_last_static_assignment(var->static_idx, bb);
        if (opt_var.has_value()) {
            var = opt_var.value();
        } else {
            return std::nullopt;
        }
    }
    if (var->info.index() != 2) {
        // TODO: this could produce parsing errors
        return std::nullopt;
    }
    Operation *op = std::get<std::unique_ptr<Operation>>(var->info).get();

    std::vector<int64_t> resolved_vars;
    for (auto &in_var : op->in_vars) {
        if (in_var != nullptr) {
            auto res = get_var_value(in_var, bb);
            if (!res.has_value()) {
                return std::nullopt;
            }
            resolved_vars.push_back(res.value());
        }
    }

    switch (op->type) {
    case Instruction::add:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] + resolved_vars[1];
    case Instruction::sub:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] - resolved_vars[1];
    case Instruction::immediate:
        if (resolved_vars.size() != 1)
            return std::nullopt;
        return resolved_vars[0];
    case Instruction::shl:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] << resolved_vars[1];
    case Instruction::_or:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] | resolved_vars[1];
    case Instruction::_and:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] & resolved_vars[1];
    case Instruction::_not:
        if (resolved_vars.size() != 1)
            return std::nullopt;
        return ~resolved_vars[0];
    case Instruction::_xor:
        if (resolved_vars.size() != 2)
            return std::nullopt;
        return resolved_vars[0] ^ resolved_vars[1];
    case Instruction::sign_extend:
        // currently, only 32-bit to 64-bit sign extension is supported
        if (op->in_vars[0]->type != Type::i32 || var->type != Type::i64) {
            return std::nullopt;
        }
        return (int64_t)resolved_vars[0];
    default:
        return std::nullopt;
    }
}

std::optional<uint64_t> Lifter::backtrace_jmp_addr(CfOp *op, BasicBlock *bb) {
    if (op->type != CFCInstruction::ijump) {
        std::cerr << "Jump address backtracing is currently only supported for indirect, JALR jumps." << std::endl;
        return std::nullopt;
    }
    return get_var_value(op->in_vars[0], bb);
}
