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
// check_host.cpp
// Lógica centralizada de descubrimiento de hosts para PSVita y Windows
// Permite iniciar/detener el hilo de descubrimiento de forma robusta y multiplataforma

#include "../../../library/mdnsniff/udp_sniffer_vita.h"
#include "../../../library/mdnsniff/udp_sniffer_win.h"
#include <borealis/core/logger.hpp>
#include <thread>
#include <atomic>
#if defined(__PSV__) || defined(_WIN32)
#include <borealis/core/thread.hpp>
#include <borealis/core/view.hpp>
#include <borealis/core/logger.hpp>
#include "tab/add_host_tab.hpp"
#endif
// --- No se requiere inicialización manual de red Vita, udp_sniffer_vita lo gestiona ---

#if defined(__PSV__)
#include <psp2/kernel/threadmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#endif

namespace check_host {

#if defined(__PSV__)
static SceUID vitaDiscoveryThread = -1;
static volatile int vitaDiscoveryStatus = 0; // 0: idle, 1: running, 2: finished
static std::atomic<bool> vitaThreadActive{false};

void stopVitaDiscovery() {
    vitaDiscoveryStatus = 2;
    if (vitaDiscoveryThread > 0) {
        SceUInt timeout = 100 * 1000;
        sceKernelWaitThreadEnd(vitaDiscoveryThread, NULL, &timeout);
        sceKernelDeleteThread(vitaDiscoveryThread);
        vitaDiscoveryThread = -1;
    }
    udp_sniffer_vita_deinit();
    vitaThreadActive = false;
}


// --- Callback C para logging y reenvío ---
extern "C" {
    // Macro de log compatible con Vita y PC (como en legacy)
#if defined(__PSV__)
#include <psp2/kernel/clib.h>
#define MDNS_LOG(...) sceClibPrintf(__VA_ARGS__)
#else
#define MDNS_LOG(...) printf(__VA_ARGS__)
#endif
}

namespace {
    static void (*vitaHostFoundCb)(int, const char*, const char*, const char*, int) = nullptr;
    extern "C" void vitaHostFoundCbLogger(int idx, const char* host, const char* pcname, const char* ip, int port) {
        MDNS_LOG("[mdns_log] CALLBACK VITA: idx=%d host=%s pcname=%s ip=%s port=%d\n", idx, host ? host : "(null)", pcname ? pcname : "(null)", ip ? ip : "(null)", port);
        brls::Logger::info("[check_host] CALLBACK VITA LLAMADO: idx={} host={} pcname={} ip={} port={}", idx, host ? host : "(null)", pcname ? pcname : "(null)", ip ? ip : "(null)", port);
        if (vitaHostFoundCb) vitaHostFoundCb(idx, host, pcname, ip, port);
    }
}

void startVitaDiscovery(void (*hostFoundCb)(int, const char*, const char*, const char*, int)) {
    // Inicialización robusta de red al estilo legacy
    brls::Logger::info("[check_host] startVitaDiscovery: INICIO (estilo legacy)");
    stopVitaDiscovery();
    brls::Logger::info("[check_host] startVitaDiscovery: después de stopVitaDiscovery");
    // --- Inicialización manual de red Vita (como legacy) ---
    static void* net_mem = nullptr;
    static bool net_initialized = false;
    if (!net_initialized) {
        int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
        if (ret < 0) {
            brls::Logger::error("[check_host] ERROR: sceSysmoduleLoadModule fallo: 0x%08X", ret);
            return;
        }
        net_mem = malloc(256 * 1024);
        if (!net_mem) {
            brls::Logger::error("[check_host] ERROR: malloc para net_mem fallo");
            return;
        }
        SceNetInitParam netInitParam;
        netInitParam.memory = net_mem;
        netInitParam.size = 256 * 1024;
        netInitParam.flags = 0;
        ret = sceNetInit(&netInitParam);
        if (ret < 0 && ret != 0x80410110) {
            brls::Logger::error("[check_host] ERROR: sceNetInit fallo: 0x%08X", ret);
            free(net_mem); net_mem = nullptr;
            return;
        }
        ret = sceNetCtlInit();
        if (ret < 0 && ret != 0x80412110 && ret != 0x80412102) {
            char errbuf[64];
            snprintf(errbuf, sizeof(errbuf), "sceNetCtlInit fallo: hex=0x%08X dec=%d", (unsigned int)ret, ret);
            std::string errMsg = std::string("[check_host] ERROR: ") + errbuf;
            brls::Logger::error("%s", errMsg.c_str());
            MDNS_LOG("[mdns_log] %s\n", errMsg.c_str());
            sceNetTerm();
            free(net_mem); net_mem = nullptr;
            return;
        }
        net_initialized = true;
        brls::Logger::info("[check_host] Red Vita inicializada correctamente (legacy)");
    }
    vitaDiscoveryStatus = 1;
    udp_sniffer_vita_init();
    brls::Logger::info("[check_host] startVitaDiscovery: después de udp_sniffer_vita_init");
    vitaHostFoundCb = hostFoundCb;
    brls::Logger::info("[check_host] startVitaDiscovery: después de asignar vitaHostFoundCb");
    udp_sniffer_vita_set_callback(vitaHostFoundCbLogger);
    brls::Logger::info("[check_host] startVitaDiscovery: después de udp_sniffer_vita_set_callback");
    vitaThreadActive = true;
    brls::Logger::info("[check_host] startVitaDiscovery: antes de crear hilo");
    vitaDiscoveryThread = sceKernelCreateThread("mdns_discovery", [](SceSize, void* argp) -> int {
        MDNS_LOG("[mdns_log] Hilo de descubrimiento mDNS iniciado (máximo 100 iteraciones o hasta cerrar pestaña)\n");
        brls::Logger::info("[check_host] Hilo de descubrimiento mDNS iniciado (máximo 100 iteraciones o hasta cerrar pestaña)");
        int iter = 0;
        while (vitaDiscoveryStatus == 1 && iter < 100) {
            iter++;
            if (iter % 10 == 0) {
                MDNS_LOG("[mdns_log] Descubrimiento mDNS: iteración %d\n", iter);
                brls::Logger::info("[check_host] Descubrimiento mDNS: iteración %d", iter);
            }
            udp_sniffer_vita_poll();
            sceKernelDelayThread(1000 * 100); // 100ms
        }
        MDNS_LOG("[mdns_log] Saliendo del bucle de descubrimiento mDNS\n");
        brls::Logger::info("[check_host] Saliendo del bucle de descubrimiento mDNS");
        udp_sniffer_vita_deinit();
        MDNS_LOG("[mdns_log] Hilo de descubrimiento mDNS finalizado\n");
        brls::Logger::info("[check_host] Hilo de descubrimiento mDNS finalizado");
        vitaDiscoveryStatus = 2;
        vitaThreadActive = false;
        // Notificar al hilo principal para ocultar el spinner
        brls::sync([]() {
            if (AddHostTab::vitaInstance) {
                brls::View* spinnerRow = AddHostTab::vitaInstance->getView("spinner_row");
                if (spinnerRow)
                    spinnerRow->setVisibility(brls::Visibility::GONE);
            }
        });
        return 0;
    }, 0x10000100, 0x10000, 0, 0, NULL);
    brls::Logger::info("[check_host] startVitaDiscovery: después de crear hilo");
    if (vitaDiscoveryThread >= 0) {
        brls::Logger::info("[check_host] startVitaDiscovery: hilo creado correctamente, iniciando...");
        sceKernelStartThread(vitaDiscoveryThread, 0, nullptr);
    } else {
        brls::Logger::error("[check_host] Error creando hilo de descubrimiento mDNS Vita");
    }
}

bool isVitaDiscoveryActive() {
    return vitaThreadActive;
}
#endif

#if defined(_WIN32)
static std::thread winDiscoveryThread;
static std::atomic<bool> winThreadActive{false};

void stopWinDiscovery() {
    winThreadActive = false;
    if (winDiscoveryThread.joinable())
        winDiscoveryThread.join();
    udp_sniffer_win_deinit();
}

void startWinDiscovery(void (*hostFoundCb)(int, const char*, const char*, const char*, int)) {
    stopWinDiscovery();
    winThreadActive = true;
    udp_sniffer_win_init();
    udp_sniffer_win_set_callback(hostFoundCb);
    winDiscoveryThread = std::thread([]() {
        int ticks = 0;
        int max_ticks = 100;
        while (winThreadActive && ticks < max_ticks) {
            udp_sniffer_win_poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ticks++;
        }
        udp_sniffer_win_deinit();
        winThreadActive = false;
        // Notificar al hilo principal para ocultar el spinner
        brls::sync([]() {
            if (AddHostTab::winInstance) {
                brls::View* spinnerRow = AddHostTab::winInstance->getView("spinner_row");
                if (spinnerRow)
                    spinnerRow->setVisibility(brls::Visibility::GONE);
            }
        });
    });
}

bool isWinDiscoveryActive() {
    return winThreadActive;
}
#endif

} // namespace check_host
