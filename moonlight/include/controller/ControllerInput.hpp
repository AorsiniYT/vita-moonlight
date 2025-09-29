#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include "Limelight.h"

// Estructura para estado de gamepad
typedef struct {
    uint16_t buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
} GamepadState;

// Estructura para estado de mouse
typedef struct {
    float scroll_y;
    bool l_pressed;
    bool m_pressed;
    bool r_pressed;
} VitaMouseState;

// Clase para manejar input en PS Vita
class ControllerInputManager {
public:
    ControllerInputManager();
    ~ControllerInputManager();

    // Procesar input y enviar eventos
    void handleInput();

    // Resetear estados de input (al desconectar)
    void dropInput();

    // Enviar estado de gamepad
    void sendGamepadState(const GamepadState& state);

    // Enviar eventos de táctil
    void handleTouch();

    // Enviar eventos de mouse (emulado por táctil si es necesario)
    void handleMouse();

private:
    bool inputEnabled;
    bool inputDropped;

    // Estados anteriores para detectar cambios
    GamepadState lastGamepadState;
    VitaMouseState lastMouseState;

    // Touch states
    SceTouchData touchData;
    SceTouchData lastTouchData;

    // Mapeo de botones Vita a flags Limelight
    uint16_t mapButtons(uint32_t vitaButtons);
};

extern ControllerInputManager* g_controllerInput;