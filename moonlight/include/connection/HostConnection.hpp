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
#pragma once
#include <string>
#include <functional>
#include <borealis.hpp>

namespace moonlight {

// Estados de la conexión, inspirados en el legacy
enum class HostConnectionState {
    Disconnected,
    Ready,
    Paired,
    Connected,
    Minimized
};

class HostConnection {
public:
    using StateChangeCallback = std::function<void(HostConnectionState)>;

    HostConnection();

    // Métodos de transición
    bool reset();
    bool pair();
    bool connect();
    bool minimize();
    bool resume();
    bool terminate();

    // Getters
    HostConnectionState getState() const;
    bool isReady() const;
    bool isConnected() const;

    // Callback para notificar cambios de estado
    void setStateChangeCallback(StateChangeCallback cb);

private:
    HostConnectionState state;
    StateChangeCallback onStateChange;
    void setState(HostConnectionState newState);
};

} // namespace moonlight
