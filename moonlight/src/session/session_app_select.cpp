/*
    session_app_select.cpp - Selección de aplicación para iniciar streaming en Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include "session/session_app_select.hpp"
#include <sys/stat.h>
#include "view/pccard.hpp"
#include "utils/dialog_utils.h"
#include "model/HostStorage.hpp"
#include "GameStreamClient.hpp"
#include "session/streaming_manager.hpp"
#include "session/session_main.hpp"
#include "session/vita_session.hpp"
#include "ConfigManager.hpp"
#include "GameStreamClient.hpp"
#include "client.h"
#include "Limelight.h"
#include <cstdlib>  // Para malloc/free
#include <cstring>  // Para strcpy
#include <string>   // Para std::string
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>

using namespace brls::literals;


SessionAppSelect::SessionAppSelect(const std::string& hostName)
    : brls::Box(brls::Axis::COLUMN), host() {
    brls::Logger::info("View: SessionAppSelect para host: %s", hostName.c_str());

    // Buscar el host real por nombre
    auto found = HostStorage::findHost(hostName);
    if (found) {
        this->host = *found;
    } else {
        brls::Logger::error("[SessionAppSelect] No se encontró el host '%s' en HostStorage", hostName.c_str());
        // Dejar host vacío, mostrará error en populateAppList
    }

    this->inflateFromXMLRes("xml/views/session_app_select.xml");

    // Configurar títulos
    app_select_title->setText(hostName);
    app_select_subtitle->setText(brls::getStr("moonlight/session/app_select/subtitle"));

    // Crear y configurar el GridView dinámicamente
    gridView = new GridView();
    gridView->setColumns(3);  // 3 columnas para PS Vita (mejor ajuste)
    gridView->setWidth(brls::Box::AUTO);
    gridView->setGrow(1.0f);
    grid_placeholder->addView(gridView);

    // Configurar el spinner con tamaño apropiado
    BRLS_BIND(brls::ProgressSpinner, loading_spinner, "loading_spinner");
    spinner = loading_spinner;
    if (!spinner) {
        spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::NORMAL);
    }

    // Comprobar si existe una sesión activa — delegando la lógica a GameStreamClient
    HostInfo hostCopy = this->host;
    brls::async([this, hostCopy]() mutable {
        RemoteAppInfo running;
        bool active = GameStreamClient::instance().probeActiveSession(hostCopy, running);
        if (active) {
            brls::sync([this, running, hostCopy]() {
                brls::Logger::info("[SessionAppSelect] Sesión activa detectada para {} -> mostrar diálogo Reanudar/Empezar nuevo", hostCopy.ip);
                auto* holder = new brls::Box(brls::Axis::COLUMN);
                auto* label = new brls::Label();
                std::string msg = brls::getStr("moonlight/session/app_select/active_session_msg");
                size_t pos = msg.find("$(app)");
                if (pos != std::string::npos) msg.replace(pos, 6, running.name);
                label->setText(msg);
                label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
                label->setMarginBottom(18);

                auto* btnBox = new brls::Box(brls::Axis::ROW);
                btnBox->setJustifyContent(brls::JustifyContent::CENTER);
                btnBox->setAlignItems(brls::AlignItems::CENTER);

                auto* resumeBtn = new brls::Button();
                resumeBtn->setText(brls::getStr("moonlight/session/app_select/button_resume"));
                resumeBtn->setStyle(&brls::BUTTONSTYLE_HIGHLIGHT);
                resumeBtn->setMargins(0, 8, 0, 0);

                auto* newBtn = new brls::Button();
                newBtn->setText(brls::getStr("moonlight/session/app_select/button_start_new"));
                newBtn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

                btnBox->addView(resumeBtn);
                btnBox->addView(newBtn);
                holder->addView(label);
                holder->addView(btnBox);
                holder->setPadding(18,18,18,18);

                auto* dialog = new brls::Dialog(holder);
                dialog->setCancelable(true);
                dialog->open();

                resumeBtn->registerClickAction([this, running, dialog, hostCopy](brls::View*) -> bool {
                    dialog->dismiss();
                    // Reanudar la sesión activa llamando a AppSelected con la app en ejecución
                    this->AppSelected(running);
                    return true;
                });

                newBtn->registerClickAction([this, running, dialog, hostCopy](brls::View*) -> bool {
                    dialog->dismiss();
                    auto* waitDlg = createLoadingDialog(brls::getStr("moonlight/session/app_select/ending_session"));
                    std::thread([hostCopy, this, waitDlg]() mutable {
                        brls::Logger::info("[SessionAppSelect] Enviando quitApp para {} antes de mostrar apps", hostCopy.ip);
                        if (!GameStreamClient::instance().isConnected(hostCopy.ip)) {
                            GameStreamClient::instance().connect(hostCopy);
                        }
                        bool ok = GameStreamClient::instance().quitApp(hostCopy.ip);
                        brls::sync([this, ok, waitDlg]() {
                            if (waitDlg) waitDlg->close();
                            if (!ok) {
                                brls::Application::notify(brls::getStr("moonlight/session/app_select/error_end_session"));
                            }
                            this->populateAppList();
                        });
                    }).detach();
                    return true;
                });
            });
        } else {
            // Si probeActiveSession devolvió false, puede deberse a que el servidor
            // no reconoce el emparejamiento aunque localmente device.ini marque paired=true.
            // En ese caso queremos alertar al usuario y ofrecer opciones de reparación.
            brls::sync([this, hostCopy]() {
                // Asegurarnos de tener serverData si estamos conectados
                if (GameStreamClient::instance().isConnected(hostCopy.ip)) {
                    SERVER_DATA& sd = GameStreamClient::instance().serverData(hostCopy.ip);
                    // Caso: localmente paired pero servidor no lo reconoce y hay currentGame activo
                    if (hostCopy.paired && !sd.paired && sd.currentGame != 0) {
                        brls::Logger::info("[SessionAppSelect] Inconsistencia: device.ini indica paired pero servidor PairStatus=0 y currentGame={}", sd.currentGame);
                        auto* holder = new brls::Box(brls::Axis::COLUMN);
                        auto* label = new brls::Label();
                        std::string msg = brls::getStr("host_dialog/active_session_mismatch");
                        size_t pos = msg.find("$(app)");
                        std::string appName = std::to_string(sd.currentGame);
                        if (pos != std::string::npos) msg.replace(pos, 6, appName);
                        label->setText(msg);
                        label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
                        label->setMarginBottom(18);

                        auto* btnBox = new brls::Box(brls::Axis::ROW);
                        btnBox->setJustifyContent(brls::JustifyContent::CENTER);
                        btnBox->setAlignItems(brls::AlignItems::CENTER);

                        auto* repairBtn = new brls::Button();
                        repairBtn->setText(brls::getStr("host_dialog/button_repair_pair"));
                        repairBtn->setStyle(&brls::BUTTONSTYLE_HIGHLIGHT);
                        repairBtn->setMargins(0, 8, 0, 0);

                        auto* forgetBtn = new brls::Button();
                        forgetBtn->setText(brls::getStr("host_dialog/button_forget_host"));
                        forgetBtn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

                        btnBox->addView(repairBtn);
                        btnBox->addView(forgetBtn);
                        holder->addView(label);
                        holder->addView(btnBox);
                        holder->setPadding(18,18,18,18);

                        auto* dialog = new brls::Dialog(holder);
                        dialog->setCancelable(true);
                        dialog->open();

                        repairBtn->registerClickAction([this, dialog, hostCopy](brls::View*) -> bool {
                            dialog->dismiss();
                            // Intentar reparar emparejamiento
                            brls::Application::notify(brls::getStr("host_dialog/repairing_pair"));
                            GameStreamClient::instance().beginPairing(hostCopy, [this](bool ok){
                                brls::sync([this, ok]() {
                                    if (ok) {
                                        brls::Application::notify(brls::getStr("moonlight/session/app_select/paired"));
                                        // Re-llenar la lista ahora que puede estar emparejado
                                        this->populateAppList();
                                    } else {
                                        brls::Application::notify(brls::getStr("moonlight/session/app_select/pairing_failed"));
                                    }
                                });
                            });
                            return true;
                        });

                        forgetBtn->registerClickAction([this, dialog, hostCopy](brls::View*) -> bool {
                            dialog->dismiss();
                            // Olvidar host localmente para forzar un nuevo flujo de pair más limpio
                            bool removed = HostStorage::removeHost(hostCopy.name);
                            if (removed) {
                                brls::Application::notify(brls::getStr("moonlight/session/app_select/forgot_host"));
                                // Volver a la lista de hosts (cerrar vista actual)
                                brls::Application::popActivity();
                            } else {
                                brls::Application::notify(brls::getStr("moonlight/session/app_select/forget_failed"));
                                this->populateAppList();
                            }
                            return true;
                        });

                        return;
                    }
                }
                this->populateAppList();
            });
        }
    });
}

SessionAppSelect::~SessionAppSelect() {
    // El destructor de la vista se encarga de liberar los hijos (gridView, spinner, etc)
}

void SessionAppSelect::onLayout() {
    Box::onLayout();
}

void SessionAppSelect::populateAppList() {
    brls::Logger::info("[SessionAppSelect] populateAppList llamado para host: {} (ip: {})", host.name, host.ip);

    // Mostrar spinner y ocultar contenido
    if (spinner) spinner->setVisibility(brls::Visibility::VISIBLE);
    if (gridView) gridView->setVisibility(brls::Visibility::INVISIBLE);
    if (app_select_empty) app_select_empty->setVisibility(brls::Visibility::GONE);

    if (host.name.empty() || host.ip.empty()) {
        if (spinner) spinner->setVisibility(brls::Visibility::GONE);
        if (app_select_empty) {
            app_select_empty->setText(brls::getStr("moonlight/session/app_select/error_no_host"));
            app_select_empty->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }

    brls::async([this]() {
        brls::Logger::info("[SessionAppSelect] Llamando a ConnectionManager::fetchRemoteApps para host: {} (ip: {})", host.name, host.ip);
        std::vector<RemoteAppInfo> apps = ConnectionManager::fetchRemoteApps(this->host);
        brls::Logger::info("[SessionAppSelect] fetchRemoteApps devolvió {} apps", apps.size());
        for (const auto& app : apps) {
            brls::Logger::info("[SessionAppSelect] App recibida: id='{}', name='{}', iconUrl='{}'", app.id, app.name, app.iconUrl);
        }
        brls::sync([this, apps]() {
            if (spinner) spinner->setVisibility(brls::Visibility::GONE);
            if (apps.empty()) {
                brls::Logger::info("[SessionAppSelect] No se encontraron aplicaciones en este host.");
                if (app_select_empty) {
                    app_select_empty->setText(brls::getStr("moonlight/session/app_select/no_apps"));
                    app_select_empty->setVisibility(brls::Visibility::VISIBLE);
                }
                return;
            }

            // Preparar datos para el GridView
            std::vector<std::string> appNames;
            std::vector<std::string> appIcons;
            for (const auto& app : apps) {
                appNames.push_back(app.name);
                appIcons.push_back("img/moonlight/pc.png");  // Icono genérico por ahora
            }

            // Configurar el GridView con los datos
            if (gridView) {
                gridView->setItems(appNames, appIcons);
                gridView->setOnItemSelect([this, apps](int index) {
                    if (index >= 0 && index < (int)apps.size()) {
                        this->AppSelected(apps[index]);
                    }
                });
                gridView->setVisibility(brls::Visibility::VISIBLE);

                // Dar foco al primer elemento después de un breve delay
                brls::async([this]() {
                    brls::sync([this]() {
                        // Intentar dar foco al GridView primero
                        if (gridView) {
                            brls::Application::giveFocus(gridView);
                            brls::Logger::info("[SessionAppSelect] Foco dado al GridView");
                        }
                    });
                });
            }
        });
    });
}

void SessionAppSelect::AppSelected(const RemoteAppInfo& app) {
    brls::Logger::info("App seleccionada: {} (ID: {})", app.name, app.id);

    // Preparar configuración de streaming para PS Vita
    STREAM_CONFIGURATION streamConfig;
    memset(&streamConfig, 0, sizeof(streamConfig));
    
    // Cargar configuración del usuario
    ConfigManager configManager;
    if (!configManager.load()) {
        brls::Logger::warning("[SessionAppSelect] No se pudo cargar configuración, usando valores por defecto");
    }
    
    StreamConfiguration streamSettings = configManager.getStreamConfig();
    VideoSettings videoSettings = configManager.getVideoSettings();
    
    // Debug: mostrar valores leídos de configuración
    brls::Logger::info("[SessionAppSelect] Configuración leída:");
    brls::Logger::info("[SessionAppSelect] - Stream: {}x{} @ {}fps, bitrate={}", 
                      streamSettings.width, streamSettings.height, streamSettings.fps, streamSettings.bitrate);
    brls::Logger::info("[SessionAppSelect] - Video: render_mode={}", videoSettings.render_mode);
    
    // Usar configuración del usuario o valores por defecto
    streamConfig.width = streamSettings.width > 0 ? streamSettings.width : 960;
    streamConfig.height = streamSettings.height > 0 ? streamSettings.height : 544;
    streamConfig.fps = streamSettings.fps > 0 ? streamSettings.fps : 30;
    
    // Calcular bitrate: si es automático (-1), usar fórmula basada en resolución y fps
    if (streamSettings.bitrate == -1) {
        // Fórmula aproximada: (ancho * alto * fps * bits_per_pixel) / 1000000 para Mbps
        // Usando 0.2 bits por pixel como aproximación conservadora para H.264
        // Calcular paso a paso para evitar pérdida de precisión
        long long total_pixels = (long long)streamConfig.width * streamConfig.height * streamConfig.fps;
        long long total_bits_per_second = (total_pixels * 2) / 10; // 0.2 * total_pixels = (2/10) * total_pixels
        int calculatedBitrate = (int)(total_bits_per_second / 1000); // Convertir a Kbps directamente
        // Limitar a un rango razonable
        streamConfig.bitrate = std::max(5000, std::min(50000, calculatedBitrate));
    } else {
        streamConfig.bitrate = streamSettings.bitrate > 0 ? streamSettings.bitrate : 8000;
    }
    
    streamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    streamConfig.encryptionFlags = ENCFLG_NONE;  // Sin encriptación por ahora

    brls::Logger::info("[SessionAppSelect] Configuración de streaming:");
    brls::Logger::info("[SessionAppSelect] - Resolución: {}x{}", streamConfig.width, streamConfig.height);
    brls::Logger::info("[SessionAppSelect] - FPS: {}", streamConfig.fps);
    brls::Logger::info("[SessionAppSelect] - Bitrate: {} Kbps", streamConfig.bitrate);

    // Asegurar packetSize y flags correctos si no han sido establecidos
    if (streamConfig.packetSize <= 0) {
        streamConfig.packetSize = 1024; // valor seguro por defecto
        brls::Logger::info("[SessionAppSelect] packetSize no definido -> usando 1024");
    }
    if (streamConfig.streamingRemotely == 0 && streamConfig.streamingRemotely != STREAM_CFG_LOCAL && streamConfig.streamingRemotely != STREAM_CFG_REMOTE && streamConfig.streamingRemotely != STREAM_CFG_AUTO) {
        streamConfig.streamingRemotely = STREAM_CFG_AUTO; // dejar al core decidir
        brls::Logger::info("[SessionAppSelect] streamingRemotely no definido -> AUTO");
    }
    if (streamConfig.supportedVideoFormats == 0) {
#ifdef VIDEO_FORMAT_H264
        streamConfig.supportedVideoFormats = VIDEO_FORMAT_H264;
        brls::Logger::info("[SessionAppSelect] supportedVideoFormats vacío -> set H264");
#endif
    }
    brls::Logger::info("[SessionAppSelect] - packetSize: {} streamingRemotely={} formats=0x{:X}",
                      streamConfig.packetSize, streamConfig.streamingRemotely, streamConfig.supportedVideoFormats);

    // Inicializar servidor usando GameStreamClient (patrón Moonlight-Switch)

    // Bloquear inputs para que el usuario no pueda navegar mientras conectamos
    brls::Application::blockInputs();
    brls::Visibility prevGridVis = brls::Visibility::GONE;
    if (this->gridView) {
        // Ocultar el selector por completo (no solo quitar el foco)
        prevGridVis = this->gridView->getVisibility();
        this->gridView->setVisibility(brls::Visibility::GONE);
        // También ocultar el highlight si por alguna razón queda visible
        this->gridView->setHideHighlight(true);
        this->gridView->setHideHighlightBackground(true);
        this->gridView->setHideHighlightBorder(true);
        // Evitar que cualquier hijo reciba foco o dibuje su highlight (quita el puntito azul)
        this->gridView->setFocusable(false);
        // Quitar foco globalmente para evitar que quede un elemento enfocado
        brls::Application::giveFocus(nullptr);
        // Ocultar highlight y foco de todos los descendientes (filas -> cards -> inner views)
        std::function<void(brls::View*)> walkHide;
        walkHide = [&walkHide](brls::View* v) {
            if (!v) return;
            v->setHideHighlight(true);
            v->setHideHighlightBackground(true);
            v->setHideHighlightBorder(true);
            v->setFocusable(false);
            // Solo Box (contenedores) exponen getChildren
            brls::Box* box = dynamic_cast<brls::Box*>(v);
            if (!box) return;
            auto ch = box->getChildren();
            for (auto* cc : ch) walkHide(cc);
        };
        auto rows = this->gridView->getChildren();
        for (auto* row : rows) walkHide(row);
    }

    // Mostrar diálogo modal con spinner indicando el progreso de conexión
    std::string connectingMsg = brls::getStr("moonlight/session/app_select/connecting");
    auto* loadingDialog = createLoadingDialog(connectingMsg);

    // Ejecutar la secuencia de conexión e inicio en un hilo de fondo
    // Capturamos por valor los datos necesarios para evitar uso de `this` en background
    HostInfo hostCopy = this->host;
    RemoteAppInfo appCopy = app;
    STREAM_CONFIGURATION cfgCopy = streamConfig;
    std::thread([hostCopy, appCopy, cfgCopy, loadingDialog, this, prevGridVis]() mutable {
        brls::Logger::info("[SessionAppSelect][async] Iniciando conexión en hilo de fondo para {}", hostCopy.ip);
        bool connected = GameStreamClient::instance().connect(hostCopy);
        // Actualizar UI en hilo principal
    brls::sync([connected, hostCopy, appCopy, cfgCopy, loadingDialog, prevGridVis, this]() mutable {
            // Cerrar dialogo de conexión inicial
            if (loadingDialog) {
                loadingDialog->close();
            }
            if (!connected) {
                // Restaurar inputs y estado antes de notificar
                if (loadingDialog) loadingDialog->close();
                brls::Application::unblockInputs();
                if (this->gridView) {
                    this->gridView->setHideHighlight(false);
                    this->gridView->setHideHighlightBackground(false);
                    this->gridView->setHideHighlightBorder(false);
                    this->gridView->setFocusable(true);
                    // Restaurar foco al grid si antes estaba visible
                    if (prevGridVis == brls::Visibility::VISIBLE)
                        brls::Application::giveFocus(this->gridView);
                    this->gridView->setVisibility(prevGridVis);
                    // Restaurar descendientes (filas -> cards -> inner views)
                    std::function<void(brls::View*)> walkRestore;
                    walkRestore = [&walkRestore](brls::View* v) {
                        if (!v) return;
                        v->setHideHighlight(false);
                        v->setHideHighlightBackground(false);
                        v->setHideHighlightBorder(false);
                        v->setFocusable(true);
                        brls::Box* box = dynamic_cast<brls::Box*>(v);
                        if (!box) return;
                        auto ch = box->getChildren();
                        for (auto* cc : ch) walkRestore(cc);
                    };
                    auto rows2 = this->gridView->getChildren();
                    for (auto* row : rows2) walkRestore(row);
                }
                brls::Logger::error("[SessionAppSelect] Error al conectar con el servidor");
                brls::Application::notify(brls::getStr("moonlight/session/app_select/error_connect"));
                return;
            }

            // Si no está emparejado y no hay device.ini, iniciar pairing (se muestra su propio popup si procede)
            // Reusar el flujo existente: beginPairing maneja su propio diálogo/popup cuando se llama desde UI
            bool pairedByFile = false;
            {
                ConfigManager cfg; cfg.load();
                std::string base = cfg.getKeysDir();
                HostInfo h = hostCopy;
                if (h.safeId.empty()) h.safeId = makeSafeHostId(h.name.empty()? h.ip : h.name);
                std::vector<std::string> cands;
                cands.push_back(base + "/" + h.safeId);
                if (!h.name.empty()) {
                    auto pos = h.name.find('.');
                    if (pos != std::string::npos) cands.push_back(base + "/" + makeSafeHostId(h.name.substr(0,pos)));
                }
                if (!h.ip.empty()) {
                    cands.push_back(base + "/" + h.ip);
                    cands.push_back(base + "/" + makeSafeHostId(h.ip));
                }
                struct stat st{};
                for (auto& dir : cands) {
                    std::string devIni = dir + "/device.ini";
                    if (stat(devIni.c_str(), &st)==0) { pairedByFile = true; break; }
                }
            }

            if (!pairedByFile && !GameStreamClient::instance().isPaired(hostCopy.ip)) {
                // Mostrar el diálogo de pairing con spinner reutilizando la utilidad si se desea
                brls::Application::notify(brls::getStr("moonlight/session/app_select/connecting"));
                GameStreamClient::instance().beginPairing(hostCopy, [this, appCopy, prevGridVis](bool ok){
                    brls::sync([this, ok, appCopy, prevGridVis]() {
                        // Restaurar inputs independientemente del resultado
                        brls::Application::unblockInputs();
                        if (this->gridView) {
                            this->gridView->setHideHighlight(false);
                            this->gridView->setHideHighlightBackground(false);
                            this->gridView->setHideHighlightBorder(false);
                            this->gridView->setFocusable(true);
                            if (prevGridVis == brls::Visibility::VISIBLE)
                                brls::Application::giveFocus(this->gridView);
                            this->gridView->setVisibility(prevGridVis);
                            std::function<void(brls::View*)> walkRestore;
                            walkRestore = [&walkRestore](brls::View* v) {
                                if (!v) return;
                                v->setHideHighlight(false);
                                v->setHideHighlightBackground(false);
                                v->setHideHighlightBorder(false);
                                v->setFocusable(true);
                                brls::Box* box = dynamic_cast<brls::Box*>(v);
                                if (!box) return;
                                auto ch = box->getChildren();
                                for (auto* cc : ch) walkRestore(cc);
                            };
                            auto rows3 = this->gridView->getChildren();
                            for (auto* row : rows3) walkRestore(row);
                        }
                        if (!ok) {
                            brls::Application::notify(brls::getStr("moonlight/session/app_select/pairing_failed"));
                            return;
                        }
                        brls::Application::notify(brls::getStr("moonlight/session/app_select/paired"));
                        // Reintentar iniciar la app ahora que está emparejado
                        this->AppSelected(appCopy);
                    });
                });
                return;
            }

            // Mostrar diálogo de 'iniciando' mientras hacemos startApp + VitaSession
            std::string startingMsg = brls::getStr("moonlight/session/app_select/starting");
            auto* startDialog = createLoadingDialog(startingMsg);

            // Ejecutar startApp y VitaSession en un hilo para no bloquear UI
            std::thread([hostCopy, appCopy, cfgCopy, startDialog, this, prevGridVis]() mutable {
                bool started = GameStreamClient::instance().startApp(hostCopy.ip, cfgCopy, std::stoi(appCopy.id));
                brls::sync([started, hostCopy, appCopy, startDialog, this, prevGridVis]() {
                    if (startDialog) startDialog->close();
                    // Restaurar inputs antes de procesar resultado
                    brls::Application::unblockInputs();
                    if (this->gridView) {
                        this->gridView->setHideHighlight(false);
                        this->gridView->setHideHighlightBackground(false);
                        this->gridView->setHideHighlightBorder(false);
                            this->gridView->setFocusable(true);
                            if (prevGridVis == brls::Visibility::VISIBLE)
                                brls::Application::giveFocus(this->gridView);
                        this->gridView->setVisibility(prevGridVis);
                            auto children = this->gridView->getChildren();
                            for (auto* c : children) {
                                if (c) {
                                    c->setHideHighlight(false);
                                    c->setHideHighlightBackground(false);
                                    c->setHideHighlightBorder(false);
                                    c->setFocusable(true);
                                }
                            }
                    }
                    if (!started) {
                        brls::Logger::error("[SessionAppSelect] Error al iniciar aplicación");
                        // Si el servidor reporta una aplicación en ejecución, ofrecer
                        // al usuario la opción de terminarla remotamente y reintentar.
                        SERVER_DATA& sd = GameStreamClient::instance().serverData(hostCopy.ip);
                        if (sd.currentGame != 0) {
                            // Construir diálogo de confirmación: Quit & Retry
                            auto* holder2 = new brls::Box(brls::Axis::COLUMN);
                            auto* label2 = new brls::Label();
                            std::string msg2 = brls::getStr("moonlight/session/app_select/host_reports_running");
                            // Insertar nombre si lo tenemos
                            size_t pos2 = msg2.find("$(app)");
                            std::string appName = appCopy.name.empty() ? std::to_string(sd.currentGame) : appCopy.name;
                            if (pos2 != std::string::npos) msg2.replace(pos2, 6, appName);
                            label2->setText(msg2);
                            label2->setHorizontalAlign(brls::HorizontalAlign::CENTER);
                            label2->setMarginBottom(18);

                            auto* btnBox2 = new brls::Box(brls::Axis::ROW);
                            btnBox2->setJustifyContent(brls::JustifyContent::CENTER);
                            btnBox2->setAlignItems(brls::AlignItems::CENTER);

                            auto* quitBtn = new brls::Button();
                            quitBtn->setText(brls::getStr("moonlight/session/app_select/button_quit_remote"));
                            quitBtn->setStyle(&brls::BUTTONSTYLE_HIGHLIGHT);
                            quitBtn->setMargins(0, 8, 0, 0);

                            auto* cancelBtn = new brls::Button();
                            cancelBtn->setText(brls::getStr("moonlight/session/app_select/button_cancel"));
                            cancelBtn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

                            btnBox2->addView(quitBtn);
                            btnBox2->addView(cancelBtn);
                            holder2->addView(label2);
                            holder2->addView(btnBox2);
                            holder2->setPadding(18,18,18,18);

                            auto* dialog2 = new brls::Dialog(holder2);
                            dialog2->setCancelable(true);
                            dialog2->open();

                            quitBtn->registerClickAction([this, dialog2, hostCopy, appCopy](brls::View*) -> bool {
                                dialog2->dismiss();
                                auto* waiting = createLoadingDialog(brls::getStr("moonlight/session/app_select/ending_session"));
                                std::thread([this, hostCopy, appCopy, waiting]() mutable {
                                    bool ok = GameStreamClient::instance().quitApp(hostCopy.ip);
                                    brls::sync([this, ok, appCopy, waiting]() {
                                        if (waiting) waiting->close();
                                        if (!ok) {
                                            brls::Application::notify(brls::getStr("moonlight/session/app_select/error_end_session"));
                                            return;
                                        }
                                        // Reintentar iniciar la app ahora que hemos terminado la remota
                                        this->AppSelected(appCopy);
                                    });
                                }).detach();
                                return true;
                            });

                            cancelBtn->registerClickAction([dialog2](brls::View*) -> bool {
                                dialog2->dismiss();
                                brls::Application::notify(brls::getStr("moonlight/session/app_select/cancelled"));
                                return true;
                            });
                            return;
                        }

                        // Si no hay currentGame, o el usuario no desea terminar la remota,
                        // mostrar el error genérico.
                        brls::Application::notify(brls::getStr("moonlight/session/app_select/error_start_app"));
                        return;
                    }

                    // Crear y arrancar VitaSession
                    SERVER_DATA& serverData = GameStreamClient::instance().serverData(hostCopy.ip);
                    int appId = std::stoi(appCopy.id);
                    bool isSunshine = false;
                    if (serverData.serverInfo.serverInfoAppVersion && std::string(serverData.serverInfo.serverInfoAppVersion).find("Sunshine") != std::string::npos)
                        isSunshine = true;
                    else if (serverData.serverInfo.serverCodecModeSupport != 0)
                        isSunshine = true;

                    auto* vitaSession = new VitaSession(hostCopy.ip, appId, isSunshine);
                    if (!vitaSession->start()) {
                        brls::Logger::error("[SessionAppSelect] VitaSession start() falló");
                        brls::Application::notify(brls::getStr("moonlight/session/app_select/error_start_stream"));
                        delete vitaSession;
                        return;
                    }

                    GameStreamClient::instance().setActiveStream(hostCopy.ip, appId, appCopy.name);
                    auto* sessionView = new SessionMainView(hostCopy, appCopy);
                    brls::Application::pushActivity(new brls::Activity(sessionView), brls::TransitionAnimation::NONE);
                });
            }).detach();
        });
    }).detach();

    // Salimos inmediatamente: el resto del flujo continúa en hilos de fondo
    return;

    // Pairing gating: comprobar primero si existe device.ini en cualquier keyDir candidato (migración)
    bool pairedByFile = false;
    {
        ConfigManager cfg; cfg.load();
        std::string base = cfg.getKeysDir();
        HostInfo h = this->host;
        if (h.safeId.empty()) h.safeId = makeSafeHostId(h.name.empty()? h.ip : h.name);
        std::vector<std::string> cands;
        cands.push_back(base + "/" + h.safeId);
        if (!h.name.empty()) {
            auto pos = h.name.find('.');
            if (pos != std::string::npos) cands.push_back(base + "/" + makeSafeHostId(h.name.substr(0,pos)));
        }
        if (!h.ip.empty()) {
            cands.push_back(base + "/" + h.ip);
            cands.push_back(base + "/" + makeSafeHostId(h.ip));
        }
        struct stat st{};
        for (auto& dir : cands) {
            std::string devIni = dir + "/device.ini";
            if (stat(devIni.c_str(), &st)==0) { pairedByFile = true; brls::Logger::info("[SessionAppSelect][gating] device.ini detectado en '{}' -> marcar como paired", dir); break; }
        }
    }
    // Si bien puede existir un device.ini (pairedByFile), eso no garantiza que
    // el host siga reconociendo el emparejamiento (PairStatus en serverinfo).
    // Intentar conectar y consultar el estado real del servidor antes de decidir
    // si debemos iniciar el flujo de pairing.
    bool serverPaired = false;
    {
        HostInfo h = this->host;
        if (h.safeId.empty()) h.safeId = makeSafeHostId(h.name.empty()? h.ip : h.name);
        // Intentar conectar (gs_init) para obtener serverData actualizado si es necesario
        if (!GameStreamClient::instance().isConnected(h.ip)) {
            brls::Logger::info("[SessionAppSelect] No conectado a %s, intentando connect() para verificar estado de pairing", h.ip.c_str());
            GameStreamClient::instance().connect(h);
        }
        serverPaired = GameStreamClient::instance().isPaired(h.ip);
    }

    if (!serverPaired) {
        brls::Logger::info("[SessionAppSelect] Host no emparejado según el servidor - iniciando beginPairing desde gating");
        HostInfo h = this->host;
        if (h.safeId.empty()) h.safeId = makeSafeHostId(h.name.empty()? h.ip : h.name);
        GameStreamClient::instance().beginPairing(h, [this, app](bool ok){
            if (ok) {
                brls::Application::notify(brls::getStr("moonlight/session/app_select/paired"));
                // Reintentar lanzamiento
                this->AppSelected(app);
            } else {
                brls::Application::notify(brls::getStr("moonlight/session/app_select/pairing_failed"));
            }
        });
        return; // esperar callback
    }

    // Obtener referencia a los datos del servidor
    SERVER_DATA& serverData = GameStreamClient::instance().serverData(this->host.ip);

    brls::Logger::info("[SessionAppSelect] ServerInfo después de conectar:");
    const char* addr = serverData.serverInfo.address ? serverData.serverInfo.address : "NULL";
    const char* ver = serverData.serverInfo.serverInfoAppVersion ? serverData.serverInfo.serverInfoAppVersion : "NULL";
    const char* url = serverData.serverInfo.rtspSessionUrl ? serverData.serverInfo.rtspSessionUrl : "NULL";
    brls::Logger::info("[SessionAppSelect] - address: {}", addr);
    brls::Logger::info("[SessionAppSelect] - serverInfoAppVersion: {}", ver);
    brls::Logger::info("[SessionAppSelect] - rtspSessionUrl: {}", url);

    // Lanzar la aplicación en el servidor usando GameStreamClient
    int appId = std::stoi(app.id);
    if (!GameStreamClient::instance().startApp(this->host.ip, streamConfig, appId)) {
        brls::Logger::error("[SessionAppSelect] Error al iniciar aplicación");
    brls::Application::notify(brls::getStr("moonlight/session/app_select/error_start_app"));
        return;
    }

    // Verificar serverInfo después de startApp
    brls::Logger::info("[SessionAppSelect] ServerInfo después de startApp:");
    const char* addr2 = serverData.serverInfo.address ? serverData.serverInfo.address : "NULL";
    const char* ver2 = serverData.serverInfo.serverInfoAppVersion ? serverData.serverInfo.serverInfoAppVersion : "NULL";
    const char* url2 = serverData.serverInfo.rtspSessionUrl ? serverData.serverInfo.rtspSessionUrl : "NULL";
    brls::Logger::info("[SessionAppSelect] - address: {}", addr2);
    brls::Logger::info("[SessionAppSelect] - serverInfoAppVersion: {}", ver2);
    brls::Logger::info("[SessionAppSelect] - rtspSessionUrl: {}", url2);

    // Reemplazo de StreamingManager por VitaSession
    // Configurar e iniciar VitaSession (usa callbacks internos)
    bool isSunshine = false;
    if (serverData.serverInfo.serverInfoAppVersion && std::string(serverData.serverInfo.serverInfoAppVersion).find("Sunshine") != std::string::npos)
        isSunshine = true;
    else if (serverData.serverInfo.serverCodecModeSupport != 0)
        isSunshine = true; // fallback débil
    auto* vitaSession = new VitaSession(this->host.ip, appId, isSunshine);
    if (!vitaSession->start()) {
        brls::Logger::error("[SessionAppSelect] VitaSession start() falló");
    brls::Application::notify(brls::getStr("moonlight/session/app_select/error_start_stream"));
        delete vitaSession;
        return;
    }

    brls::Logger::info("[SessionAppSelect] VitaSession iniciada");

    // Registrar stream activo
    GameStreamClient::instance().setActiveStream(this->host.ip, appId, app.name);

    // Crear vista de sesión activa y transicionar
    auto* sessionView = new SessionMainView(this->host, app);
    brls::Application::pushActivity(new brls::Activity(sessionView), brls::TransitionAnimation::NONE);
}
