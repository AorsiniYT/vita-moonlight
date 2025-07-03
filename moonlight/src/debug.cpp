#include <stdio.h>
#include <stdarg.h>
#include "debug.h"
#if defined(__PSV__)
#include <psp2/kernel/clib.h>
#endif

// Implementación para redirigir logs de libgamestream en Vita
extern "C" void vita_debug_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
#if defined(__PSV__)
    sceClibPrintf(fmt, args);
#else
    vprintf(fmt, args);
#endif
    va_end(args);
}
