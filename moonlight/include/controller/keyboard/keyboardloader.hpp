#pragma once

#include <cstdint>

// Result of a VK mapping lookup: the Windows Virtual Key code plus required modifiers
struct VkMapping
{
    int vk           = 0;
    bool needs_shift = false;
    bool needs_altgr = false;
};

// Enumeration of supported keyboard layouts (works for both client display and host mapping)
enum KeyboardLayout
{
    KB_LAYOUT_EN_US = 0,
    KB_LAYOUT_ES_ES,
    KB_LAYOUT_ES_LATAM,
    KB_LAYOUT_COUNT
};

// Auto-detect and load all XML keyboard layouts from resources/keyboard/
void auto_load_keyboard_layouts();

// Lookup a VK mapping for a Unicode codepoint using the current layout XML.
// Returns false when the codepoint has no VK mapping.
bool lookup_vk_mapping(KeyboardLayout layout, std::uint32_t codepoint, VkMapping& out_mapping);
