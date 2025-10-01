#pragma once

#include <stdint.h>
#include <psp2/touch.h>

// Clase para manejar input táctil en PS Vita
class TouchInputManager {
public:
    TouchInputManager();
    ~TouchInputManager();

    // Procesar input táctil basado en modo
    void handleTouch(int touchscreenMode);

    // Resetear estados de touch
    void dropTouch(int touchscreenMode);

private:
    // Touch states
    SceTouchData touchData;
    SceTouchData lastTouchData;

    // Funciones específicas de cada modo
    void handleDS4Touch();
    void handleTabletTouch();
    void handleMouse();
};

extern TouchInputManager* g_touchInput;