#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <functional>
#include <psp2/ctrl.h>
#include "controller/TouchInput.hpp"
#include "controller/RearTouchInput.hpp"
#include "controller/FrontTouchInput.hpp"
#include "controller/keyboard/IKeyboard.hpp"
#include "debug.hpp"
#include "controller/GamepadState.hpp"
#include "ConfigManager.hpp"

// Forward declare Gyro manager
class GyroManager;

struct VitaMouseState {
    uint16_t buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
};

// Class to handle input on PS Vita
class ControllerInputManager {
public:
    ControllerInputManager();
    ~ControllerInputManager();

    // Process input and send events
    void handleInput();

    // Reset input states (when disconnecting)
    void dropInput();

    // Enable/disable input processing/sending (useful for overlays/menus)
    void setInputEnabled(bool enabled);

    // Set touchscreen mode
    void setTouchscreenMode(int mode) { touchscreenMode = mode; }

    // Validate and change touch mode (with gamepad compatibility)
    bool setTouchscreenModeWithValidation(int newMode);

    // Change touch mode at runtime (like setGamepadType)
    // Validate compatibility, update config and notify change
    bool setTouchscreenModeRuntime(int newMode);

    // Send gamepad status
    void sendGamepadState(const GamepadState& state);

    // Callback for pause hotkey (START+L1+R1)
    void setPauseCallback(const std::function<void()>& cb);
    void setKeyboardShortcutCallback(const std::function<void()>& cb);

    void applyRearTouchSettings(const RearTouchSettings& settings);
    void setRearTouchEnabled(bool enabled);

    void applyFrontTouchSettings(const VideoSettings& settings);
    void setFrontTouchEnabled(bool enabled);

    // Change gamepad type without restarting session
    void setGamepadType(GamepadType type);

    // Keyboard integration
    void setActiveKeyboard(IKeyboard* kb);
    IKeyboard* getActiveKeyboard() const { return activeKeyboard; }

    // Gyroscope access for test overlay
    GyroManager* getGyroManager() const { return gyroManager; }

    // PS button capture (lock/unlock for streaming)
    void lockPSButton();
    void unlockPSButton();
    void setStreamingActive(bool active);

private:
    bool inputEnabled;
    bool streamingActive = false;
    bool inputDropped;
    bool touchSuppressed = false;
    uint64_t touchSuppressUntilUs = 0;
    bool lastKeyboardOpen = false;

    // Previous states to detect changes
    GamepadState lastGamepadState;
    VitaMouseState lastMouseState;
    SceCtrlData lastCtrlData;

    int touchscreenMode;

    TouchInputManager* touchManager;
    RearTouchInputManager* rearTouchManager;
    FrontTouchInputManager* frontTouchManager;

    // Hotkey state
    std::function<void()> pauseCallback;

    // Active keyboard for polling
    IKeyboard* activeKeyboard = nullptr;
    bool activeKeyboardSeenOpen = false;

    // Current gamepad type
    GamepadType currentGamepadType = GAMEPAD_TYPE_XBOX;

    struct ButtonMapping {
        uint32_t btnDpadUp;
        uint32_t btnDpadDown;
        uint32_t btnDpadLeft;
        uint32_t btnDpadRight;
        uint32_t btnSouth;
        uint32_t btnEast;
        uint32_t btnNorth;
        uint32_t btnWest;
        uint32_t btnSelect;
        uint32_t btnStart;
        uint32_t btnL1;
        uint32_t btnR1;
        uint32_t btnL2;
        uint32_t btnR2;
        uint32_t btnL3;
        uint32_t btnR3;
        uint32_t absLX;
        uint32_t absLY;
        uint32_t absRX;
        uint32_t absRY;
    };

    ButtonMapping mapping;
    bool isPstvModel;
    GyroManager* gyroManager = nullptr;

    // PS button state
    bool psButtonLocked = false;
    bool psButtonWasPressed = false;
    uint64_t psButtonPressedTimeUs = 0;
    bool psButtonSpecialActive = false;

    void initMapping();
    GamepadState buildGamepadState(const SceCtrlData& ctrlData) const;
    bool isPressed(uint32_t binding, const SceCtrlData& pad) const;
    unsigned char readTrigger(uint32_t binding, const SceCtrlData& pad) const;
    short readAxis(uint32_t binding, const SceCtrlData& pad) const;
    short applyDeadzone(short value) const;
};

extern ControllerInputManager* g_controllerInput;