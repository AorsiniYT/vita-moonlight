/*
 * hosts_tab.cpp
 * Implementación segura y limpia de HostsTab
 */

#include "tab/hosts_tab.hpp"
#include "utils/host_search.hpp"
#include "view/pccard.hpp"
#include "model/HostStorage.hpp"
#include "connection_manager.hpp"
#include "session/session_app_select.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <borealis/core/thread.hpp>
#include <thread>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <cctype>
#include <memory>

#ifdef _WIN32
#include <direct.h>
#endif

#ifdef __PSV__
#include <psp2/kernel/clib.h>
#define VITALOG sceClibPrintf
#else
#define VITALOG(...) ((void)0)
#endif

#include "debug.hpp"
#include "activity/main_activity.hpp"

// Definición de la bandera estática
std::atomic<bool> HostsTab::s_isRefreshing{false};
// Definición de la instancia estática para discovery
HostsTab* HostsTab::vitaInstance = nullptr;

namespace {
std::string trim_copy(const std::string& value) {
    auto b = value.begin();
    auto e = value.end();
    while (b != e && std::isspace(static_cast<unsigned char>(*b))) ++b;
    while (e != b && std::isspace(static_cast<unsigned char>(*(e - 1)))) --e;
    return std::string(b, e);
}
}

HostsTab::HostsTab() {
    VITALOG("[HostsTab::HostsTab] Constructor llamado\n");
    this->inflateFromXMLRes("xml/tabs/hosts_tab.xml");
    this->refreshHostsList();

    const std::string refreshKey = "host_dialog/actions/refresh";

    auto actionId = this->registerAction(brls::getStr(refreshKey), brls::BUTTON_Y, [this](brls::View*) {
#ifdef __PSV__
        VITALOG("[HostsTab] Triangle action callback invoked (debug)\n");
        vita_debug_log("[HostsTab] Triangle pressed - triggering updateHostsGrid()\n");
#endif
    // Avoid using the old in-place refresher that caused crashes on PSVita.
    // Request a full reload of the MainActivity content instead.
    HostsTab::requestGlobalRefresh();
        return true;
    });

#ifdef __PSV__
    VITALOG("[HostsTab] Triangle action registered id=%d\n", actionId);
#endif
    this->initialized = true;
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

    auto hosts = HostStorage::loadHosts();
    brls::Logger::info("[HostsTab::refreshHostsList] Total hosts cargados desde HostStorage: {}", hosts.size());
#ifdef __PSV__
    VITALOG("[HostsTab::refreshHostsList] Total hosts cargados desde HostStorage: %d\n", (int)hosts.size());
#endif
    if (hosts.empty()) {
        auto* emptyItem = new brls::Label();
        emptyItem->setText(brls::getStr("host_dialog/host_list_empty"));
        emptyItem->setFontSize(16);
        emptyItem->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        this->hostsList->addView(emptyItem);
        return;
    }

    // Modo diagnóstico: si está activo, usamos labels simples en lugar de PCCard
    // Cambiar a 'false' para usar PCCard en producción
    static const bool DIAG_USE_SIMPLE_CARDS = false;
    int count = 0;
    brls::Box* row = nullptr;
    for (const auto& host : hosts) {
        // Log detallado por host para diagnóstico
    #if defined(__PSV__)
        // HostInfo fields: name, ip, port, paired, safeId
        brls::Logger::info("[HostsTab::refreshHostsList] Procesando host: name='{}' ip='{}' safeId='{}' paired='{}' port='{}'",
                            host.name, host.ip, host.safeId, host.paired ? "true" : "false", host.port);
    #else
        brls::Logger::info("[HostsTab::refreshHostsList] Procesando host: name='{}' ip='{}' safeId='{}' paired='{}' port='{}'",
                            host.name, host.ip, host.safeId, host.paired ? "true" : "false", host.port);
    #endif
#ifdef __PSV__
        VITALOG("[HostsTab::refreshHostsList] Procesando host: %s (%s)\n", host.name.c_str(), host.ip.c_str());
#endif
        brls::Logger::info("[HostsTab::refreshHostsList] diag: before creating row/card for %s", host.name);
        if (count % CARDS_PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            this->hostsList->addView(row);
            brls::Logger::info("[HostsTab::refreshHostsList] diag: added new row %p", (void*)row);
        }
        brls::View* cardView = nullptr;
        auto* card = new PCCard(host.name, "img/moonlight/pc.png");
        cardView = card;
        brls::Logger::info("[HostsTab::refreshHostsList] diag: created PCCard %p for %s", (void*)card, host.name);
        // Agregar márgenes y añadir a la fila antes de asignar callbacks para evitar dependencias de scope
        if ((count + 1) % CARDS_PER_ROW != 0) {
            if (auto c = dynamic_cast<brls::Label*>(cardView)) {
                // Labels no tienen setMarginRight, ignorar
            } else if (auto c2 = dynamic_cast<PCCard*>(cardView)) {
                c2->setMarginRight(16);
            }
        }
        row->addView(cardView);
        brls::Logger::info("[HostsTab::refreshHostsList] diag: added cardView %p to row %p", (void*)cardView, (void*)row);
        ++count;

        // Si usamos PCCard real, configurar la acción; si usamos label, dejamos simple.
        if (!DIAG_USE_SIMPLE_CARDS) {
            // Crear copias de los campos necesarios para evitar capturas problemáticas
            std::string hostNameCopy = host.name;
            std::string hostIpCopy = host.ip;
            std::string hostSafeIdCopy = host.safeId;
            auto* realCard = static_cast<PCCard*>(cardView);
            realCard->setClickAction([this, hostNameCopy, hostIpCopy, hostSafeIdCopy]() {
            VITALOG("[PCCard] Click en card de host: %s (%s)\n", hostNameCopy.c_str(), hostIpCopy.c_str());
            auto* dialog = new brls::Dialog(brls::getStr("host_dialog/dialog/title"));
            dialog->setCancelable(false);

            dialog->addButton(brls::getStr("host_dialog/dialog/connect"), [this, hostNameCopy]() {
#ifdef __PSV__
                sceClibPrintf("[PCCard] Conectar a %s\n", hostNameCopy.c_str());
#endif
                brls::sync([this, hostNameCopy] { this->present(new SessionAppSelect(hostNameCopy)); });
            });

            dialog->addButton(brls::getStr("host_dialog/dialog/info"), [hostNameCopy]() {
#ifdef __PSV__
                sceClibPrintf("[PCCard] Info para %s\n", hostNameCopy.c_str());
#endif
            });

            dialog->addButton(brls::getStr("host_dialog/dialog/settings"), [this, hostNameCopy, hostIpCopy, hostSafeIdCopy, dialog]() {
#ifdef __PSV__
                sceClibPrintf("[PCCard] Settings para %s (%s)\n", hostNameCopy.c_str(), hostIpCopy.c_str());
#endif
                std::vector<std::string> options = {
                    brls::getStr("host_dialog/dropdown/change_ip"),
                    brls::getStr("host_dialog/dropdown/delete"),
                    brls::getStr("host_dialog/dropdown/pair_online")
                };
                std::string dropdownTitle = brls::getStr("host_dialog/dropdown/title") + ": " + hostNameCopy;
                auto* dropdown = new brls::Dropdown(dropdownTitle, options, [this, hostNameCopy, hostIpCopy, hostSafeIdCopy, dialog](int selected) {
                    if (selected == 0) {
                        auto ime = brls::Application::getImeManager();
                        if (!ime) {
                            brls::Logger::error("[HostsTab] ImeManager no disponible para cambiar IP");
                            brls::Application::notify(brls::getStr("host_dialog/change_ip_failure"));
                            return;
                        }
                        std::string currentIp = hostIpCopy;
                        ime->openForText([this, hostNameCopy, currentIp, hostSafeIdCopy](const std::string& input) {
                            std::string trimmed = trim_copy(input);
                            if (trimmed.empty()) {
                                brls::Application::notify(brls::getStr("host_dialog/change_ip_empty_error"));
                                return;
                            }
                            if (trimmed == currentIp) {
                                brls::Application::notify(brls::getStr("host_dialog/change_ip_same"));
                                return;
                            }
                            std::string targetName = !hostSafeIdCopy.empty() ? hostSafeIdCopy : hostNameCopy;
                            if (HostStorage::updateHostIp(targetName, trimmed)) {
                                brls::Application::notify(brls::getStr("host_dialog/change_ip_success"));
                                brls::sync([this]() { this->refreshHostsList(); });
                            } else {
                                brls::Application::notify(brls::getStr("host_dialog/change_ip_failure"));
                            }
                        },
                        brls::getStr("host_dialog/change_ip_title"),
                        brls::getStr("host_dialog/change_ip_hint"),
                        64,
                        currentIp,
                        0);

                    } else if (selected == 1) {
                        brls::sync([this, hostNameCopy, dialog]() {
                            std::string confirmMsg = brls::getStr("host_dialog/confirm_delete_msg") + "\n" + hostNameCopy;
                            auto* confirm = new brls::Dialog(confirmMsg);
                            confirm->setCancelable(false);
                            {
                                std::string hostToRemove = hostNameCopy;
                                confirm->addButton(brls::getStr("host_dialog/yes"), [hostToRemove]() {
#ifdef __PSV__
                                    sceClibPrintf("[PCCard] Eliminando host: %s\n", hostToRemove.c_str());
#endif
                                        brls::sync([]() {
                                            auto activities = brls::Application::getActivitiesStack();
                                            if (!activities.empty()) {
                                                brls::Activity* top = activities.back();
                                                if (top) {
                                                    brls::View* content = top->getContentView();
                                                    HostsTab* hostsTab = dynamic_cast<HostsTab*>(content);
                                                    if (hostsTab && hostsTab->hostsList) {
                                                        auto* s = new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
                                                        s->setMarginTop(12);
                                                        s->setMarginBottom(12);
                                                        hostsTab->hostsList->addView(s);
                                                    }
                                                }
                                            }
                                        });

                                        std::thread([hostToRemove]() {
                                            bool ok = HostStorage::removeHost(hostToRemove);
                                            brls::sync([hostToRemove, ok]() {
                                                if (ok) {
                                                    std::string msg = brls::getStr("host_dialog/notification_deleted") + ": " + hostToRemove;
                                                    brls::Application::notify(msg);
                                                } else {
                                                    brls::Application::notify(brls::getStr("host_dialog/change_ip_failure"));
                                                }

                                                // Independientemente de la activity superior (puede ser el diálogo),
                                                // solicitar una recarga global segura del home en el hilo UI.
                                                HostsTab::requestGlobalRefresh();
                                            });
                                        }).detach();
                                });
                            }
                            confirm->addButton(brls::getStr("host_dialog/no"), []() {});
                            confirm->open();
                        });

                    } else if (selected == 2) {
#ifdef __PSV__
                        sceClibPrintf("[PCCard] Emparejar online fuera de casa: %s\n", hostNameCopy.c_str());
#endif
                    }
                }, -1);

                brls::Application::pushActivity(new brls::Activity(dropdown));
            });
            dialog->addButton(brls::getStr("main/cancel"), []() {});
            dialog->open();
            });
        } // end if real card actions
    }
}

