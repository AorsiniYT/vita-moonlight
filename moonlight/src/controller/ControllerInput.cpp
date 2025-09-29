#include "controller/ControllerInput.hpp"
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <cmath>

// Instancia global
ControllerInputManager* g_controllerInput = nullptr;

// Constructor
ControllerInputManager::ControllerInputManager() : inputEnabled(true), inputDropped(false) {
    memset(&lastGamepadState, 0, sizeof(GamepadState));
    memset(&lastMouseState, 0, sizeof(VitaMouseState));
    memset(&touchData, 0, sizeof(SceTouchData));
    memset(&lastTouchData, 0, sizeof(SceTouchData));

    // Inicializar touch
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    // Inicializar controles
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    printf("[ControllerInput] Initialized\n");
}

// Destructor
ControllerInputManager::~ControllerInputManager() {
    // Nada especial
}

// Mapeo de botones Vita a flags Limelight
uint16_t ControllerInputManager::mapButtons(uint32_t vitaButtons) {
    uint16_t flags = 0;

    if (vitaButtons & SCE_CTRL_UP) flags |= UP_FLAG;
    if (vitaButtons & SCE_CTRL_DOWN) flags |= DOWN_FLAG;
    if (vitaButtons & SCE_CTRL_LEFT) flags |= LEFT_FLAG;
    if (vitaButtons & SCE_CTRL_RIGHT) flags |= RIGHT_FLAG;

    if (vitaButtons & SCE_CTRL_CROSS) flags |= A_FLAG;  // A
    if (vitaButtons & SCE_CTRL_CIRCLE) flags |= B_FLAG; // B
    if (vitaButtons & SCE_CTRL_SQUARE) flags |= X_FLAG; // X
    if (vitaButtons & SCE_CTRL_TRIANGLE) flags |= Y_FLAG; // Y

    if (vitaButtons & SCE_CTRL_START) flags |= PLAY_FLAG;
    if (vitaButtons & SCE_CTRL_SELECT) flags |= BACK_FLAG;

    if (vitaButtons & SCE_CTRL_L1) flags |= LB_FLAG;
    if (vitaButtons & SCE_CTRL_R1) flags |= RB_FLAG;

    if (vitaButtons & SCE_CTRL_L3) flags |= LS_CLK_FLAG;
    if (vitaButtons & SCE_CTRL_R3) flags |= RS_CLK_FLAG;

    return flags;
}

// Procesar input
void ControllerInputManager::handleInput() {
    if (!inputEnabled) return;

    inputDropped = false;

    // Leer controles
    SceCtrlData ctrlData;
    sceCtrlPeekBufferPositive(0, &ctrlData, 1);

    GamepadState gamepadState = {
        .buttonFlags = mapButtons(ctrlData.buttons),
        .leftTrigger = (ctrlData.buttons & SCE_CTRL_L2) ? 255 : 0,  // Simplificado
        .rightTrigger = (ctrlData.buttons & SCE_CTRL_R2) ? 255 : 0,
        .leftStickX = (short)((ctrlData.lx - 128) * 256),  // Escalar a -32768..32767
        .leftStickY = (short)((ctrlData.ly - 128) * 256),
        .rightStickX = (short)((ctrlData.rx - 128) * 256),
        .rightStickY = (short)((ctrlData.ry - 128) * 256),
    };

    // Deadzone simple
    if (abs(gamepadState.leftStickX) < 1024) gamepadState.leftStickX = 0;
    if (abs(gamepadState.leftStickY) < 1024) gamepadState.leftStickY = 0;
    if (abs(gamepadState.rightStickX) < 1024) gamepadState.rightStickX = 0;
    if (abs(gamepadState.rightStickY) < 1024) gamepadState.rightStickY = 0;

    // Enviar si cambió
    if (memcmp(&gamepadState, &lastGamepadState, sizeof(GamepadState)) != 0) {
        sendGamepadState(gamepadState);
        lastGamepadState = gamepadState;
    }

    // Manejar táctil
    handleTouch();

    // Manejar mouse (si se emula con táctil)
    handleMouse();
}

// Enviar estado de gamepad
void ControllerInputManager::sendGamepadState(const GamepadState& state) {
    if (LiSendMultiControllerEvent(0, 1, state.buttonFlags, state.leftTrigger, state.rightTrigger,
                                    state.leftStickX, state.leftStickY, state.rightStickX, state.rightStickY) != 0) {
        printf("[ControllerInput] Failed to send gamepad state\n");
    }
}

// Manejar táctil
void ControllerInputManager::handleTouch() {
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touchData, 1);

    // Enviar UP para dedos que se levantaron (estaban en last pero no en current)
    for (int j = 0; j < lastTouchData.reportNum; j++) {
        SceTouchReport lastReport = lastTouchData.report[j];
        bool stillPressed = false;
        for (int i = 0; i < touchData.reportNum; i++) {
            if (touchData.report[i].id == lastReport.id) {
                stillPressed = true;
                break;
            }
        }
        if (!stillPressed) {
            LiSendTouchEvent(LI_TOUCH_EVENT_UP, lastReport.id, lastReport.x / 1920.0f, lastReport.y / 1088.0f, 0, 0, 0, LI_ROT_UNKNOWN);
        }
    }

    // Enviar DOWN/MOVE para dedos actuales
    for (int i = 0; i < touchData.reportNum; i++) {
        SceTouchReport report = touchData.report[i];
        uint8_t eventType = LI_TOUCH_EVENT_MOVE;

        // Verificar si es nuevo (DOWN)
        bool wasPressed = false;
        for (int j = 0; j < lastTouchData.reportNum; j++) {
            if (lastTouchData.report[j].id == report.id) {
                wasPressed = true;
                break;
            }
        }
        if (!wasPressed) {
            eventType = LI_TOUCH_EVENT_DOWN;
        }

        // Enviar evento
        LiSendTouchEvent(eventType, report.id, report.x / 1920.0f, report.y / 1088.0f, 0, 0, 0, LI_ROT_UNKNOWN);
    }

    lastTouchData = touchData;
}

// Manejar mouse (emulado con táctil si un dedo)
void ControllerInputManager::handleMouse() {
    // Si hay exactamente un toque, mover mouse y manejar click
    static bool lastMouseDown = false;
    bool mouseDown = (touchData.reportNum == 1);

    if (touchData.reportNum == 1) {
        SceTouchReport report = touchData.report[0];
        LiSendMousePositionEvent(report.x, report.y, 1920, 1088);
    }

    // Enviar click down/up
    if (mouseDown && !lastMouseDown) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    } else if (!mouseDown && lastMouseDown) {
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    }

    lastMouseDown = mouseDown;
}

// Resetear input
void ControllerInputManager::dropInput() {
    if (inputDropped) return;

    // Reset gamepad
    GamepadState zeroState = {0};
    sendGamepadState(zeroState);

    // Reset touch
    for (int i = 0; i < 10; i++) {  // Máx toques
        LiSendTouchEvent(LI_TOUCH_EVENT_CANCEL, i, 0, 0, 0, 0, 0, LI_ROT_UNKNOWN);
    }

    // Reset mouse
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);

    inputDropped = true;
    printf("[ControllerInput] Input dropped\n");
}
