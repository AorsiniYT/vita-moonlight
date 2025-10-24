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

// (Se eliminó s_isRefreshing y vitaInstance; no eran usados)

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