void HostsTab::updateHostsGrid()
{
    // Deprecated: the old in-place refresher caused intermittent coredumps on PSVita.
    // Delegate to a safe full reload of the main activity instead.
    VITALOG("[HostsTab::updateHostsGrid] deprecated, delegating to requestGlobalRefresh()\n");
    HostsTab::requestGlobalRefresh();
}

void HostsTab::requestGlobalRefresh()
{
    // Safer approach: clear the activities stack and push a fresh MainActivity.
    // Use a short delayed task so we don't delete the current activity while
    // still handling its input/event (prevents reentrancy crashes on Vita).
    brls::delay(50, []() {
        brls::Logger::info("[HostsTab::requestGlobalRefresh] Pushing new MainActivity (safe reload)");
        // Push a fresh MainActivity on top. We avoid clearing the whole stack because
        // Application::clear() is private. This may leave the old MainActivity below,
        // but pushing a fresh one provides a clean UI state without in-place mutations
        // that previously caused crashes on PSVita.
        brls::Application::pushActivity(new MainActivity(), brls::TransitionAnimation::NONE);
        brls::Logger::info("[HostsTab::requestGlobalRefresh] New MainActivity pushed");
    });
}

void HostsTab::startDeviceDiscovery()
{
    VITALOG("[HostsTab::startDeviceDiscovery] start\n");
    brls::Application::notify("[DEBUG] startDeviceDiscovery entered");
    brls::Logger::info("[HostsTab::startDeviceDiscovery] (logger) invoked on %p", (void*)this);
#if defined(__PSV__)
    VITALOG("[HostsTab] isVitaDiscoveryActive() = %d\n", (int)check_host::isVitaDiscoveryActive());
    brls::Logger::info("[HostsTab] startDeviceDiscovery invoked; isVitaDiscoveryActive={}", check_host::isVitaDiscoveryActive());
#endif
    brls::sync([this]() {
        brls::View* spinnerRow = this->getView("spinner_row");
        if (!this->hostsList) return;
        auto children = this->hostsList->getChildren();
        for (auto* child : children) {
            if (!spinnerRow || child != spinnerRow)
                this->hostsList->removeView(child);
        }
        if (spinnerRow) spinnerRow->setVisibility(brls::Visibility::VISIBLE);
    });

#if defined(__PSV__)
    static std::vector<std::pair<std::string, std::string>> discoveredHosts; // name, ip
    discoveredHosts.clear();
    HostsTab::vitaInstance = this;
    brls::Logger::info("[HostsTab] startDeviceDiscovery: vitaInstance set to %p", (void*)HostsTab::vitaInstance);
    vita_debug_log("[HostsTab] About to call check_host::startVitaDiscovery()\n");
    check_host::startVitaDiscovery([](int idx, const char* host, const char* pcname, const char* ip, int port) {
        brls::Logger::info("[HostsTab] discovery callback VITA invoked: idx={} pcname={} ip={} port={} host={}", idx, pcname?pcname:"(null)", ip?ip:"(null)", port, host?host:"(null)");
        VITALOG("[HostsTab] discovery callback VITA invoked: idx=%d pcname=%s ip=%s port=%d host=%s\n", idx, pcname?pcname:"(null)", ip?ip:"(null)", port, host?host:"(null)");
        std::string name(pcname ? pcname : "");
        std::string ipStr(ip ? ip : "");
        std::string displayName = name.empty() ? ipStr : name;
        for (const auto& h : discoveredHosts) {
            if (h.first == displayName && h.second == ipStr) return;
        }
        discoveredHosts.push_back({displayName, ipStr});

        brls::sync([displayName, ipStr]() {
            auto activities = brls::Application::getActivitiesStack();
            for (auto it = activities.rbegin(); it != activities.rend(); ++it) {
                brls::Activity* act = *it;
                if (!act) continue;
                brls::View* content = act->getContentView();
                HostsTab* hostsTab = dynamic_cast<HostsTab*>(content);
                if (!hostsTab || !hostsTab->hostsList) continue;

                brls::View* spinnerRow = hostsTab->getView("spinner_row");
                auto children = hostsTab->hostsList->getChildren();
                for (auto* child : children) {
                    if (!spinnerRow || child != spinnerRow)
                        hostsTab->hostsList->removeView(child);
                }

                const int cardsPerRow = 3;
                int count = 0;
                brls::Box* currentRow = nullptr;
                for (const auto& h : discoveredHosts) {
                    if (count % cardsPerRow == 0) {
                        currentRow = new brls::Box(brls::Axis::ROW);
                        currentRow->setMarginBottom(16);
                        hostsTab->hostsList->addView(currentRow);
                    }
                    auto* card = new PCCard(h.first.c_str(), "img/moonlight/pc.png");
                    card->setFocusable(true);
                    card->setMarginRight(16);
                    card->setMarginBottom(0);
                    std::string ipCopy = h.second;
                    std::string nameCopy = h.first;
                    card->setClickAction([hostsTab, ipCopy, nameCopy]() {
                        std::string msg = brls::getStr("moonlight/settings/add_host_connect_question_dialog");
                        size_t pos_ip = msg.find("$(ip)");
                        if (pos_ip != std::string::npos) msg.replace(pos_ip, 5, ipCopy);
                        size_t pos_name = msg.find("$(name)");
                        if (pos_name != std::string::npos) msg.replace(pos_name, 7, nameCopy);
                        auto* dialog = new brls::Dialog(msg);
                        dialog->addButton(brls::getStr("moonlight/settings/add_host_connect"), [hostsTab, nameCopy]() {
                            brls::sync([hostsTab, nameCopy]() { hostsTab->present(new SessionAppSelect(nameCopy)); });
                        });
                        dialog->addButton(brls::getStr("moonlight/settings/add_host_cancel"), []() {});
                        dialog->open();
                    });
                    if (currentRow) currentRow->addView(card);
                    count++;
                }

                if (spinnerRow) spinnerRow->setVisibility(brls::Visibility::VISIBLE);
                break;
            }
        });
    });
#endif

}
