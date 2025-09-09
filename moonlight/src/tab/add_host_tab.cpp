#if 1
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
#endif
#include "tab/add_host_tab.hpp"
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_selector.hpp>
#include "view/pccard.hpp"
#include "model/HostStorage.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <fstream>
#include "ConfigManager.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <thread>
// Declaración anticipada para uso global
std::vector<std::pair<std::string, std::string>>& getDiscoveredHostsWin();
#endif
#include <borealis/core/thread.hpp>
#include <atomic>
#include <thread>

#include <thread>
#include <vector>
#include <sstream>

#if defined(__PSV__)
#include <psp2/kernel/threadmgr.h>
#endif


#include "check_host.hpp"
#include "connection/pairing.hpp"

using namespace brls::literals;

AddHostTab::AddHostTab() {
    brls::Logger::info("[AddHostTab] Constructor: Iniciando inflateFromXMLRes");
    this->inflateFromXMLRes("xml/tabs/add_host.xml");
    // --- Diagnóstico de núcleos de CPU disponibles y uso actual ---
// --- Diagnóstico de núcleos de CPU disponibles y uso actual ---
#if defined(__PSV__)
    int cpuCoreCount = 4; // Teórico
    brls::Logger::info("[AddHostTab][CPU] Núcleos CPU PSVita (teórico): {}", cpuCoreCount);
    // Obtener affinity mask del hilo principal
    int affinityMask = sceKernelGetThreadCpuAffinityMask(0);
    brls::Logger::info("[AddHostTab][CPU] Affinity mask del hilo principal: 0x{:X}", affinityMask);
    // Mostrar en el log qué núcleos están disponibles realmente
    std::string availableCores;
    for (int i = 0; i < 4; ++i) {
        if (affinityMask & (1 << i)) {
            availableCores += std::to_string(i) + " ";
        }
    }
    if (!availableCores.empty())
        brls::Logger::info("[AddHostTab][CPU] Núcleos disponibles para este hilo: {}", availableCores);
    else
        brls::Logger::warning("[AddHostTab][CPU] ¡Ningún núcleo disponible para este hilo! (mask=0x{:X})", affinityMask);
#else
    unsigned int cpuCoreCount = std::thread::hardware_concurrency();
    brls::Logger::info("[AddHostTab][CPU] Núcleos CPU detectados: {}", cpuCoreCount);
#endif
    // Log de IDs de hilos y afinidad (diagnóstico)
    std::stringstream ss;
    ss << std::this_thread::get_id();
    brls::Logger::info("[AddHostTab][CPU] ID del hilo principal: {}", ss.str());

    // El spinner y el label ya están en el XML, controlar visibilidad por id
    brls::View* spinner = this->getView("spinner");
    brls::View* searchLabel = this->getView("search_label");
    if (spinner)
        spinner->setVisibility(brls::Visibility::VISIBLE);
    if (searchLabel)
        searchLabel->setVisibility(brls::Visibility::VISIBLE);

    // Los textos estáticos (title) ya se manejan en XML con i18n.
    // El placeholder solo puede traducirse en C++.
    // Prueba manual de traducción y logs
    std::string testKey = "moonlight/settings/add_host_ip_placeholder";
    std::string testTranslation = brls::getStr(testKey);
    brls::Logger::info("[AddHostTab] Prueba manual: clave='{}', traducción='{}'", testKey, testTranslation);
    brls::Logger::info("[AddHostTab] Locale activo: {}", brls::Application::getLocale());
    brls::Logger::info("[AddHostTab] Ruta recursos esperada: resources/i18n/es/moonlight.json");

    if (this->ipField) {
        std::string placeholder = brls::getStr("moonlight/settings/add_host_ip_placeholder");
        brls::Logger::info("[AddHostTab] Placeholder resolved: {}", placeholder);
        // Permitir IP:PUERTO hasta 22 caracteres (ej: 255.255.255.255:65535)
        this->ipField->init(brls::getStr("moonlight/settings/ip"), "", [](std::string){}, placeholder, "", 22);
    }
    if (this->nameField)
        // Limite ampliado para nombre de PC: 50 caracteres
        this->nameField->init(brls::getStr("moonlight/settings/add_host_manual"), "", [](std::string){}, brls::getStr("moonlight/settings/add_host_name_placeholder"), "", 50);

    // Inicializar el selector de preferencia LAN/Online
    // Usar init(title, opciones, seleccion por defecto, callback cambio)
    if (this->preferExternalSelector) {
        std::vector<std::string> options = {
            brls::getStr("moonlight/settings/add_host_lan"),
            brls::getStr("moonlight/settings/add_host_online")
        };
        this->preferExternalSelector->init(
            brls::getStr("moonlight/settings/add_host_connection_type"),
            options,
            0,
            [](int){} // callback vacío, personalizable
        );
    }

    if (this->addButton && this->ipField && this->nameField && this->preferExternalSelector) {
        // Botón de emparejar manual (Add Host Manual)
        this->addButton->registerClickAction([this](brls::View*) {
            brls::Logger::info("[AddHostTab][MANUAL] Emparejamiento manual iniciado");
#if defined(__PSV__)
            if (AddHostTab::vitaInstance != nullptr) {
                check_host::stopVitaDiscovery();
                if (AddHostTab::vitaInstance != this) {
                    brls::Logger::warning("[AddHostTab][SAFE] Descubrimiento ya fue cerrado por otra instancia, abortando.");
                    return true;
                }
                AddHostTab::vitaInstance = nullptr;
            }
#endif
            std::string ipInput = this->ipField->getValue();
            std::string name = this->nameField->getValue();
            if (ipInput.empty() || name.empty()) {
                brls::Application::notify(brls::getStr("moonlight/settings/add_host_missing_fields"));
                return true;
            }
#if defined(__PSV__)
            if (AddHostTab::vitaInstance != nullptr) {
                brls::Application::notify("Por favor espera a que termine la búsqueda de dispositivos antes de emparejar.");
                brls::Logger::warning("[AddHostTab][SAFE] Emparejamiento bloqueado: descubrimiento aún activo.");
                return true;
            }
#endif
            // Cancelar cualquier pairing anterior
            if (this->pairingContext) this->pairingContext->cancelled = true;
            if (this->pairingThread.joinable()) this->pairingThread.join();
            this->pairingContext = std::make_shared<PairingContext>();
            auto context = this->pairingContext;
            // Lógica de pairing: delegar la UI del diálogo a pairing.cpp
            brls::Application::blockInputs();
            auto weakSelf = this->weak_from_this();
            startMoonlightPairingWithPopup(ipInput, name, [weakSelf](bool pairingSuccess) {
                auto self = weakSelf.lock();
                if (!self) return;
                brls::sync([self, pairingSuccess]() {
                    brls::Application::unblockInputs();
                    if (pairingSuccess) {
                        if (self->ipField) self->ipField->setValue("");
                        if (self->nameField) self->nameField->setValue("");
                        if (self->preferExternalSelector) self->preferExternalSelector->setSelection(0);
                        self->refreshHostsList();
                    }
                });
            });
            return true;
        });
    }
    // El label de dispositivos encontrados se traduce en XML, no es necesario aquí


    this->startDeviceDiscovery();
}


