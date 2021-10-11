#include <common/internal.h>
#include <generator/x86_64/hashing.h>

using namespace generator::x86_64::hashing;

bool HashtableBuilder::build() {
    buckets.clear();
    buckets.reserve(bucket_number);

    hash_idxs.clear();
    hash_idxs.reserve(bucket_number);
    for (size_t i = 0; i < bucket_number; ++i) {
        buckets.emplace_back(Bucket());
        hash_idxs.emplace_back(0);
    }

    hash_table.clear();
    hash_table.reserve(hash_table_size);
    for (size_t i = 0; i < hash_table_size; ++i) {
        hash_table.emplace_back(0);
    }

    for (auto key : keys) {
        auto hashes = spookey_hash(key);
        buckets[hashes[0]].stored_hashes.emplace_back(hashes[0], hashes[1], hashes[2], key);
    }

    // sort buckets by number of items, descending
    std::sort(buckets.begin(), buckets.end(), [](const Bucket &b1, const Bucket &b2) { return b1.stored_hashes.size() > b2.stored_hashes.size(); });

    std::vector<bool> occupied_bins(hash_table_size);
    std::fill(occupied_bins.begin(), occupied_bins.end(), false);

    for (auto &bucket : buckets) {
        if (bucket.stored_hashes.empty()) {
            continue;
        }

        // apply hashing scheme described in "Hash, displace, and compress" (Appendix: A Practical Version)
        size_t d0 = 0;
        size_t d1 = 0;

        // for storage efficient storage of hash function
        uint16_t combination_idx = 0;

        while ((d0 < hash_table_size || d1 < hash_table_size) && (combination_idx < UINT16_MAX)) {
            std::vector<size_t> valid_idxs;
            for (auto &hash : bucket.stored_hashes) {
                size_t hashed_idx = (hash.h1 + d0 * hash.h2 + d1) % hash_table_size;
                if (occupied_bins[hashed_idx]) {
                    // revert false inserts
                    for (size_t idx : valid_idxs) {
                        occupied_bins[idx] = false;
                        hash_table[idx] = 0;
                    }
                    break;
                } else {
                    valid_idxs.emplace_back(hashed_idx);
                    occupied_bins[hashed_idx] = true;
                    hash_table[hashed_idx] = hash.key;
                }
            }

            if (valid_idxs.size() == bucket.stored_hashes.size()) {
                hash_idxs[bucket.stored_hashes.front().h0] = combination_idx;
                break;
            }

            d1++;
            combination_idx++;
            // "rollover" to d0 and clear d1
            if (d1 >= hash_table_size) {
                d1 = 0;
                d0++;
            }
        }

        if (hash_idxs[bucket.stored_hashes.front().h0] != combination_idx) {
            DEBUG_LOG("Unable to calculate valid hash function. Reducing load factor by 10%.");
            return false;
        }
    }
    return true;
}

