#include "greatest.h"
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "linking.h"

#define KEP_IMP
#include "kepler.h"

TEST arena_alloc(void) {
    kep_arena arena = kep_new_arena(1024);

    ASSERT_NEQ(NULL, arena.buffer);
    ASSERT_EQ(1024, arena.cap);
    ASSERT_EQ(0, arena.current_pos);

    PASS();
}

TEST arena_push(kep_arena arena) {
    int *x = kep_arena_push(&arena, sizeof(int));
    *x = 69;

    ASSERT_EQ(arena.current_pos, sizeof(int));
    ASSERT_EQ(x, arena.buffer);
    ASSERT_EQ(69, *x);

    int *y = kep_arena_push(&arena, sizeof(int));
    *y = 420;

    ASSERT_EQ(arena.current_pos, 2 * sizeof(int));
    ASSERT_EQ(y, arena.buffer + sizeof(int));
    ASSERT_EQ(420, *y);

    PASS();
}

TEST da_init(kep_arena arena) {
    kep_da_str str;
    KEP_DA_INIT(&arena, &str, char, 128);

    ASSERT_NEQ(str.items, NULL);
    ASSERT_EQ(str.cap, 128);
    ASSERT_EQ(str.len, 0);

    PASS();
}

TEST da_append(kep_arena arena) {
    kep_da_str str;
    KEP_DA_INIT(&arena, &str, char, 128);
    KEP_DA_APPEND(&arena, &str, 'k');

    ASSERT_EQ(str.len, 1);
    ASSERT_EQ(str.items[0], 'k');

    PASS();
}

TEST da_realloc_append(kep_arena arena) {
    kep_da_str str;
    KEP_DA_INIT(&arena, &str, char, 1);

    char a = 0x61;

    for (int i = 0; i < 127; i++) {
        KEP_DA_APPEND(&arena, &str, a);
    }

    ASSERT_EQ(str.len, 127);
    ASSERT_EQ(str.cap, 128);

    for (int i = 0; i < 127; i++) {
        ASSERT_EQ_FMT(0x61, str.items[i], "%c");
    }

    PASS();
}

TEST da_concat(kep_arena arena) {
    kep_da_str str;
    KEP_DA_INIT(&arena, &str, char, 8);

    kep_str test_str = kep_slice_from_cstr("Hello World!!!!!");

    KEP_DA_CONCAT(&arena, &str, test_str);

    ASSERT_EQ_FMT((size_t)16, str.len, "%ld");
    ASSERT_EQ_FMT((size_t)32, str.cap, "%ld");
    ASSERT_EQ_FMT((size_t)16, test_str.len, "%ld");

    ASSERT(kep_strcmp(test_str, kep_slice_from_da(str)));

    KEP_DA_CONCAT(&arena, &str, test_str);

    ASSERT_EQ_FMT((size_t)32, str.len, "%ld");
    ASSERT_EQ_FMT((size_t)64, str.cap, "%ld");

    PASS();
}

TEST str_cmp(void) {
    char *ptr_match = "Hello World!";
    kep_str str1 = kep_slice_from_cstr(ptr_match);
    kep_str str2 = kep_slice_from_cstr(ptr_match);

    kep_str str3 = kep_slice_from_cstr("Hello World!");

    ASSERT(kep_strcmp(str1, str2));
    ASSERT(kep_strcmp(str1, str3));
    ASSERT(kep_strcmp_cstr(str1, "Hello World!"));

    PASS();
}

TEST array_indexing(void) {
    char *strs[3] = {"Hello", "World", "!!!"};
    kep_cstrs src;

    src.items = strs;
    src.len = 3;

    ASSERT_EQ(KEP_A_GET(src, 2), strs[2]);
    // ASSERT_EQ(KEP_A_GET(src, 3), strs[2]);

    PASS();
}

// TEST build_exe(void) {
//     kep_cstrs srcs = KEP_CREATE_CSTRS("tests/tests.c");
//     kep_cstrs includes = KEP_CREATE_CSTRS(".");
//
//     kep_artifact exe = kep_create_exe("test_build", srcs, includes);
//
//     int res = kep_build_artifact(exe);
//
//     ASSERT_EQ(0, res);
//     ASSERT(access("test_build", F_OK) == 0);
//     ASSERT_EQ(0, unlink("test_build"));
//
//     PASS();
// }

SUITE(arena) {
    kep_arena arena = kep_new_arena(1024);

    RUN_TEST(arena_alloc);
    // intentionally passing arena by value
    // so that the stack pointer "resets" for us automatically
    RUN_TESTp(arena_push, arena);

    kep_arena_free(&arena);
}

SUITE(da) {
    kep_arena arena = kep_new_arena(1024);

    RUN_TESTp(da_init, arena);
    RUN_TESTp(da_append, arena);
    RUN_TESTp(da_realloc_append, arena);
    RUN_TESTp(da_concat, arena);

    kep_arena_free(&arena);
}

SUITE(str) { RUN_TEST(str_cmp); }

SUITE(array) { RUN_TEST(array_indexing); }

// SUITE(build) { RUN_TEST(build_exe); }

TEST vari_size(void) {
    ASSERT_EQ_FMT((long)1, KEP_VARI_SIZE(char *, "Hello"), "%ld");

    ASSERT_EQ_FMT((long)2, KEP_VARI_SIZE(char *, "Hello", "World"), "%ld");

    ASSERT_EQ_FMT((long)3,
                  KEP_VARI_SIZE(char *, "Hello", "World", "Hello World!"),
                  "%ld");

    ASSERT_NEQ(0, KEP_VARI_SIZE(char *, "Hello", "World", "Hello World!"));

    // testing outside of macro aswell
    //(sizeof((T[]){__VA_ARGS__}) / sizeof(T))
    ASSERT_EQ_FMT((long)(2),
                  (sizeof(char *[]){"Hello", "World!"}) / sizeof(char *),
                  "%ld");

    PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();

    // for testing linking (with libm)
    printf("We're linked!: %d\n", return_negative_one());

    RUN_SUITE(arena);
    RUN_SUITE(da);
    RUN_SUITE(str);
    RUN_SUITE(array);

    RUN_TEST(vari_size);

    GREATEST_MAIN_END();
}