// Callbacks para check_host

// Instancias estáticas para callbacks de descubrimiento
#if defined(__PSV__)
AddHostTab* AddHostTab::vitaInstance = nullptr;
// Declaración anticipada para uso en todo el archivo
#if defined(_WIN32)
std::vector<std::pair<std::string, std::string>>& getDiscoveredHostsWin();
#endif

#elif defined(_WIN32)
AddHostTab* AddHostTab::winInstance = nullptr;
extern "C" void AddHostTab::winHostFoundCb(int idx, const char* host, const char* pcname, const char* ip, int port) {
    if (!AddHostTab::winInstance) return;
    brls::Logger::info("[AddHostTab] Host detectado (WIN): {} - {}:{} (host={})", pcname, ip, port, host);
    brls::Logger::info("[AddHostTab] Datos callback WIN: host='{}' pcname='{}' ip='{}' port={}", host, pcname, ip, port);
    // Usar la lista global de hosts descubiertos en Windows
    auto& discoveredHostsWin = getDiscoveredHostsWin();
    std::string name(pcname ? pcname : "");
    std::string ipStr(ip ? ip : "");
    std::string displayName = name;
    constexpr size_t maxDisplayLen = 50;
    if (displayName.length() > maxDisplayLen && maxDisplayLen > 3) {
        displayName = displayName.substr(0, maxDisplayLen - 3) + "...";
    } else if (displayName.length() > maxDisplayLen && maxDisplayLen <= 3) {
        displayName = std::string("...").substr(0, maxDisplayLen);
    }
    if (name.empty() && ipStr.empty()) {
        brls::Logger::error("[AddHostTab] Host detectado pero sin datos válidos (WIN)");
        return;
    }
    // Evitar duplicados
    for (const auto& h : discoveredHostsWin) {
        if (h.first == displayName && h.second == ipStr) return;
    }
    discoveredHostsWin.push_back({displayName, ipStr});
    brls::sync([=]() {
        if (!AddHostTab::winInstance->hostsList) {
            brls::Logger::error("[AddHostTab] hostsList es nulo (WIN)");
            return;
        }
        // Limpiar todo menos el spinner
        brls::View* spinnerRow = AddHostTab::winInstance->getView("spinner_row");
        auto children = AddHostTab::winInstance->hostsList->getChildren();
        for (auto* child : children) {
            if (child != spinnerRow)
                AddHostTab::winInstance->hostsList->removeView(child);
        }
        // Parámetros de grid
        const int cardsPerRow = 3;
        int count = 0;
        brls::Box* currentRow = nullptr;
        for (const auto& host : discoveredHostsWin) {
            if (count % cardsPerRow == 0) {
                currentRow = new brls::Box(brls::Axis::ROW);
                currentRow->setMarginBottom(16);
                AddHostTab::winInstance->hostsList->addView(currentRow);
            }
            auto* card = new PCCard(host.first.c_str(), "img/moonlight/pc.png");
            card->setFocusable(true);
            card->setMarginRight(16);
            card->setMarginBottom(0);
            // La lógica de truncado/animación se maneja internamente en PCCard
            // Captura por valor el par host (name, ip)
            card->setClickAction([host]() {
                std::string msg = brls::getStr("moonlight/settings/add_host_connect_question_dialog");
                size_t pos_ip = msg.find("$(ip)");
                if (pos_ip != std::string::npos)
                    msg.replace(pos_ip, 5, host.second);
                size_t pos_name = msg.find("$(name)");
                if (pos_name != std::string::npos)
                    msg.replace(pos_name, 7, host.first);
                auto* dialog = new brls::Dialog(msg);
                dialog->addButton(brls::getStr("moonlight/settings/add_host_connect"), [host, dialog]() {
                    if (AddHostTab::winInstance && AddHostTab::winInstance->ipField) AddHostTab::winInstance->ipField->setValue(host.second);
                    if (AddHostTab::winInstance && AddHostTab::winInstance->nameField) AddHostTab::winInstance->nameField->setValue(host.first);
             
                });
                dialog->addButton(brls::getStr("moonlight/settings/add_host_cancel"), [dialog]() {
             
                });
                dialog->open();
            });
            if (currentRow)
                currentRow->addView(card);
            count++;
        }
        // Spinner siempre al final
        if (spinnerRow)
            spinnerRow->setVisibility(brls::Visibility::VISIBLE);
        else
            brls::Logger::error("[AddHostTab] spinnerRow es nulo tras añadir host (WIN)");
    });
}
#endif

