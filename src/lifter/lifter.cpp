#include "lifter/lifter.h"

#include <frvdec.h>

using namespace lifter::RV64;

void Lifter::lift(Program *prog) {
    uint64_t start_addr = prog->elf_base->header.e_entry;

    Function *curr_fun = ir->add_func();
    BasicBlock *curr_bb = ir->add_basic_block(start_addr);

    for (size_t i = 0; i < prog->elf_base->program_headers.size(); i++) {
        if (prog->elf_base->program_headers.at(i).p_type == PT_LOAD) {
            for (Elf64_Shdr *sec : prog->elf_base->segment_section_map.at(i)) {
                prog->load_section(sec);
            }
        }
    }

    uint64_t curr_addr = start_addr;
    for (size_t i = 0; i < 100; i++) {
        auto mem = prog->memory.find(curr_addr);
        if (mem != prog->memory.end()) {
            if (mem->second.index() == 1) {
                auto &instr = std::get<RV64Inst>(mem->second);
                parse_instruction(instr, curr_fun, curr_bb);
            }
        }
        curr_addr++;
    }
}

BasicBlock* Lifter::get_bb(uint64_t addr) const {
    for (auto& bb_ptr : ir->basic_blocks) {
        if (bb_ptr->virt_addr == addr) {
            return bb_ptr.get();
        }
    }
    return nullptr;
}

void Lifter::liftRec(Program *prog, Function* func, uint64_t start_addr, BasicBlock* pred) {
    {
        BasicBlock* bb = get_bb(start_addr);
        if (bb != nullptr) {
            bb->predecessors.push_back(pred);
        }
        return;
    }
    std::array<SSAVar*, 32> mapping{};
    std::unique_ptr<BasicBlock> bb = std::make_unique<BasicBlock>(ir, next_id++, start_addr);

    // TODO: get this from the previous basic block
    SSAVar memory_token(next_var_id++, Type::mt);

    while (true) {
        RV64Inst instr = prog->memory.at(start_addr);
        parse_instruction(instr, func, bb.get(), mapping, &memory_token);
        // TODO: increment address
        // TODO: detect control flow change
    }
}

// TODO: create function which splits a BasicBlock. Used when jumping into an existing basic block.
// TODO: Start a new basic block with each defined symbol

void Lifter::parse_instruction(RV64Inst instr, Function *fun, BasicBlock *bb, std::array<SSAVar*, 32>& mapping, SSAVar* memory_token) {
    switch (instr.instr.mnem) {
        case FRV_INVALID:
            liftInvalid(func, bb.get());
            break;
        case FRV_LB:
        case FRV_LH:
        case FRV_LW:
        case FRV_LD:
        case FRV_LBU:
        case FRV_LHU:
        case FRV_LWU:
            liftLoad(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SB:
        case FRV_SH:
        case FRV_SW:
        case FRV_SD:
            liftStore(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ADD:
            liftAdd(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ADDW:
            liftAddW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ADDI:
            liftAddI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ADDIW:
            liftAddIW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLLI:
            liftSLLI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLLIW:
            liftSLLIW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLTI:
            liftSLTI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLTIU:
            liftSLTIU(func, bb.get(), mapping, memory_token);
            break;
        case FRV_XORI:
            liftXORI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRAI:
            liftSRAI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRAIW:
            liftSRAIW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRLI:
            liftSRLI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRLIW:
            liftSRLIW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ORI:
            liftORI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ANDI:
            liftANDI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLL:
            liftSLL(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLLW:
            liftSLLW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLT:
            liftSLT(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SLTU:
            liftSLTU(func, bb.get(), mapping, memory_token);
            break;
        case FRV_XOR:
            liftXOR(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRL:
            liftSRL(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRLW:
            liftSRLW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_OR:
            liftOR(func, bb.get(), mapping, memory_token);
            break;
        case FRV_AND:
            liftAND(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SUB:
            liftSUB(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SUBW:
            liftSUBW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRA:
            liftSRA(func, bb.get(), mapping, memory_token);
            break;
        case FRV_SRAW:
            liftSRAW(func, bb.get(), mapping, memory_token);
            break;
        case FRV_FENCE:
            liftFENCE(func, bb.get(), mapping, memory_token);
            break;
        case FRV_FENCEI:
            liftFENCEI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_AUIPC:
            liftAUIPC(func, bb.get(), mapping, memory_token);
            break;
        case FRV_LUI:
            liftLUI(func, bb.get(), mapping, memory_token);
            break;
        case FRV_JAL:
            liftJAL(func, bb.get(), mapping, memory_token);
            break;
        case FRV_JALR:
            liftJALR(func, bb.get(), mapping, memory_token);
            break;
        case FRV_BEQ:
        case FRV_BNE:
        case FRV_BLT:
        case FRV_BGE:
        case FRV_BLTU:
        case FRV_BGEU:
            liftBranch(func, bb.get(), mapping, memory_token);
            break;
        case FRV_ECALL:
            liftECALL(func, bb.get(), mapping, memory_token);
            break;
        default:
            char instr_str[16];
            frv_format(&inst.instr, 16, instr_str);
            std::cerr << "Encountered invalid instruction during lifting: " << instr_str << "\n";
    }
}

void Lifter::liftAdd(Function *, BasicBlock *, std::array<SSAVar*, 32>& mapping) {

}
