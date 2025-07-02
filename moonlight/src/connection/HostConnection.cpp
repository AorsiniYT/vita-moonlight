/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#include "connection/HostConnection.hpp"
#include <borealis/core/logger.hpp>

namespace moonlight {

HostConnection::HostConnection() : state(HostConnectionState::Disconnected) {}

bool HostConnection::reset() {
    if (state != HostConnectionState::Disconnected) {
        brls::Logger::warning("[HostConnection] reset() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Ready);
    return true;
}

bool HostConnection::pair() {
    if (state != HostConnectionState::Ready && state != HostConnectionState::Paired && state != HostConnectionState::Connected) {
        brls::Logger::warning("[HostConnection] pair() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Paired);
    return true;
}

bool HostConnection::connect() {
    if (state != HostConnectionState::Paired) {
        brls::Logger::warning("[HostConnection] connect() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Connected);
    return true;
}

bool HostConnection::minimize() {
    if (state != HostConnectionState::Connected) {
        brls::Logger::warning("[HostConnection] minimize() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Minimized);
    return true;
}

bool HostConnection::resume() {
    if (state != HostConnectionState::Minimized) {
        brls::Logger::warning("[HostConnection] resume() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Connected);
    return true;
}

bool HostConnection::terminate() {
    if (state != HostConnectionState::Paired && state != HostConnectionState::Connected && state != HostConnectionState::Minimized) {
        brls::Logger::warning("[HostConnection] terminate() inválido en estado {}", (int)state);
        return false;
    }
    setState(HostConnectionState::Disconnected);
    return true;
}

HostConnectionState HostConnection::getState() const {
    return state;
}

bool HostConnection::isReady() const {
    return state != HostConnectionState::Disconnected;
}

bool HostConnection::isConnected() const {
    return state == HostConnectionState::Connected;
}

void HostConnection::setStateChangeCallback(StateChangeCallback cb) {
    onStateChange = cb;
}

void HostConnection::setState(HostConnectionState newState) {
    if (state != newState) {
        state = newState;
        brls::Logger::info("[HostConnection] Estado cambiado a {}", (int)state);
        if (onStateChange)
            onStateChange(state);
    }
}

} // namespace moonlight
