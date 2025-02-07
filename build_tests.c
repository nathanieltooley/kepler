#define KEP_IMP
#include "kepler.h"

int main() {
    kep_cstrs lib_srcs = KEP_CREATE_CSTRS("tests/linking.c");
    kep_lib liblinking = kep_create_lib("linking", lib_srcs, "./tests/");

    kep_cstrs srcs = KEP_CREATE_CSTRS("tests/tests.c");
    kep_cstrs includes = KEP_CREATE_CSTRS(".", "./tests/");

    kep_libs libs = (kep_libs){(kep_lib[]){liblinking}, 1};

    kep_exe exe = kep_create_exe("run_tests", srcs, includes);
    kep_link_libraries(&exe, libs);

    kep_build_lib(liblinking);
    return kep_build_exe(exe);
}
