#pragma once

#include <psp2/touch.h>
#include <stdint.h>

// Enumeration of touch modes
enum TouchscreenMode
{
    TOUCHSCREEN_MODE_OFF            = 0, // No touch input
    TOUCHSCREEN_MODE_TRACKPAD       = 1, // Trackpad (relative movement)
    TOUCHSCREEN_MODE_DS4_TOUCHPAD   = 2, // DS4 touchpad (only with PS4 gamepad)
    TOUCHSCREEN_MODE_MOUSE_ABSOLUTE = 3, // Mouse absoluto
    TOUCHSCREEN_MODE_TABLET         = 4 // Tablet/Sunshine
};

// Class to handle touch input on PS Vita
class TouchInputManager
{
  public:
    TouchInputManager();
    ~TouchInputManager();

    // Process touch input based on mode
    void handleTouch(int touchscreenMode);

    // Reset touch states
    void dropTouch(int touchscreenMode);

    // Validate that the mode is compatible with the current gamepad
    bool isModeSupportedByGamepad(int touchscreenMode, int gamepadType);

    // Change mode with validation
    bool setTouchMode(int newMode, int gamepadType);

    // Methods for Instant Trackpad Settings Changes
    void setTrackpadSettings(int pointerSpeed, int deadZone, bool tapToClick,
        bool twoFingerRightClick, bool twoFingerScroll,
        bool invertScroll, bool multiTouch, int edgeZone);

    // Getters for current trackpad settings
    int getPointerSpeed() const;
    int getDeadZone() const;
    bool isTapToClickEnabled() const;
    bool isTwoFingerRightClickEnabled() const;
    bool isTwoFingerScrollEnabled() const;
    bool isInvertScrollEnabled() const;
    bool isMultiTouchEnabled() const;
    int getEdgeZone() const;

  private:
    // Touch states
    SceTouchData touchData;
    SceTouchData lastTouchData;
    int currentMode;

    // Trackpad settings (for instant changes)
    int trackpadPointerSpeed         = 100; // 0-200
    int trackpadDeadZone             = 50; // 0-200px
    bool trackpadTapToClick          = true;
    bool trackpadTwoFingerRightClick = true;
    bool trackpadTwoFingerScroll     = true;
    bool trackpadInvertScroll        = false;
    bool trackpadMultiTouch          = true;
    int trackpadEdgeZone             = 15; // 0-50%

    // Trackpad state machine state
    int trackpadState                   = 0;
    int trackpadFingerCount             = 0;
    uint64_t trackpadStateStartTime     = 0;
    SceTouchReport trackpadInitialTouch = { 0 };
    SceTouchReport trackpadSwipeStart   = { 0 };

    // States for each mode
    struct
    {
        bool lastMouseDown;
        int lastAbsX, lastAbsY;
    } trackpadModeState;

    struct
    {
        uint8_t prevFingerActive[10];
        int prevX[10], prevY[10];
    } ds4State;

    struct
    {
        int frontState;
        short fingerCount;
        uint32_t twoFingerStartTime;
        int twoFingerStartY;
        int twoFingerLastY;
        bool twoFingerScroll;
        bool rightClickSent;
    } absoluteState;

    struct
    {
        uint8_t prevFingerActive[10];
    } tabletState;

    // Specific functions of each mode
    void handleTrackpad(); // Modo 1: Trackpad (relative mouse)
    void handleDS4Touch(); // Modo 2: DS4 touchpad (PS4 only)
    void handleMouseAbsolute(); // Mode 3: Absolute mouse
    void handleTabletTouch(); // Modo 4: Tablet/Sunshine

    // Auxiliary functions
    void resetTrackpadState();
    void resetDS4State();
    void resetAbsoluteState();
    void resetTabletState();
};

extern TouchInputManager* g_touchInput;