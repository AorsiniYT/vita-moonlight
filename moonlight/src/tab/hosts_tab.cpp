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
#include "model/pccard.hpp"
#include "model/HostStorage.hpp"


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
#ifdef __PSV__
    auto hosts = loadHostsVita();
    VITALOG("[HostsTab::refreshHostsList] loadHostsVita() llamado, hosts.size=%d\n", (int)hosts.size());
    if (hosts.empty()) {
        VITALOG("[HostsTab::refreshHostsList] No hay hosts guardados\n");
        auto* emptyItem = new brls::Label();
        emptyItem->setText(brls::getStr("host_dialog/host_list_empty"));
        emptyItem->setFontSize(16);
        emptyItem->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        this->hostsList->addView(emptyItem);
        return;
    }
#else
    auto hosts = loadHosts();
    if (hosts.empty()) {
        auto* emptyItem = new brls::Label();
        emptyItem->setText(brls::getStr("host_dialog/host_list_empty"));
        emptyItem->setFontSize(16);
        emptyItem->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        this->hostsList->addView(emptyItem);
        return;
    }
#endif
    int count = 0;
    brls::Box* row = nullptr;
#ifdef __PSV__
    for (const auto& host : hosts) {
        VITALOG("[HostsTab::refreshHostsList] Procesando host: %s (%s)\n", host.name, host.ip);
        if (count % CARDS_PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            this->hostsList->addView(row);
            VITALOG("[HostsTab::refreshHostsList] Nueva fila creada\n");
        }
        std::string name = host.name;
        std::string ip = host.ip;
        auto* card = new PCCard(name, "img/moonlight/pc.png");
        card->setClickAction([this, name, ip]() {
            VITALOG("[PCCard] Click en card de host: %s (%s)\n", name.c_str(), ip.c_str());
            auto* dialog = new brls::Dialog(brls::getStr("host_dialog/dialog/title"));
            dialog->addButton(brls::getStr("host_dialog/dialog/connect"), [name, ip, dialog]() {
                sceClibPrintf("[PCCard] Conectar a %s (%s)\n", name.c_str(), ip.c_str());
                dialog->close();
                // Lógica real de conexión aquí
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/info"), [name, ip, dialog]() {
                sceClibPrintf("[PCCard] Info para %s (%s)\n", name.c_str(), ip.c_str());
                // No hacer nada más al pulsar info
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/settings"), [this, name, ip, dialog]() {
                sceClibPrintf("[PCCard] Settings para %s (%s)\n", name.c_str(), ip.c_str());
                brls::sync([this, name, ip]() {
                    std::vector<std::string> options = {
                        brls::getStr("host_dialog/dropdown/delete"),
                        brls::getStr("host_dialog/dropdown/pair_online")
                    };
                    std::string dropdownTitle = brls::getStr("host_dialog/dropdown/title") + ": " + name;
                    auto* dropdown = new brls::Dropdown(dropdownTitle, options, [this, name, ip](int selected) {
                        VITALOG("[Dropdown] Opción seleccionada: %d para host: %s\n", selected, name.c_str());
                        if (selected == 0) {
                            brls::sync([this, name]() {
                                std::string confirmMsg = brls::getStr("host_dialog/confirm_delete_msg") + "\n" + name;
                                auto* confirm = new brls::Dialog(confirmMsg);
                                confirm->addButton(brls::getStr("host_dialog/yes"), [this, name, confirm]() {
                                    sceClibPrintf("[PCCard] Eliminando host: %s\n", name.c_str());
                                    bool ok = HostStorage::removeHost(name);
                                    VITALOG("[PCCard] HostStorage::removeHost devuelto: %d\n", ok);
                                    std::string msg = brls::getStr("host_dialog/notification_deleted");
                                    msg += ": ";
                                    msg += name;
                                    brls::Application::notify(msg);
                                    confirm->close();
                                    brls::sync([this]() {
                                        VITALOG("[PCCard] Llamando a refreshHostsList tras borrado\n");
                                        this->refreshHostsList();
                                    });
                                });
                                confirm->addButton(brls::getStr("host_dialog/no"), [confirm]() {
                                    VITALOG("[PCCard] Cancelado borrado de host\n");
                                });
                                confirm->open();
                            });
                        } else if (selected == 1) {
                            sceClibPrintf("[PCCard] Emparejar online fuera de casa: %s\n", name.c_str());
                        }
                    }, -1);
                    brls::Application::pushActivity(new brls::Activity(dropdown));
                });
            });
            dialog->open();
        });
        if ((count + 1) % CARDS_PER_ROW != 0) {
            card->setMarginRight(16);
        }
        row->addView(card);
        VITALOG("[HostsTab::refreshHostsList] Card añadida para host: %s\n", name.c_str());
        count++;
    }
