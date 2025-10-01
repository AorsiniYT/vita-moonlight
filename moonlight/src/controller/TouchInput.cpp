#include "controller/TouchInput.hpp"
#include "Limelight.h"
#include <psp2/touch.h>
#include <cstring>
#include <stdio.h>

// Instancia global
TouchInputManager* g_touchInput = nullptr;

// Constructor
TouchInputManager::TouchInputManager() {
    memset(&touchData, 0, sizeof(SceTouchData));
    memset(&lastTouchData, 0, sizeof(SceTouchData));

    // Inicializar touch
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    printf("[TouchInput] Initialized\n");
}

// Destructor
TouchInputManager::~TouchInputManager() {
    // Nada especial
}

// Procesar input táctil basado en modo
void TouchInputManager::handleTouch(int touchscreenMode) {
    switch(touchscreenMode) {
        case 1: handleDS4Touch(); break;
        case 2: handleMouse(); break;
        case 3: handleTabletTouch(); break;
        default: break;
    }
}

// Manejar táctil para DS4 touchpad
void TouchInputManager::handleDS4Touch() {
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touchData, 1);

    // Enviar UP para dedos que se levantaron
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
            LiSendControllerTouchEvent(0, 1, lastReport.id, 0.0f, 0.0f, 0.0f);
        }
    }

    // Enviar DOWN/MOVE para dedos actuales
    for (int i = 0; i < touchData.reportNum; i++) {
        SceTouchReport report = touchData.report[i];
        uint8_t eventType = 2; // MOVE

        // Verificar si es nuevo (DOWN)
        bool wasPressed = false;
        for (int j = 0; j < lastTouchData.reportNum; j++) {
            if (lastTouchData.report[j].id == report.id) {
                wasPressed = true;
                break;
            }
        }
        if (!wasPressed) {
            eventType = 0; // DOWN
        }

        // Enviar evento
        float x = (float)report.x * 65535.0f / 1919.0f;
        float y = (float)report.y * 65535.0f / 1087.0f;
        LiSendControllerTouchEvent(0, eventType, report.id, x, y, 65535.0f);
    }

    lastTouchData = touchData;
}

// Manejar táctil para tablet multitouch
void TouchInputManager::handleTabletTouch() {
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touchData, 1);

    // Enviar UP para dedos que se levantaron
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
            LiSendTouchEvent(LI_TOUCH_EVENT_UP, lastReport.id, lastReport.x / 1920.0f, lastReport.y / 1088.0f, 1.0f, 0.0f, 0.0f, LI_ROT_UNKNOWN);
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
        LiSendTouchEvent(eventType, report.id, report.x / 1920.0f, report.y / 1088.0f, 1.0f, 0.0f, 0.0f, LI_ROT_UNKNOWN);
    }

    lastTouchData = touchData;
}

// Manejar mouse (emulado con táctil si un dedo)
void TouchInputManager::handleMouse() {
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

// Resetear touch basado en modo
void TouchInputManager::dropTouch(int touchscreenMode) {
    switch(touchscreenMode) {
        case 1: // DS4
            for (int i = 0; i < 10; i++) {
                LiSendControllerTouchEvent(0, 1, i, 0.0f, 0.0f, 0.0f);
            }
            break;
        case 3: // Tablet
            for (int i = 0; i < 10; i++) {
                LiSendTouchEvent(LI_TOUCH_EVENT_CANCEL, i, 0, 0, 0, 0, 0, LI_ROT_UNKNOWN);
            }
            break;
        case 2: // Mouse
            LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
            break;
        default:
            break;
    }
}