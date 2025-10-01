#include "controller/ControllerInput.hpp"
#include <psp2/ctrl.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <psp2/kernel/sysmem.h>
#include "Limelight.h"
#include "debug.hpp"
#include "controller/shortcuts.hpp"
#include "controller/input_types.hpp"
#include "ConfigManager.hpp"

namespace {

using controller::INPUT_TYPE_ANALOG;
using controller::INPUT_TYPE_GAMEPAD;
using controller::INPUT_TYPE_MASK;
using controller::INPUT_VALUE_MASK;
using controller::ANALOG_LEFT_TRIGGER;
using controller::ANALOG_RIGHT_TRIGGER;

enum AnalogBinding : uint32_t {
    ANALOG_LEFT_X = 0,
    ANALOG_LEFT_Y,
    ANALOG_RIGHT_X,
    ANALOG_RIGHT_Y,
    ANALOG_LEFT_TRIGGER_BIND = ANALOG_LEFT_TRIGGER,
    ANALOG_RIGHT_TRIGGER_BIND = ANALOG_RIGHT_TRIGGER,
};

inline uint32_t makeGamepadBinding(uint32_t buttonMask) {
    return buttonMask ? (buttonMask | INPUT_TYPE_GAMEPAD) : 0;
}

inline uint32_t makeAnalogBinding(AnalogBinding binding) {
    return static_cast<uint32_t>(binding) | INPUT_TYPE_ANALOG;
}

}

// Instancia global
ControllerInputManager* g_controllerInput = nullptr;

// Constructor
ControllerInputManager::ControllerInputManager() : inputEnabled(true), inputDropped(false), touchscreenMode(0), touchManager(nullptr), rearTouchManager(nullptr), pauseCallback(nullptr) {
    memset(&lastGamepadState, 0, sizeof(GamepadState));
    memset(&lastMouseState, 0, sizeof(VitaMouseState));
    memset(&lastCtrlData, 0, sizeof(SceCtrlData));
    memset(&mapping, 0, sizeof(mapping));

    // Inicializar controles
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);

    touchManager = new TouchInputManager();
    rearTouchManager = new RearTouchInputManager();

    ConfigManager config;
    config.load();
    VideoSettings initialSettings = config.getVideoSettings();
    rearTouchManager->updateSettings(initialSettings.rear_touch);

    isPstvModel = (sceKernelGetModel() == SCE_KERNEL_MODEL_VITATV);
    initMapping();

    vita_debug_log("[ControllerInput] Initialized");
}

// Destructor
ControllerInputManager::~ControllerInputManager() {
    if (touchManager) {
        delete touchManager;
        touchManager = nullptr;
    }
    if (rearTouchManager) {
        delete rearTouchManager;
        rearTouchManager = nullptr;
    }
}

// Procesar input
void ControllerInputManager::handleInput() {
    if (!inputEnabled) return;

    inputDropped = false;

    // Leer controles
    SceCtrlData ctrlData;
    sceCtrlPeekBufferPositive(0, &ctrlData, 1);

    if (!isPstvModel) {
        // En PS Vita portátil, los botones físicos L/R aparecen como LTRIGGER/RTRIGGER.
        // Normalizamos para que también activen los flags de L1/R1 requeridos por los shortcuts.
        if (ctrlData.buttons & SCE_CTRL_LTRIGGER) {
            ctrlData.buttons |= SCE_CTRL_L1;
        }
        if (ctrlData.buttons & SCE_CTRL_RTRIGGER) {
            ctrlData.buttons |= SCE_CTRL_R1;
        }
    }

    // Debug: mostrar botones presionados y valores de triggers
    static int debug_counter = 0;
    if (debug_counter++ % 60 == 0) { // Cada segundo aproximadamente
        vita_debug_log("[ControllerInput] Botones: 0x%08X (L1:%d R1:%d L2:%d R2:%d START:%d) LT:%d RT:%d", 
                      ctrlData.buttons,
                      (ctrlData.buttons & SCE_CTRL_L1) ? 1 : 0,
                      (ctrlData.buttons & SCE_CTRL_R1) ? 1 : 0,
                      (ctrlData.buttons & SCE_CTRL_L2) ? 1 : 0,
                      (ctrlData.buttons & SCE_CTRL_R2) ? 1 : 0,
                      (ctrlData.buttons & SCE_CTRL_START) ? 1 : 0,
                      ctrlData.lt,
                      ctrlData.rt);
    }

    // Procesar shortcuts físicos
    if (process_physical_shortcuts(&ctrlData, &lastCtrlData)) {
        // Shortcut ejecutado, no procesar input normal
        vita_debug_log("[ControllerInput] Shortcut ejecutado, retornando temprano");
        lastCtrlData = ctrlData;
        return;
    }

    GamepadState gamepadState = buildGamepadState(ctrlData);
    if (rearTouchManager) {
        rearTouchManager->process(gamepadState, isPstvModel);
    }

    // Enviar si cambió
    if (memcmp(&gamepadState, &lastGamepadState, sizeof(GamepadState)) != 0) {
        sendGamepadState(gamepadState);
        lastGamepadState = gamepadState;
    }

    // Actualizar estado anterior
    lastCtrlData = ctrlData;

    // Manejar táctil basado en modo
    touchManager->handleTouch(touchscreenMode);
}

