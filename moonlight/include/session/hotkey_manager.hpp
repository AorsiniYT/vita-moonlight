#pragma once
#include <chrono>
#include <functional>
#include <vector>

#include "borealis.hpp"

// HotkeyManager: manages button combinations for global actions (ex: open pause overlay).
// Initial design: hardcode START+L+R; it will later be expanded to dynamic configuration and persistence.
// Intended use: lightweight singleton instance; register callback for "pauseCombo".

class HotkeyManager
{
  public:
    static HotkeyManager& instance();

    // Registers callback that fires when the pause combination is detected.
    void setPauseCallback(const std::function<void()>& cb) { pauseCallback = cb; }

    void onButtonEvent(brls::ControllerButton button, bool pressed);

    // Future config: set maximum ms window for simultaneous detection (default 220ms)
    void setComboWindowMs(int ms) { comboWindowMs = ms; }

  private:
    HotkeyManager()                                = default;
    HotkeyManager(const HotkeyManager&)            = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    std::function<void()> pauseCallback;
    std::function<void()> menuCallback;

    // Status of relevant buttons
    bool btnStart = false;
    bool btnL     = false;
    bool btnR     = false;

    using Clock = std::chrono::steady_clock;
    Clock::time_point lastStart {};
    Clock::time_point lastL {};
    Clock::time_point lastR {};

    int comboWindowMs = 220; // window to consider combination

    void tryTrigger();
};
