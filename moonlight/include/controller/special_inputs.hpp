#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace controller
{

struct SpecialInputOption
{
    std::uint32_t code;
    const char* name;
    bool selectable;
};

inline constexpr std::uint32_t INPUT_SPECIAL_KEY_PAUSE    = 0;
inline constexpr std::uint32_t INPUT_SPECIAL_KEY_KEYBOARD = 1;

// Returns the entire list (includes non-selectable headers)
const std::vector<SpecialInputOption>& getAllSpecialInputOptions();

// Returns only selectable entries in the same order as legacy
const std::vector<SpecialInputOption>& getSelectableSpecialInputOptions();

// Search for the option by code. Returns nullptr if it does not exist.
const SpecialInputOption* findSpecialInputOption(std::uint32_t code);

// Gets the index within the selectable list. Returns 0 if not found.
std::size_t getSelectableIndexForCode(std::uint32_t code);

// Gets the code associated with an index of the selectable list. Returns 0 if the index is invalid.
std::uint32_t getCodeForSelectableIndex(std::size_t index);

// Returns a display-ready name; if it does not exist, it returns a hexadecimal representation.
std::string getDisplayNameForCode(std::uint32_t code);

} // namespace controller
