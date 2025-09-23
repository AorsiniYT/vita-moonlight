#pragma once
#include <cstdint>
#include <functional>
#include <array>

// Abstracción inicial para input de control (gamepad / touch) para futura sincronización y envío.
// Objetivos: mapear botones Vita -> códigos Moonlight / plataforma host.

struct ControllerState {
    uint32_t buttons = 0;   // bitmask
    int16_t lx = 0;         // stick izquierdo X (-32768..32767)
    int16_t ly = 0;         // stick izquierdo Y
    int16_t rx = 0;         // stick derecho X
    int16_t ry = 0;         // stick derecho Y
    uint8_t lt = 0;         // gatillo L analógico (si aplica)
    uint8_t rt = 0;         // gatillo R analógico
};

class ControllerInput {
public:
    static ControllerInput& instance();

    void poll();                 // leer estado actual de hardware
    ControllerState getState() const; // snapshot
    void reset();

private:
    ControllerInput() = default;
    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;

    ControllerState state;
};
