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

#include "tab/hosts_tab.hpp"
#include "utils/host_search.hpp"
#include "view/pccard.hpp"
#include "model/HostStorage.hpp"
#include "connection_manager.hpp"
#include "session/session_app_select.hpp"

#include <borealis/core/logger.hpp>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#endif

#ifdef __PSV__
#include <psp2/kernel/clib.h>
#define VITALOG sceClibPrintf
#else
#define VITALOG(...) ((void)0)
#endif

HostsTab::HostsTab() {
    VITALOG("[HostsTab::HostsTab] Constructor llamado\n");
    this->inflateFromXMLRes("xml/tabs/hosts_tab.xml");
    VITALOG("[HostsTab::HostsTab] Antes de refreshHostsList\n");
    this->refreshHostsList();
    VITALOG("[HostsTab::HostsTab] Después de refreshHostsList\n");
}

brls::View* HostsTab::create() {
    return new HostsTab();
}

void HostsTab::refreshHostsList() {
    VITALOG("[HostsTab::refreshHostsList] INICIO\n");
    if (!this->hostsList) {
        VITALOG("[HostsTab::refreshHostsList] hostsList es nullptr, saliendo\n");
        return;
    }
    this->hostsList->clearViews(true);
    constexpr int CARDS_PER_ROW = 3;

    // --- Comprobación de hosts guardados ---
    auto hosts = HostStorage::loadHosts();
    if (hosts.empty()) {
        auto* emptyItem = new brls::Label();
        emptyItem->setText(brls::getStr("host_dialog/host_list_empty"));
        emptyItem->setFontSize(16);
        emptyItem->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        this->hostsList->addView(emptyItem);
        return;
    }
    int count = 0;
    brls::Box* row = nullptr;
    for (const auto& host : hosts) {
        // Logging multiplataforma
#ifdef __PSV__
        VITALOG("[HostsTab::refreshHostsList] Procesando host: %s (%s)\n", host.name, host.ip);
#else
        brls::Logger::info("[HostsTab::refreshHostsList] Procesando host: %s (%s)", host.name.c_str(), host.ip.c_str());
#endif
        if (count % CARDS_PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            this->hostsList->addView(row);
            VITALOG("[HostsTab::refreshHostsList] Nueva fila creada\n");
        }
        auto* card = new PCCard(host.name, "img/moonlight/pc.png");
        card->setClickAction([this, host, card]() {
            // Logging multiplataforma para click
            VITALOG("[PCCard] Click en card de host: %s (%s)\n", host.name, host.ip);
            auto* dialog = new brls::Dialog(brls::getStr("host_dialog/dialog/title"));
            
            // Evita que el diálogo se cierre automáticamente al pulsar un botón
            dialog->setCancelable(false);

            dialog->addButton(brls::getStr("host_dialog/dialog/connect"), [this, host](/*dialog*/) {
                sceClibPrintf("[PCCard] Conectar a %s (%s)\n", host.name, host.ip);
                brls::sync([this, host] {
                    this->present(new SessionAppSelect(host.name));
                });
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/info"), [dialog, host]() {
                sceClibPrintf("[PCCard] Info para %s (%s)\n", host.name, host.ip);
                // No cerrar el diálogo para evitar que aparezca el menú de cerrar app
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/settings"), [this, host](/*dialog*/) {
                sceClibPrintf("[PCCard] Settings para %s (%s)\n", host.name, host.ip);
                // El diálogo se cerrará automáticamente, solo abrir el dropdown en el siguiente frame
                brls::sync([this, host]() {
                    VITALOG("[hosts_tab.cpp] Entrando en brls::sync para crear Dropdown de settings para host: %s\n", host.name);
                    std::vector<std::string> options = {
                        brls::getStr("host_dialog/dropdown/delete"),
                        brls::getStr("host_dialog/dropdown/pair_online")
                    };
                    std::string dropdownTitle = brls::getStr("host_dialog/dropdown/title") + ": " + host.name;
                    VITALOG("[hosts_tab.cpp] Creando Dropdown con título: %s\n", dropdownTitle.c_str());
                    auto* dropdown = new brls::Dropdown(dropdownTitle, options, [this, host](int selected) {
                        VITALOG("[Dropdown] Opción seleccionada: %d para host: %s\n", selected, host.name);
                        if (selected == 0) {
                            brls::sync([this, host]() {
                                std::string confirmMsg = brls::getStr("host_dialog/confirm_delete_msg") + "\n" + host.name;
                                auto* confirm = new brls::Dialog(confirmMsg);
                                confirm->addButton(brls::getStr("host_dialog/yes"), [this, host, confirm]() {
                                    sceClibPrintf("[PCCard] Eliminando host: %s\n", host.name);
                                    HostStorage::removeHost(host.name);
                                    std::string msg = brls::getStr("host_dialog/notification_deleted");
                                    msg += ": ";
                                    msg += host.name;
                                    brls::Application::notify(msg);
                                    confirm->close();
                                    brls::sync([this]() {
                                        VITALOG("[PCCard] Llamando a refreshHostsList tras borrado\n");
                                        this->refreshHostsList();
                                        if (this->hostsList && !this->hostsList->getChildren().empty()) {
                                            brls::Application::giveFocus(this->hostsList->getChildren().front());
                                        }
                                    });
                                });
                                confirm->addButton(brls::getStr("host_dialog/no"), [confirm]() {
                                    VITALOG("[PCCard] Cancelado borrado de host\n");
                                    confirm->close();
                                });
                                confirm->open();
                            });
                        } else if (selected == 1) {
                            sceClibPrintf("[PCCard] Emparejar online fuera de casa: %s\n", host.name);
                            // Lógica real para emparejar online aquí
                        }
                    }, -1);
                    VITALOG("[hosts_tab.cpp] Pushing Activity con Dropdown para host: %s\n", host.name);
                    brls::Application::pushActivity(new brls::Activity(dropdown));
                });
            });
            // Añadir un botón para cerrar el diálogo explícitamente
            dialog->addButton(brls::getStr("main/cancel"), [dialog]() {
                dialog->close();
            });
            dialog->open();
        });
        if ((count + 1) % CARDS_PER_ROW != 0) {
            card->setMarginRight(16);
        }
        row->addView(card);
#ifdef __PSV__
        VITALOG("[HostsTab::refreshHostsList] Card añadida para host: %s\n", host.name);
#endif
        count++;
    }
}
