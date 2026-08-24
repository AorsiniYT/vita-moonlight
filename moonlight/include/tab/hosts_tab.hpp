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
#include "utils/host_search.hpp"
#include "model/HostStorage.hpp" // For HostInfo

class HostsTab : public brls::Box {
public:
    HostsTab();
    static brls::View* create();
    void refreshHostsList();
    // Requests that the main activity be completely reloaded (rebuilds the home)
    // Safe implementation: Execute the recreation of the MainActivity content in the UI thread.
    static void requestGlobalRefresh();
    BRLS_BIND(brls::Box, hostsList, "hosts_list");
};
// check_host.hpp
#pragma once

#if defined(__PSV__)
#include <psp2/kernel/threadmgr.h>
#endif

namespace check_host {
#if defined(__PSV__)
void startVitaDiscovery(void (*hostFoundCb)(int, const char*, const char*, const char*, int));
void stopVitaDiscovery();
bool isVitaDiscoveryActive();
#endif
#if defined(_WIN32)
void startWinDiscovery(void (*hostFoundCb)(int, const char*, const char*, const char*, int));
void stopWinDiscovery();
bool isWinDiscoveryActive();
#endif
} // namespace check_host
