#pragma once

#include <cstring>

// Keyboard state structure for polling-based keyboards
struct KeyboardState
{
    bool keys[256];

    KeyboardState()
    {
        memset(keys, 0, sizeof(keys));
    }
};

// Abstract interface for keyboard implementations.
// Both Legacy (SCE IME) and Modern (Borealis overlay) keyboards implement this.
class IKeyboard
{
  public:
    virtual ~IKeyboard() = default;

    // Open/show the keyboard
    virtual void open() = 0;

    // Close/hide the keyboard
    virtual void close() = 0;

    // Returns true if the keyboard is currently open/visible
    virtual bool isOpen() const = 0;

    // Called every frame from the main thread (ControllerInputManager::handleInput).
    // Legacy keyboard uses this to call sceImeUpdate().
    // Modern keyboard does nothing here (it draws via Borealis).
    virtual void update() { }

    // Get current keyboard state for polling (Modern keyboard only).
    // Legacy keyboard returns empty state since it sends keys directly.
    virtual KeyboardState getKeyboardState() const
    {
        KeyboardState s;
        return s;
    }

    // Returns true if this keyboard sends key events directly via
    // LiSendKeyboardEvent() (Legacy IME), rather than through polling.
    // When true, ControllerInputManager skips the polling loop.
    virtual bool sendsDirectly() const { return false; }

    // Returns true if this keyboard expects VK codes to be treated as-is
    // (non-normalized) by Sunshine.
    virtual bool usesNonNormalizedVk() const { return false; }

    // Returns true if the keyboard implementation manages its own memory lifetime
    // (e.g. Borealis View lifecycle). If false, ControllerInputManager will delete it.
    virtual bool selfDestructs() const { return false; }
};
