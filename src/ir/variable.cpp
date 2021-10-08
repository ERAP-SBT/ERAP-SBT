#include "ir/variable.h"

#include "ir/ir.h"
#include "ir/operation.h"

#include <cassert>

void SSAVar::set_op(std::unique_ptr<Operation> &&ptr) {
    assert(info.index() == 0);
    info = std::move(ptr);
}

void SSAVar::print(std::ostream &stream, const IR *ir) const {
    print_type_name(stream, ir);

    switch (info.index()) {
    case 0: // not defined
        break;
    case 1: // immediate
        stream << " <- immediate " << std::get<1>(info).val;
        break;
    case 2: // static
        stream << " <- @" << ir->statics[std::get<2>(info)].id;
        break;
    case 3: // op
        stream << " <- ";
        std::get<3>(info)->print(stream, ir);
        break;
    }

    stream << " (" << ref_count;

    if (info.index() == 1 && std::get<1>(info).binary_relative) {
        stream << ", bin-rel";
    }

    stream << ")";
}

void SSAVar::print_type_name(std::ostream &stream, const IR *) const { stream << type << " v" << id; }

void StaticMapper::print(std::ostream &stream) const { stream << "static " << type << " @" << id << ";\n"; }
