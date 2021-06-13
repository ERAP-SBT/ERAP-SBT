#include "argument_parser.h"

Args::Args(const char **begin, const char **end) noexcept {
    for (auto i = begin; i != end; ++i) {
        std::string_view arg(*i);
        if (arg.length() >= 2 && arg[0] == '-' && arg[1] == '-') {
            arg.remove_prefix(2);
            if (arg.empty()) {
                continue;
            } else if (auto eq = arg.find('='); eq != std::string_view::npos) {
                arguments.insert({arg.substr(0, eq), arg.substr(eq + 1, arg.length() - eq)});
            } else {
                arguments.insert({std::move(arg), std::string_view()});
            }
        } else if (!arg.empty()) {
            positional.push_back(std::move(arg));
        }
    }
}

bool Args::get_value_as_bool(std::string_view arg) const noexcept {
    auto value = get_argument(arg);
    return value == "yes" || value == "on" || value == "true";
}