// Enviar estado de gamepad
void ControllerInputManager::sendGamepadState(const GamepadState& state) {
    if (LiSendMultiControllerEvent(0, 1, state.buttonFlags, state.leftTrigger, state.rightTrigger,
                                    state.leftStickX, state.leftStickY, state.rightStickX, state.rightStickY) != 0) {
        vita_debug_log("[ControllerInput] Failed to send gamepad state");
    }
}

// Resetear input
void ControllerInputManager::dropInput() {
    if (inputDropped) return;

    // Reset gamepad
    GamepadState zeroState = {0};
    sendGamepadState(zeroState);

    // Reset touch basado en modo
    touchManager->dropTouch(touchscreenMode);
    if (rearTouchManager) {
        rearTouchManager->dropState();
    }

    inputDropped = true;
    vita_debug_log("[ControllerInput] Input dropped");
}

// Callback para hotkey de pausa (START+L1+R1)
void ControllerInputManager::setPauseCallback(const std::function<void()>& cb) { 
    pauseCallback = cb;
    vita_debug_log("[ControllerInput] setPauseCallback llamado, configurando callback");
    set_pause_callback(cb);
}

void ControllerInputManager::initMapping() {
    mapping.btnDpadUp = makeGamepadBinding(SCE_CTRL_UP);
    mapping.btnDpadDown = makeGamepadBinding(SCE_CTRL_DOWN);
    mapping.btnDpadLeft = makeGamepadBinding(SCE_CTRL_LEFT);
    mapping.btnDpadRight = makeGamepadBinding(SCE_CTRL_RIGHT);

    mapping.btnSouth = makeGamepadBinding(SCE_CTRL_CROSS);
    mapping.btnEast = makeGamepadBinding(SCE_CTRL_CIRCLE);
    mapping.btnNorth = makeGamepadBinding(SCE_CTRL_TRIANGLE);
    mapping.btnWest = makeGamepadBinding(SCE_CTRL_SQUARE);

    mapping.btnSelect = makeGamepadBinding(SCE_CTRL_SELECT);
    mapping.btnStart = makeGamepadBinding(SCE_CTRL_START);

    mapping.btnL1 = makeGamepadBinding(SCE_CTRL_L1);
    mapping.btnR1 = makeGamepadBinding(SCE_CTRL_R1);
    mapping.btnL3 = makeGamepadBinding(SCE_CTRL_L3);
    mapping.btnR3 = makeGamepadBinding(SCE_CTRL_R3);

    mapping.absLX = makeAnalogBinding(ANALOG_LEFT_X);
    mapping.absLY = makeAnalogBinding(ANALOG_LEFT_Y);
    mapping.absRX = makeAnalogBinding(ANALOG_RIGHT_X);
    mapping.absRY = makeAnalogBinding(ANALOG_RIGHT_Y);

    if (isPstvModel) {
    mapping.btnL2 = makeAnalogBinding(ANALOG_LEFT_TRIGGER_BIND);
    mapping.btnR2 = makeAnalogBinding(ANALOG_RIGHT_TRIGGER_BIND);
    } else {
        // En PS Vita portátil no hay gatillos físicos analógicos
        mapping.btnL2 = 0;
        mapping.btnR2 = 0;
    }
}

