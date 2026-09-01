// shortcuts.hpp
#pragma once
#include <psp2/ctrl.h>

#include <cstdint>
#include <functional>

// Set callback for pause
void set_pause_callback(const std::function<void()>& cb);

// Set callback for keyboard shortcut
void set_keyboard_callback(const std::function<void()>& cb);

// Reload shortcuts from shortcuts.conf
void reload_shortcuts_config();

// Returns true if a shortcut was executed and the input should be cleared
bool process_physical_shortcuts(const SceCtrlData* pad, const SceCtrlData* pad_old);

// Executes a virtual shortcut associated with a special code (for example, pause menu).
bool trigger_virtual_shortcut(std::uint32_t specialKey);