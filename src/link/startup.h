#ifndef STARTUP_H
#define STARTUP_H TRUE

#ifndef PARASOL_MAIN_H
#include <parasol/main.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void print(CSTRING, ...);
const char * init_parasol(int argc, CSTRING *argv);
void close_parasol(void);

#ifdef __cplusplus
}
#endif

#endif // STARTUP_H