GamepadState ControllerInputManager::buildGamepadState(const SceCtrlData& ctrlData) const {
    GamepadState state{};

    if (isPressed(mapping.btnDpadUp, ctrlData)) state.buttonFlags |= UP_FLAG;
    if (isPressed(mapping.btnDpadDown, ctrlData)) state.buttonFlags |= DOWN_FLAG;
    if (isPressed(mapping.btnDpadLeft, ctrlData)) state.buttonFlags |= LEFT_FLAG;
    if (isPressed(mapping.btnDpadRight, ctrlData)) state.buttonFlags |= RIGHT_FLAG;

    if (isPressed(mapping.btnSouth, ctrlData)) state.buttonFlags |= A_FLAG;
    if (isPressed(mapping.btnEast, ctrlData)) state.buttonFlags |= B_FLAG;
    if (isPressed(mapping.btnWest, ctrlData)) state.buttonFlags |= X_FLAG;
    if (isPressed(mapping.btnNorth, ctrlData)) state.buttonFlags |= Y_FLAG;

    if (isPressed(mapping.btnStart, ctrlData)) state.buttonFlags |= PLAY_FLAG;
    if (isPressed(mapping.btnSelect, ctrlData)) state.buttonFlags |= BACK_FLAG;

    if (isPressed(mapping.btnL1, ctrlData)) state.buttonFlags |= LB_FLAG;
    if (isPressed(mapping.btnR1, ctrlData)) state.buttonFlags |= RB_FLAG;
    state.leftTrigger = readTrigger(mapping.btnL2, ctrlData);
    state.rightTrigger = readTrigger(mapping.btnR2, ctrlData);

    if (isPressed(mapping.btnL3, ctrlData)) state.buttonFlags |= LS_CLK_FLAG;
    if (isPressed(mapping.btnR3, ctrlData)) state.buttonFlags |= RS_CLK_FLAG;

    state.leftStickX = readAxis(mapping.absLX, ctrlData);
    state.leftStickY = (short)-readAxis(mapping.absLY, ctrlData);
    state.rightStickX = readAxis(mapping.absRX, ctrlData);
    state.rightStickY = (short)-readAxis(mapping.absRY, ctrlData);

    state.leftStickX = applyDeadzone(state.leftStickX);
    state.leftStickY = applyDeadzone(state.leftStickY);
    state.rightStickX = applyDeadzone(state.rightStickX);
    state.rightStickY = applyDeadzone(state.rightStickY);

    return state;
}

bool ControllerInputManager::isPressed(uint32_t binding, const SceCtrlData& pad) const {
    if (binding == 0) {
        return false;
    }

    if ((binding & INPUT_TYPE_MASK) != INPUT_TYPE_GAMEPAD) {
        return false;
    }

    return (pad.buttons & (binding & INPUT_VALUE_MASK)) != 0;
}

unsigned char ControllerInputManager::readTrigger(uint32_t binding, const SceCtrlData& pad) const {
    if (binding == 0) {
        return 0;
    }

    switch (binding & INPUT_TYPE_MASK) {
        case INPUT_TYPE_ANALOG: {
            switch (binding & INPUT_VALUE_MASK) {
                case ANALOG_LEFT_TRIGGER_BIND:
                    return pad.lt;
                case ANALOG_RIGHT_TRIGGER_BIND:
                    return pad.rt;
                default:
                    return 0;
            }
        }
        case INPUT_TYPE_GAMEPAD:
            return (pad.buttons & (binding & INPUT_VALUE_MASK)) ? 0xFF : 0;
        default:
            return 0;
    }
}

short ControllerInputManager::readAxis(uint32_t binding, const SceCtrlData& pad) const {
    if ((binding & INPUT_TYPE_MASK) != INPUT_TYPE_ANALOG) {
        return 0;
    }

    int value = 128;
    switch (binding & INPUT_VALUE_MASK) {
        case ANALOG_LEFT_X:
            value = pad.lx;
            break;
        case ANALOG_LEFT_Y:
            value = pad.ly;
            break;
        case ANALOG_RIGHT_X:
            value = pad.rx;
            break;
        case ANALOG_RIGHT_Y:
            value = pad.ry;
            break;
        default:
            value = 128;
            break;
    }

    return static_cast<short>(value * 256 - 32640);
}

short ControllerInputManager::applyDeadzone(short value) const {
    return (std::abs(value) < 1024) ? 0 : value;
}

void ControllerInputManager::applyRearTouchSettings(const RearTouchSettings& settings) {
    if (rearTouchManager) {
        rearTouchManager->updateSettings(settings);
    }
}

void ControllerInputManager::setRearTouchEnabled(bool enabled) {
    if (rearTouchManager) {
        rearTouchManager->setEnabled(enabled);
    }
}