void AddHostTab::startDeviceDiscovery() {
    // Limpiar la lista de hosts y mostrar el spinner
    brls::View* spinnerRow = this->getView("spinner_row");
    if (!this->hostsList) {
        brls::Logger::error("[AddHostTab] hostsList es nulo. No se puede limpiar la lista de hosts.");
    } else if (!spinnerRow) {
        brls::Logger::error("[AddHostTab] spinnerRow es nulo. No se puede mostrar el spinner.");
    } else {
        auto children = this->hostsList->getChildren();
        for (auto* child : children) {
            if (child != spinnerRow)
                this->hostsList->removeView(child);
        }
        spinnerRow->setVisibility(brls::Visibility::VISIBLE);
    }
    // Iniciar descubrimiento usando la nueva interfaz centralizada
#if defined(__PSV__)
    AddHostTab::vitaInstance = this;
    static std::vector<std::pair<std::string, std::string>> discoveredHosts; // name, ip
    discoveredHosts.clear();
    check_host::startVitaDiscovery([](int idx, const char* host, const char* pcname, const char* ip, int port) {
        if (!AddHostTab::vitaInstance) {
            brls::Logger::error("[check_host] vitaInstance es nulo");
            return;
        }
        brls::Logger::info("[AddHostTab] Host detectado (VITA): {} - {}:{} (host={})", pcname ? pcname : "(null)", ip ? ip : "(null)", port, host ? host : "(null)");
        brls::Logger::info("[AddHostTab] Datos callback VITA: host='{}' pcname='{}' ip='{}' port={}", host ? host : "(null)", pcname ? pcname : "(null)", ip ? ip : "(null)", port);
        std::string name(pcname ? pcname : "");
        std::string ipStr(ip ? ip : "");
        std::string displayName = name;
        constexpr size_t maxDisplayLen = 50;
        if (displayName.length() > maxDisplayLen && maxDisplayLen > 3) {
            displayName = displayName.substr(0, maxDisplayLen - 3) + "...";
        } else if (displayName.length() > maxDisplayLen && maxDisplayLen <= 3) {
            displayName = std::string("...").substr(0, maxDisplayLen);
        }
        if (name.empty() && ipStr.empty()) {
            brls::Logger::error("[AddHostTab] Host detectado pero sin datos válidos (VITA)");
            return;
        }
        // Guardar en la lista temporal
        discoveredHosts.push_back({displayName, ipStr});
        // Redibujar el grid en el hilo principal
        brls::sync([=]() {
            if (!AddHostTab::vitaInstance->hostsList) {
                brls::Logger::error("[check_host] hostsList es nulo");
                return;
            }
            // Limpiar todo menos el spinner
            brls::View* spinnerRow = AddHostTab::vitaInstance->getView("spinner_row");
            auto children = AddHostTab::vitaInstance->hostsList->getChildren();
            for (auto* child : children) {
                if (child != spinnerRow)
                    AddHostTab::vitaInstance->hostsList->removeView(child);
            }
            // Parámetros de grid
            const int cardsPerRow = 3;
            int count = 0;
            brls::Box* currentRow = nullptr;
            for (const auto& host : discoveredHosts) {
                if (count % cardsPerRow == 0) {
                    currentRow = new brls::Box(brls::Axis::ROW);
                    currentRow->setMarginBottom(16);
                    AddHostTab::vitaInstance->hostsList->addView(currentRow);
                }
                auto* card = new PCCard(host.first.c_str(), "img/moonlight/pc.png");
                card->setFocusable(true);
                card->setMarginRight(16);
                card->setMarginBottom(0);
                // La lógica de truncado/animación se maneja internamente en PCCard
                AddHostTab* self = AddHostTab::vitaInstance;
                std::string ipStr = host.second;
                std::string name = host.first;
                card->setClickAction([self, ipStr, name]() {
                    std::string msg = brls::getStr("moonlight/settings/add_host_connect_question_dialog");
                    size_t pos_ip = msg.find("$(ip)");
                    if (pos_ip != std::string::npos)
                        msg.replace(pos_ip, 5, ipStr);
                    size_t pos_name = msg.find("$(name)");
                    if (pos_name != std::string::npos)
                        msg.replace(pos_name, 7, name);
                    auto* dialog = new brls::Dialog(msg);
                    dialog->addButton(brls::getStr("moonlight/settings/add_host_connect"), [self, ipStr, name, dialog]() {
                        // Solo rellenar los campos manuales y cerrar el diálogo, sin emparejar
                        if (self && self->ipField) self->ipField->setValue(ipStr);
                        if (self && self->nameField) self->nameField->setValue(name);

                    });
                    dialog->addButton(brls::getStr("moonlight/settings/add_host_cancel"), [dialog]() {

                    });
                    dialog->open();
                });
                if (currentRow)
                    currentRow->addView(card);
                count++;
            }
            // Spinner siempre al final
            if (spinnerRow)
                spinnerRow->setVisibility(brls::Visibility::VISIBLE);
            else
                brls::Logger::error("[check_host] spinnerRow es nulo tras añadir host");
        });
    });
#elif defined(_WIN32)
    AddHostTab::winInstance = this;
    // Limpiar la lista de hosts descubiertos al iniciar búsqueda
    getDiscoveredHostsWin().clear();
    check_host::startWinDiscovery(winHostFoundCb);
#endif
    // Cuando termine la búsqueda, el callback se encarga de ocultar el spinner si no hay hosts
}

brls::View* AddHostTab::create() {
// Crear la instancia como puntero crudo, Borealis gestiona el ciclo de vida
return new AddHostTab();
}

void AddHostTab::refreshHostsList() {
    // No hacer nada aquí, la lista se maneja en startDeviceDiscovery
}

AddHostTab::~AddHostTab() {
    brls::Logger::info("[AddHostTab] (DEBUG) Destructor: deteniendo hilo de descubrimiento y pairing...");
#if defined(__PSV__)
    AddHostTab::vitaInstance = nullptr;
    check_host::stopVitaDiscovery();
#elif defined(_WIN32)
    check_host::stopWinDiscovery();
    AddHostTab::winInstance = nullptr;
    getDiscoveredHostsWin().clear();
#endif
    // Cancelar pairing seguro
    if (this->pairingContext) this->pairingContext->cancelled = true;
    if (this->pairingThread.joinable()) this->pairingThread.join();
}

#if defined(_WIN32)
// Acceso global a la lista de hosts descubiertos en Windows
std::vector<std::pair<std::string, std::string>>& getDiscoveredHostsWin() {
    static std::vector<std::pair<std::string, std::string>> discoveredHostsWin;
    return discoveredHostsWin;
}
#endif
