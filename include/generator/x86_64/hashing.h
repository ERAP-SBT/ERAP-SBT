#pragma once

#include <iostream>
#include <ir/ir.h>
#include <math.h>

namespace generator::x86_64::hashing {
struct Hash {
    uint64_t h0;
    uint64_t h1;
    uint64_t h2;
    uint64_t key;

    Hash(uint64_t h0, uint64_t h1, uint64_t h2, uint64_t key) : h0(h0), h1(h1), h2(h2), key(key){};
};

struct Bucket {
    std::vector<Hash> stored_hashes;
};

// variation of the CHD algorithm described in http://cmph.sourceforge.net/papers/esa09.pdf "Hash, displace, and compress"
struct HashtableBuilder {
    uint32_t optimizations{0};
    enum Optimization : uint32_t { OPT_UNUSED_STATIC = 1 << 0, OPT_MBRA = 1 << 1, OPT_MERGE_OP = 1 << 2, OPT_ARCH_BMI2 = 1 << 3, OPT_NO_TRANS_BBS = 1 << 4 };

    float load_factor = 1.0;
    size_t bucket_size = 19;

    size_t hash_table_size{};
    size_t bucket_number{};

    std::vector<uint64_t> keys{};
    std::vector<Bucket> buckets{};
    std::vector<uint16_t> hash_idxs{};
    std::vector<uint64_t> hash_table{};

    const std::pair<uint64_t, uint64_t> seeds = {42, 0xbeef};

    HashtableBuilder() : hash_table_size(0), bucket_number(0){};
    HashtableBuilder(HashtableBuilder &){};
    ~HashtableBuilder(){};

    void fill(std::vector<uint64_t> &keys) {
        hash_table_size = std::floor(keys.size() / load_factor) + 1;
        bucket_number = std::floor(keys.size() / bucket_size) + 1;
        this->keys = keys;
    }

    bool build();
    void print_hash_table(FILE *out_fd, IR *ir);
    void print_hash_func_ids(FILE *out_fd);
    void print_hash_constants(FILE *out_fd) const;
    void print_ijump_lookup(FILE *out_fd) const;
    [[nodiscard]] std::array<uint64_t, 3> spookey_hash(uint64_t key) const;
};

} // namespace generator::x86_64::hashing
