#include "../../include/ir/variable.h"
#include "ir/operation.h"

void Variable::print_name_type(std::ostream &stream) const
{
	stream << type << " v" << id;
}

void Variable::print(std::ostream &stream) const
{
	stream << type << " v" << id << " <- ";
	if (operation)
	{
		operation->print(stream);
	} else
	{
		// immediate
		stream << "immediate " << immediate;
	}
	stream << "\n";
}

void StaticMapper::print(std::ostream &stream) const {
    stream << "static " << type << " " << name << ";\n";
}
