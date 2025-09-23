#pragma once
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
// Log básico (siempre imprime). Usa formato printf.
__attribute__((format(printf, 1, 2))) void vita_debug_log(const char* fmt, ...);
#ifdef __cplusplus
}
#endif
