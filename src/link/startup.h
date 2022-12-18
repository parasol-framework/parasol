#pragma once

#include <parasol/main.h>

#ifdef __cplusplus
extern "C" {
#endif

void print(CSTRING, ...);
const char * init_parasol(int argc, CSTRING *argv);
void close_parasol(void);

#ifdef __cplusplus
}
#endif
