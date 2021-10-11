#include <lifter/lifter.h>

using namespace lifter::RV64;

/* Jump tables are considered to be used by an ijump if the following operation
 * sequence is called: "x add a, b" -> "y load x, c" -> "x0 jalr y, 0"
 */
bool Lifter::is_jump_table_jump(const BasicBlock *bb, CfOp &cf_op, const RV64Inst &instr, const Program *prog) {
    // exclude unwanted jumps, e.g. direct jumps, branches or returns.
    if ((cf_op.type != CFCInstruction::ijump && cf_op.type != CFCInstruction::icall) || instr.instr.rs1 == LINK_IDX_1 || instr.instr.rs1 == LINK_IDX_2 || instr.instr.imm != 0) {
        return false;
    }

    // get the actual jump address before all the sign extending, adding and anding.
    SSAVar *loaded_addr_var = cf_op.in_vars[0].get();

    // reverse the "and" operation
    if (!std::holds_alternative<std::unique_ptr<Operation>>(loaded_addr_var->info)) {
        return false;
    }
    loaded_addr_var = std::get<std::unique_ptr<Operation>>(loaded_addr_var->info)->in_vars[0].get();

    // reverse the "add" operation
    if (!std::holds_alternative<std::unique_ptr<Operation>>(loaded_addr_var->info)) {
        return false;
    }
    loaded_addr_var = std::get<std::unique_ptr<Operation>>(loaded_addr_var->info)->in_vars[0].get();

    // reverse the "signextend" operation (if exists)
    if (!std::holds_alternative<std::unique_ptr<Operation>>(loaded_addr_var->info)) {
        return false;
    }
    auto &check_op = std::get<std::unique_ptr<Operation>>(loaded_addr_var->info);
    if (check_op->type == Instruction::sign_extend) {
        loaded_addr_var = check_op->in_vars[0].get();
    }
    if (!std::holds_alternative<std::unique_ptr<Operation>>(loaded_addr_var->info)) {
        return false;
    }
    auto &loaded_addr_op = std::get<std::unique_ptr<Operation>>(loaded_addr_var->info);

    // test if the operation is a load
    if (loaded_addr_op->type != Instruction::load) {
        return false;
    }

    // test if the load address is the result of an operation
    auto &jt_addr_var = loaded_addr_op->in_vars[0];
    if (!std::holds_alternative<std::unique_ptr<Operation>>(jt_addr_var->info)) {
        return false;
    }

    // test if the load address is the result of an add operation
    auto &jt_addr_op = std::get<std::unique_ptr<Operation>>(jt_addr_var->info);
    if (jt_addr_op->type != Instruction::add) {
        return false;
    }

    // one of these two is the jump table start address, which was loaded using a lui + addi
    std::deque<SSAVar *> unprocessed;
    unprocessed.emplace_back(jt_addr_op->in_vars[0]);
    unprocessed.emplace_back(jt_addr_op->in_vars[1]);
    uint64_t jt_start_addr = 0;
    uint64_t jt_end_addr = 0;

    while (!unprocessed.empty()) {
        SSAVar *next = unprocessed.front();
        unprocessed.pop_front();

        if (next && std::holds_alternative<std::unique_ptr<Operation>>(next->info)) {
            auto &op = std::get<std::unique_ptr<Operation>>(next->info);

            // find the addi instruction with the corresponding lui for jump table address loading
            if (op->type == Instruction::add && std::holds_alternative<SSAVar::ImmInfo>(op->in_vars[1]->info) && std::holds_alternative<SSAVar::ImmInfo>(op->in_vars[0]->info)) {
                auto addi_imm = std::get<SSAVar::ImmInfo>(op->in_vars[1]->info).val & 0xfff;
                auto lui_imm = std::get<SSAVar::ImmInfo>(op->in_vars[0]->info).val & 0xfffff000;
                jt_start_addr = addi_imm | lui_imm;
                break;
            }

            for (auto &pair : op->in_vars) {
                unprocessed.emplace_back(pair.get());
            }
        }
    }

    // no matching jump table start address loading found
    if (jt_start_addr == 0) {
        return false;
    }

    // the switch-condition integer is multiplied by this value to select the 4-byte wide addresses.
    constexpr int64_t addr_step = 4;

    // find upper bound for jump table (might not succeed)
    // manually get basic block before current one because predecessors aren't yet resolved
    BasicBlock *pred = get_bb(bb->virt_start_addr - 4);
    if (pred && pred->control_flow_ops[0].type == CFCInstruction::cjump) {
        auto &cf_op = pred->control_flow_ops[0];
        for (SSAVar *in_var : cf_op.in_vars) {
            if (in_var && std::holds_alternative<SSAVar::ImmInfo>(in_var->info)) {
                jt_end_addr = jt_start_addr + addr_step * std::get<SSAVar::ImmInfo>(in_var->info).val;
                break;
            }
        }
    }

    // map jump table start and end address to indices in the prog->addr and prog->data array.
    auto addr_start_idx = std::distance(prog->addrs.begin(), std::lower_bound(prog->addrs.begin(), prog->addrs.end(), jt_start_addr));

    auto next_addr = [prog, addr_step](size_t idx) -> uint64_t {
        uint64_t value_at_addr = 0;
        for (size_t i = 0; i < addr_step; ++i) {
            // exit address joining if the entered start address is obviously incorrect. Returning 0 will completely stop the jump table parsing.
            if (idx + i > prog->data.size() || !std::holds_alternative<uint8_t>(prog->data[idx + i])) {
                return 0;
            }
            value_at_addr |= std::get<uint8_t>(prog->data[idx + i]) << (i * 8);
        }
        return value_at_addr;
    };

    for (size_t i = addr_start_idx;; i += addr_step) {
        if (jt_end_addr && prog->addrs[i] >= jt_end_addr) {
            break;
        }
        uint64_t value_at_addr = next_addr(i);
        if (value_at_addr >= ir->virt_bb_start_addr && value_at_addr <= ir->virt_bb_end_addr) {
            needs_bb_start[(value_at_addr - ir->virt_bb_start_addr) / 2] = true;
        } else {
            break;
        }
    }

    std::get<CfOp::LifterInfo>(cf_op.lifter_info).jump_addr = next_addr(addr_start_idx);
    return true;
}
