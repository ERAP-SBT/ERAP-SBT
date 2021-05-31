#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * A simple argument parser.
 * Key-value arguments must be separated using '=' (equal sign).
 * Values stored in this structure are views into the source argument array.
 */
struct Args {
    /** A list of positional arguments (i.e. arguments without dashes) */
    std::vector<std::string_view> positional;
    /** Key-value arguments. The value can be empty. */
    std::unordered_map<std::string_view, std::string_view> arguments;

    /**
     * @param begin A pointer to the first argument.
     * @param end   A pointer pointing to one element past the last argument.
     */
    Args(const char **begin, const char **end) noexcept;

    bool has_argument(std::string_view arg) const noexcept { return arguments.find(arg) != arguments.end(); }
    std::string_view get_argument(std::string_view arg) const noexcept {
        auto entry = arguments.find(arg);
        return entry != arguments.end() ? entry->second : "";
    }
};
