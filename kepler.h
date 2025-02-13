#ifndef KEPLER_H_
#define KEPLER_H_
// Definitions

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define KEP_ALLOC malloc
#define KEP_FREE free

#define KEP_COMPILER "clang"

// 100 MB
// im pretty sure this should be fine through malloc
// as not all of it will be committed (maybe?)
#define KEP_ARENA_SIZE 100 * 1024 * 1024

// https://github.com/tsoding/nob.h/blob/main/nob.h#L488
#ifndef SV_FMT
#define SV_FMT "%.*s"
#endif // SV_FMT

#ifndef SV_ARGS
#define SV_ARGS(sv) (int)sv.len, sv.items
#endif // SV_ARGS

typedef struct {
    void *buffer;
    ptrdiff_t cap;
    ptrdiff_t current_pos;
} kep_arena;

// String slice
typedef struct {
    char *items;
    size_t len;
} kep_str;

typedef struct {
    kep_str items;
    size_t len;
    size_t cap;
} kep_strs;

typedef struct {
    char **items;
    int len;
} kep_cstrs;

// Appendable string
typedef struct {
    char *items;
    size_t len;
    size_t cap;
} kep_da_str;

typedef enum {
    EXE,
    STATIC_LIB,
    DYN_LIB,
} kep_arti_type;

typedef struct {
    kep_cstrs sources;

    char *path;
    char *name;

    bool dynamic;
    bool system;
} kep_lib;

typedef struct {
    kep_lib *items;
    size_t len;
} kep_libs;

typedef struct {
    kep_cstrs sources;
    kep_cstrs include_dirs;
    kep_libs linked_libs;

    char *name;
} kep_exe;

// Runtime bounds checking
#define KEP_A_GET(array, index)                                                \
    (assert(index < array.len && "Array OOB"), array.items[index])

#define KEP_DA_INIT(arena, da, type, initial_cap)                              \
    do {                                                                       \
        type *data_ptr = kep_arena_push(arena, initial_cap * sizeof(type));    \
                                                                               \
        (da)->items = data_ptr;                                                \
        (da)->cap = initial_cap;                                               \
        (da)->len = 0;                                                         \
    } while (0)

#define KEP_DA_APPEND(arena, da, item)                                         \
    do {                                                                       \
        size_t new_len = (da)->len + 1;                                        \
        if (new_len >= (da)->cap) {                                            \
            int new_cap = (da)->cap * 2;                                       \
            int new_size = new_cap * sizeof(item);                             \
            void *new_data_ptr = kep_arena_push(arena, new_size);              \
            assert(new_data_ptr != NULL && "Increase arena size!");            \
            memcpy(new_data_ptr, (da)->items, (da)->len * sizeof(item));       \
                                                                               \
            (da)->items = new_data_ptr;                                        \
            (da)->cap = new_cap;                                               \
        }                                                                      \
        int len = (da)->len;                                                   \
        (da)->items[len] = item;                                               \
        (da)->len += 1;                                                        \
    } while (0)

#define KEP_DA_CONCAT(arena, da, array)                                        \
    do {                                                                       \
        size_t new_len = (da)->len + array.len;                                \
        size_t item_size = sizeof((da)->items[0]);                             \
        if (new_len >= (da)->cap) {                                            \
            size_t new_cap = (da)->cap * 2;                                    \
            while (new_cap <= new_len) {                                       \
                new_cap *= 2;                                                  \
            }                                                                  \
            int new_size = new_cap * item_size;                                \
            void *new_data_ptr = kep_arena_push(arena, new_size);              \
                                                                               \
            assert(new_data_ptr != NULL && "Increase arena size!");            \
            memcpy(new_data_ptr, (da)->items, (da)->len *item_size);           \
                                                                               \
            (da)->items = new_data_ptr;                                        \
            (da)->cap = new_cap;                                               \
        }                                                                      \
                                                                               \
        memcpy((da)->items + (da)->len, array.items, array.len * item_size);   \
        (da)->len = new_len;                                                   \
    } while (0)

// hack to get the amount of args passed in via VA_ARGS
// copies the args into a compound literal array
#define KEP_VARI_SIZE(T, ...) (sizeof((T[]){__VA_ARGS__}) / sizeof(T))

// A macro to help create a cstr_array without having to directly give a length
#define KEP_CREATE_CSTRS(...)                                                  \
    kep_create_cstrs(KEP_VARI_SIZE(char *, __VA_ARGS__),                       \
                     (char *[]){__VA_ARGS__});

kep_arena kep_new_arena(size_t size);
void *kep_arena_push(kep_arena *a, size_t size);
void kep_arena_reset(kep_arena *a);
void kep_arena_free(kep_arena *a);