// taken from https://www.burtleburtle.net/bob/c/SpookyV2.h
inline uint64_t rot_64(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

std::array<uint64_t, 3> HashtableBuilder::spookey_hash(uint64_t key) const {
    uint64_t h0 = seeds.first;
    uint64_t h1 = seeds.second;

    // "sc_const" = 0xdeadbeefdeadbeef
    uint64_t h2 = 0xdeadbeefdeadbeef;
    uint64_t h3 = 0xdeadbeefdeadbeef + key;

    h2 += ((uint64_t)8) << 56;

    // ShortEnd from https://www.burtleburtle.net/bob/c/SpookyV2.h
    h3 ^= h2;
    h2 = rot_64(h2, 15);
    h3 += h2;
    h0 ^= h3;
    h3 = rot_64(h3, 52);
    h0 += h3;
    h1 ^= h0;
    h0 = rot_64(h0, 26);
    h1 += h0;
    h2 ^= h1;
    h1 = rot_64(h1, 51);
    h2 += h1;
    h3 ^= h2;
    h2 = rot_64(h2, 28);
    h3 += h2;
    h0 ^= h3;
    h3 = rot_64(h3, 9);
    h0 += h3;
    h1 ^= h0;
    h0 = rot_64(h0, 47);
    h1 += h0;
    h2 ^= h1;
    h1 = rot_64(h1, 54);
    h2 += h1;
    h3 ^= h2;
    h2 = rot_64(h2, 32);
    h3 += h2;
    h0 ^= h3;
    h3 = rot_64(h3, 25);
    h0 += h3;
    h1 ^= h0;
    h0 = rot_64(h0, 63);
    h1 += h0;

    // apply size constraints
    h0 %= bucket_number;
    h1 %= hash_table_size;
    h2 %= hash_table_size;
    return {h0, h1, h2};
}

void HashtableBuilder::print_hash_table(_IO_FILE *out_fd, IR *ir) {
    fprintf(out_fd, ".global ijump_hash_table\n");
    fprintf(out_fd, "ijump_hash_table:\n");
    for (uint64_t key : hash_table) {
        fprintf(out_fd, ".8byte 0x%lx\n", key);

        BasicBlock *bb_at_addr = ir->bb_at_addr(key);
        if (bb_at_addr != nullptr) {
            fprintf(out_fd, ".8byte b%zu\n", bb_at_addr->id);
        } else {
            fprintf(out_fd, ".8byte unresolved_ijump\n");
        }
    }
}

void HashtableBuilder::print_hash_func_ids(_IO_FILE *out_fd) {
    fprintf(out_fd, ".global ijump_hash_function_idxs\n");
    fprintf(out_fd, "ijump_hash_function_idxs:\n");
    for (uint16_t idx : hash_idxs) {
        fprintf(out_fd, ".word %hu\n", idx);
    }
}

void HashtableBuilder::print_hash_constants(_IO_FILE *out_fd) const {
    fprintf(out_fd, ".global ijump_hash_bucket_number\n");
    fprintf(out_fd, "ijump_hash_bucket_number:\n.quad %zu\n", bucket_number);

    fprintf(out_fd, ".global ijump_hash_table_size\n");
    fprintf(out_fd, "ijump_hash_table_size:\n.quad %zu\n", hash_table_size);
}

void HashtableBuilder::print_ijump_lookup(_IO_FILE *out_fd, bool print_call) const {
    if (print_call) {
        fprintf(out_fd, "icall_lookup:\n");
    } else {
        fprintf(out_fd, "ijump_lookup:\n");
    }

    // setup stack frame
    fprintf(out_fd, "push rax\npush rdx\npush rdi\npush rsi\npush rcx\n");
    fprintf(out_fd, "sub rsp, 32\n");

    // hash function input parameters
    fprintf(out_fd, "mov rsi, rbx\n");
    fprintf(out_fd, "mov rdi, rsp\n");

    fprintf(out_fd, "call spookey_hash\n");

    // load hash idx to rax (h0 at rbp-32)
    fprintf(out_fd, "mov rsi, [rsp]\n");
    fprintf(out_fd, "shl rsi, 1\n");
    fprintf(out_fd, "xor rax, rax\n");
    fprintf(out_fd, "mov ax, [rsi + ijump_hash_function_idxs]\n");

    // calculate d0 and d1 values and store into r8 and r9
    fprintf(out_fd, "xor rdx, rdx\n");
    fprintf(out_fd, "mov rdi, %zu\n", hash_table_size);
    fprintf(out_fd, "div rdi\n");
    fprintf(out_fd, "mov rcx, rdx\n");

    // calculate index in hash table: (hashes[1] + (d0 * hashes[2]) + d1) % builder.hash_table_size;
    fprintf(out_fd, "mov rsi, [rsp + 16]\n");
    fprintf(out_fd, "mul rsi\n");
    fprintf(out_fd, "add rax, rcx\n");
    fprintf(out_fd, "add rax, [rsp + 8]\n");
    fprintf(out_fd, "div rdi\n");
    fprintf(out_fd, "mov rax, rdx\n");

    // destroy stack frame in advance
    fprintf(out_fd, "add rsp, 32\n");
    fprintf(out_fd, "pop r9\npop rsi\npop rdi\n");

    // load address from hash table
    fprintf(out_fd, "shl rax, 4\n");
    fprintf(out_fd, "mov rdx, [ijump_hash_table + rax]\n");
    fprintf(out_fd, "cmp rdx, rbx\n");
    fprintf(out_fd, "pop rdx\n");
    fprintf(out_fd, "jne 0f\n");

    // reset rax for reg_alloc -> not for interpreter run
    fprintf(out_fd, "mov rbx, rax\npop rax\n");

    // jump to ijump entry point
    if (print_call) {
        fprintf(out_fd, "call [ijump_hash_table + rbx + 8]\n");
    } else {
        fprintf(out_fd, "jmp [ijump_hash_table + rbx + 8]\n");
    }

    // panic
    fprintf(out_fd, "0:\nmov rdi, rbx\njmp unresolved_ijump\n");
}