#else
    for (const auto& host : hosts) {
        if (count % CARDS_PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            this->hostsList->addView(row);
        }
        auto* card = new PCCard(host.name, "img/moonlight/pc.png");
        card->setClickAction([this, host]() {
            // Mostrar el diálogo principal con acciones
            auto* dialog = new brls::Dialog(brls::getStr("host_dialog/dialog/title"));
            dialog->addButton(brls::getStr("host_dialog/dialog/connect"), [host, dialog]() {
                brls::Logger::info("[PCCard] Conectar a %s (%s)", host.name.c_str(), host.ip.c_str());
                dialog->close();
                // Lógica real de conexión aquí
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/info"), [host, dialog]() {
                brls::Logger::info("[PCCard] Info para %s (%s)", host.name.c_str(), host.ip.c_str());
                // No hacer nada más al pulsar info
            });
            dialog->addButton(brls::getStr("host_dialog/dialog/settings"), [this, host, dialog]() {
                brls::Logger::info("[PCCard] Settings para %s (%s)", host.name.c_str(), host.ip.c_str());
                brls::sync([this, host]() {
                    // Dropdown contextual sin selección previa y con claves traducibles
                    std::vector<std::string> options = {
                        brls::getStr("host_dialog/dropdown/delete"),
                        brls::getStr("host_dialog/dropdown/pair_online")
                    };
                    std::string dropdownTitle = brls::getStr("host_dialog/dropdown/title") + ": " + host.name;
                    // Creamos el Dropdown y lo mostramos como actividad
                    auto* dropdown = new brls::Dropdown(dropdownTitle, options, [this, host](int selected) {
                        if (selected == 0) {
                            // Cerrar el Dropdown antes de mostrar el diálogo de confirmación
                            brls::sync([this, host]() {
                                std::string confirmMsg = brls::getStr("host_dialog/confirm_delete_msg") + "\n" + host.name;
                                auto* confirm = new brls::Dialog(confirmMsg);
                                confirm->addButton(brls::getStr("host_dialog/yes"), [this, host, confirm]() {
                                    brls::Logger::info("[PCCard] Eliminando host: %s", host.name.c_str());
                                    HostStorage::removeHost(host.name);
                                    // Notificación de éxito
                                    std::string msg = brls::getStr("host_dialog/notification_deleted");
                                    msg += ": ";
                                    msg += host.name;
                                    brls::Application::notify(msg);
                                    brls::sync([this]() {
                                        this->refreshHostsList();
                                    });
                                });
                                confirm->addButton(brls::getStr("host_dialog/no"), [confirm]() {
                                });
                                confirm->open();
                            });
                        } else if (selected == 1) {
                            brls::Logger::info("[PCCard] Emparejar online fuera de casa: %s", host.name.c_str());
                            // Lógica real para emparejar online aquí
                        }
                    }, -1);
                    // Cuando se selecciona una opción, el Dropdown se cierra automáticamente antes de ejecutar el callback
                    brls::Application::pushActivity(new brls::Activity(dropdown));
                });
            });
            dialog->open();
        });
        if ((count + 1) % CARDS_PER_ROW != 0) {
            card->setMarginRight(16);
        }
        row->addView(card);
        count++;
    }
#endif
}
