#include "vita_globals.hpp"
// Ruta GLS/OpenGL stub: por ahora solo placeholders para evitar mezclar con implementación GXM.
// Esta unidad se activaría si en el futuro se compila con un backend OpenGL (no en PS Vita real).

#ifndef __PSV__
// Renderizado stub (no hace nada real en Vita):
void vitavideo_draw_fps() {}
void vitavideo_draw_indicators() {}
#endif