kep_str kep_slice_from_cstr(char *str);
kep_str kep_slice_from_da(kep_da_str str);
bool kep_strcmp(kep_str str1, kep_str str2);
bool kep_strcmp_cstr(kep_str k_str, char *cstr);
kep_cstrs kep_create_cstrs(size_t len, ...);

kep_exe kep_create_exe(char *name, kep_cstrs srcs, kep_cstrs includes);
kep_lib kep_create_lib(char *name, kep_cstrs srcs, char *path);
kep_lib kep_create_syslib(char *name);
void kep_link_libraries(kep_exe *exe, kep_libs libs);
int kep_build_lib(kep_lib lib);
int kep_build_exe(kep_exe art);
int kep_run_command(char *cmd);

#endif // KEPLAR_H_

#ifdef KEP_IMP

// Inner funcs
static char *create_compile_cmd(kep_arena *arena, char *comp_name, char *cflags,
                                kep_exe exe) {
    size_t buffer_size = 0;
    buffer_size += strlen(comp_name);
    buffer_size += strlen(cflags);
    buffer_size += strlen(exe.name);

    kep_da_str srcs_str;
    kep_da_str includes_str;
    kep_da_str libs_str;
    KEP_DA_INIT(arena, &srcs_str, char, 64);
    KEP_DA_INIT(arena, &includes_str, char, 64);
    KEP_DA_INIT(arena, &libs_str, char, 64);

    for (int i = 0; i < exe.sources.len; i++) {
        kep_str sv = kep_slice_from_cstr(KEP_A_GET(exe.sources, i));
        KEP_DA_CONCAT(arena, &srcs_str, sv);
    }

    for (int i = 0; i < exe.include_dirs.len; i++) {
        char *inc = KEP_A_GET(exe.include_dirs, i);
        int buffer_size = strlen(inc) + 4; // "-I %s "
        char i_buf[buffer_size];
        snprintf(i_buf, buffer_size + 1, "-I %s ", inc);
        kep_str sv = kep_slice_from_cstr(i_buf);
        KEP_DA_CONCAT(arena, &includes_str, sv);
    }

    for (size_t i = 0; i < exe.linked_libs.len; i++) {
        kep_lib lib = KEP_A_GET(exe.linked_libs, i);
        int buffer_size = strlen(lib.name) + 4; // "-l%s "

        if (lib.path != NULL) {
            buffer_size += strlen(lib.path);
            buffer_size += 4; // "-L%s "
        }

        char l_buf[buffer_size];

        if (lib.path != NULL) {
            snprintf(l_buf, buffer_size + 1, "-L%s -l%s ", lib.path, lib.name);
        } else {
            snprintf(l_buf, buffer_size + 1, "-l%s ", lib.name);
        }

        kep_str sv = kep_slice_from_cstr(l_buf);
        KEP_DA_CONCAT(arena, &libs_str, sv);
    }

    printf("srcs: " SV_FMT "\n", SV_ARGS(srcs_str));
    printf("includes: " SV_FMT "\n", SV_ARGS(includes_str));

    buffer_size += srcs_str.len;
    buffer_size += includes_str.len;
    buffer_size += libs_str.len;
    buffer_size +=
        10; // a bit extra for spaces and extra stuff in the format string

    char *buffer = kep_arena_push(arena, buffer_size);
    // plus 1 for null terminator
    snprintf(buffer, buffer_size + 1,
             "%s %s " SV_FMT " " SV_FMT " " SV_FMT "-o %s", comp_name, cflags,
             SV_ARGS(srcs_str), SV_ARGS(includes_str), SV_ARGS(libs_str),
             exe.name);

    return buffer;
}

static char *create_lib_compile_cmd(kep_arena *arena, char *comp_name,
                                    char *cflags, kep_lib lib) {
    size_t buffer_size = 0;
    buffer_size += strlen(comp_name);
    buffer_size += strlen(cflags);
    buffer_size += strlen(lib.name);

    kep_da_str srcs_str;
    kep_da_str objs_str;
    KEP_DA_INIT(arena, &srcs_str, char, 64);
    KEP_DA_INIT(arena, &objs_str, char, 64);

    for (int i = 0; i < lib.sources.len; i++) {
        kep_str sv = kep_slice_from_cstr(KEP_A_GET(lib.sources, i));
        KEP_DA_CONCAT(arena, &srcs_str, sv);

        // Slice off ".c"
        kep_str new_sv = sv;
        new_sv.len -= 2;

        KEP_DA_CONCAT(arena, &objs_str, new_sv);
        // Take the name and append ".o"
        KEP_DA_CONCAT(arena, &objs_str, kep_slice_from_cstr(".o"));
    }

    printf("srcs: " SV_FMT "\n", SV_ARGS(srcs_str));
    printf("objs: " SV_FMT "\n", SV_ARGS(objs_str));

    buffer_size += srcs_str.len;
    buffer_size += objs_str.len;
    buffer_size += 100;

    char *buffer = kep_arena_push(arena, buffer_size);
    assert(buffer != NULL && "Scratch Arena too Small!");

    // first cmd builds .o files,                 "cc -c foo.c"
    // second moves all object files to lib.path, "mv foo.o lib/foo.o"
    // third builds lib at lib.path/lib(lib.name) "ar rcs lib/libfoo.a foo.o"
    snprintf(buffer, buffer_size + 1,
             "%s %s " SV_FMT " -c && mv *.o %s && ar rsc %slib%s.a " SV_FMT,
             comp_name, cflags, SV_ARGS(srcs_str), lib.path, lib.path, lib.name,
             SV_ARGS(objs_str));

    return buffer;
}

