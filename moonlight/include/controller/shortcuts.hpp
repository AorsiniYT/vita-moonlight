// shortcuts.hpp
#pragma once
#include <psp2/ctrl.h>
#include <cstdint>
#include <functional>

// Set callback for pause
void set_pause_callback(const std::function<void()>& cb);

// Returns true if a shortcut was executed and the input should be cleared
bool process_physical_shortcuts(const SceCtrlData* pad, const SceCtrlData* pad_old);

// Executes a virtual shortcut associated with a special code (for example, pause menu).
bool trigger_virtual_shortcut(std::uint32_t specialKey);