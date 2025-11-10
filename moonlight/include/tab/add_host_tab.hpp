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
#include <borealis.hpp>
#include <string>
#include "utils/host_search.hpp"
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>

// Contexto seguro para pairing asíncrono
struct PairingContext {
    std::atomic<bool> cancelled{false};
    // Puedes agregar más datos aquí si es necesario
};

class AddHostTab : public brls::Box, public std::enable_shared_from_this<AddHostTab> {
public:
    AddHostTab();
    virtual ~AddHostTab();
    static brls::View* create();
    void refreshHostsList();
    void startDeviceDiscovery();
#ifdef _WIN32
    static AddHostTab* winInstance;
    static void winHostFoundCb(int idx, const char* host, const char* pcname, const char* ip, int port);
#endif
#if defined(__PSV__)
    static AddHostTab* vitaInstance;
    void rebuildDiscoveredHostsUI();
#endif
    BRLS_BIND(brls::InputCell, ipField, "ip_field");
    BRLS_BIND(brls::InputCell, nameField, "name_field");
    BRLS_BIND(brls::SelectorCell, preferExternalSelector, "prefer_external_selector");
    BRLS_BIND(brls::Button, addButton, "add_button");
    BRLS_BIND(brls::Box, hostsList, "hosts_list");
    std::atomic<bool> discoveryRunning {false};
    std::thread discoveryThread;
    BRLS_BIND(brls::Box, loader, "loader");
    std::atomic<bool> pairingInProgress {false};
    // Marca si los inputs ya fueron desbloqueados cuando apareció el PIN
    std::atomic<bool> inputsUnblockedByPin {false};

    // --- Pairing seguro y asincrónico ---
    std::shared_ptr<PairingContext> pairingContext;
    std::thread pairingThread;
#if defined(__PSV__)
    std::vector<std::pair<std::string, std::string>> discoveredHosts;
#endif
};