// Implementation

int kep_run_command(char *cmd) { return system(cmd); }

kep_arena kep_new_arena(size_t size) {
    void *a_buffer = KEP_ALLOC(size);
    kep_arena a;
    a.buffer = a_buffer;
    a.cap = size;
    a.current_pos = 0;
    return a;
}

// allocs memory onto arena and memsets bytes to 0
void *kep_arena_push(kep_arena *a, size_t size) {
    void *new_ptr = (void *)(a->current_pos + a->buffer);
    a->current_pos = a->current_pos + size;

    if (a->current_pos > a->cap) {
        return NULL;
    }

    memset(new_ptr, 0, size);

    return new_ptr;
}

void kep_arena_reset(kep_arena *a) { a->current_pos = 0; }

void kep_arena_free(kep_arena *a) { KEP_FREE(a->buffer); }

kep_str kep_slice_from_cstr(char *str) {
    kep_str k_str;
    k_str.items = str;
    k_str.len = strlen(str);

    return k_str;
}

kep_str kep_slice_from_da(kep_da_str str) {
    kep_str slice;
    slice.items = str.items;
    slice.len = str.len;

    return slice;
}

bool kep_strcmp(kep_str str1, kep_str str2) {
    // Pointer match
    if (str1.items == str2.items) {
        return true;
    }

    if (str1.len != str2.len) {
        return false;
    }

    for (size_t i = 0; i < str1.len; i++) {
        if (str1.items[i] != str2.items[i]) {
            return false;
        }
    }

    return true;
}

bool kep_strcmp_cstr(kep_str k_str, char *cstr) {
    // Pointer match
    if (k_str.items == cstr) {
        return true;
    }

    if (k_str.len != strlen(cstr)) {
        return false;
    }

    for (size_t i = 0; i < k_str.len; i++) {
        if (k_str.items[i] != cstr[i]) {
            return false;
        }
    }

    return true;
}

kep_cstrs kep_create_cstrs(size_t len, ...) {
    va_list args;
    va_start(args, len);

    kep_cstrs cstrs;
    // get first element (start of array)
    cstrs.items = va_arg(args, char **);
    cstrs.len = len;

    return cstrs;
}

kep_exe kep_create_exe(char *name, kep_cstrs srcs, kep_cstrs includes) {
    kep_exe exe;
    exe.sources = srcs;
    exe.include_dirs = includes;
    exe.name = name;

    return exe;
}

kep_lib kep_create_lib(char *name, kep_cstrs srcs, char *path) {
    kep_lib lib;
    lib.sources = srcs;
    lib.name = name;
    lib.path = path;
    lib.dynamic = false;

    return lib;
}

kep_lib kep_create_syslib(char *name) {
    kep_lib lib;
    lib.name = name;
    lib.path = NULL;

    return lib;
}

void kep_link_libraries(kep_exe *exe, kep_libs libs) {
    exe->linked_libs = libs;
}

int kep_build_lib(kep_lib lib) {
    kep_arena scratch_a = kep_new_arena(1024);

    char *cmd = create_lib_compile_cmd(&scratch_a, KEP_COMPILER,
                                       "-Wall -Werror -Wextra -g", lib);
    printf("Compiling lib with cmd: %s\n", cmd);

    kep_arena_free(&scratch_a);

    return kep_run_command(cmd);
}

int kep_build_exe(kep_exe art) {
    kep_arena scratch_a = kep_new_arena(1024);

    char *cmd = create_compile_cmd(&scratch_a, KEP_COMPILER,
                                   "-Wall -Werror -Wextra -g", art);
    printf("Compiling with cmd: %s\n", cmd);

    kep_arena_free(&scratch_a);

    return kep_run_command(cmd);
}

#endif // KEP_IMP
