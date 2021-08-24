#include "zip.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void print_help(const char *file_name);
int compress_files(const char *file_target, const char **files_to_compress, int count);
int decompress_files(const char **files_to_decompress, int count);

int main(int argc, char *argv[]) {
    if (argc == 1 || !strcmp(argv[0], "--help")) {
        print_help(argv[0]);
        return 0;
    }

    const char *file_target = 0;
    const char **files_to_compress = 0;
    int number_to_compress = 0;
    const char **files_to_decompress = 0;
    int number_to_decompress = 0;
    int extract_mode = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-e")) {
            extract_mode = 1;
            continue;
        }

        if (!extract_mode) {
            if (!file_target) {
                file_target = argv[i];
                continue;
            }
            if (!files_to_compress) {
                files_to_compress = argv + i;
            }
            number_to_compress++;
            continue;
        }

        if (!files_to_decompress) {
            files_to_decompress = argv + i;
        }
        number_to_decompress++;

        if (i + 1 >= argc) {
            printf("Error: Missing directory parameter for extraction\n");
            return 3;
        }
        i += 1;
    }

    if (file_target) {
        if (compress_files(file_target, files_to_compress, number_to_compress)) {
            printf("Error compressing. Exiting...\n");
            return 1;
        }
    }

    if (files_to_decompress) {
        if (decompress_files(files_to_decompress, number_to_decompress)) {
            printf("Error decompressing. Exiting...\n");
            return 2;
        }
    }

    return 0;
}

void print_help(const char *file_name) {
    printf("Simple ZIP-Utility\n");
    printf("Usage: \nTo compress and decompress: %s target.zip file1 file2 -e file_to_extract.zip file2.zip\n", file_name);
    printf("To only compress: %s target.zip file1 file2...\n", file_name);
    printf("To only decompress: %s -e file_to_extract.zip target_dir file2.zip target_dir2...\n");
}

int compress_files(const char *file_target, const char **files_to_compress, int count) {
    int res = zip_create(file_target, files_to_compress, count);
    if (res != 0) {
        printf("Error creating zip: %d\n", res);
        return 1;
    }
    chmod(file_target, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    return 0;
    /*struct zip_t* zip = zip_open(file_target, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (!zip) {
            printf("Error opening zip target\n");
            return 1;
    }

    for (int i = 0; i < count; ++i) {
            char* f_dup = strdup(files_to_compress[i]);
            char* base = basename(f_dup);
            int res = zip_entry_open(zip, base);
            if (res < 0) {
                    printf("Error opening zip entry '%s'(%s): %d\n", files_to_compress[i], base, res);
                    zip_close(zip);
                    remove(file_target);
                    return 1;
            }
            res = zip_entry_fwrite(zip, files_to_compress[i]);
            if (res < 0) {
                    printf("Error writing file '%s' to zip: %d\n", files_to_compress[i], res);
                    zip_entry_close(zip);
                    zip_close(zip);
                    remove(file_target);
                    return 1;
            }
            zip_entry_close(zip);
    }

    zip_close(zip);
    chmod(file_target, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR);
    return 0;*/
}

// from https://stackoverflow.com/a/9210960
int mkpath(char *file_path, mode_t mode) {
    assert(file_path && *file_path);
    for (char *p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

int decompress_files(const char **files_to_decompress, int count) {
    for (int i = 0; i < count; ++i) {
        int res = mkpath(files_to_decompress[i + 1], S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (res) {
            printf("Error creating directory '%s' for '%s': %d\n", files_to_decompress[i + 1], files_to_decompress[i], res);
            return 1;
        }
        res = zip_extract(files_to_decompress[i], files_to_decompress[i + 1], 0, 0);
        if (res) {
            printf("Error extracting '%s' to '%s': %d\n", files_to_decompress[i], files_to_decompress[i + 1], res);
            return 1;
        }
    }
    return 0;
}
