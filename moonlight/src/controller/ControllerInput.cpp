#include "ControllerInput.hpp"

ControllerInput& ControllerInput::instance() { static ControllerInput inst; return inst; }

void ControllerInput::poll() {
    // TODO: implementar lectura real de SceCtrlData / touch
}

ControllerState ControllerInput::getState() const { return state; }

void ControllerInput::reset() { state = ControllerState{}; }
