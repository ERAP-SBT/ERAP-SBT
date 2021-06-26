#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

struct File {
    FILE *file;

    explicit File(FILE *f) : file(f) {}
    ~File() noexcept {
        if (file) {
            fclose(file);
        }
    }

    File(File &) = delete;
    File &operator=(File &) = delete;

    File(File &&other) : file(std::exchange(other.file, nullptr)) {}
    File &operator=(File &&other) {
        file = std::exchange(other.file, nullptr);
        return *this;
    }

    constexpr FILE *handle() { return file; }
};

struct Buffer {
    char *buf{nullptr};
    size_t size{0};

    ~Buffer() noexcept {
        if (buf) {
            free(buf);
            buf = nullptr;
        }
    }

    File open() {
        FILE *file = open_memstream(&buf, &size);
        if (!file) {
            fprintf(stderr, "FATAL ERROR: Could not open memory stream: %s\n", std::strerror(errno));
            abort();
        }

        return File(file);
    }

    constexpr std::string_view view() const { return std::string_view(buf, size); }
};
