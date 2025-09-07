#ifndef BUILD_H
#define BUILD_H
#include <stddef.h>



typedef struct {
    char*  compiler;   // "cc", "gcc", "clang", 
    char** cfiles;     int cfiles_n;   // *.c 
    char** cflags;     int cflags_n;   // (-I, -D, -std, -O*, -g, -Wall, …)
    char** ldflags;    int ldflags_n;  // (-L, -l*, -Wl,*, -pthread, …)
} build_spec_t;


int parse_user_cmd(const char* cmdline, build_spec_t* out);

void free_build_spec(build_spec_t* s);


#endif
