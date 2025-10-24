/*
    session_app_select.cpp - Selección de aplicación para iniciar streaming en Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include "session/session_app_select.hpp"
#include <sys/stat.h>
#include "view/pccard.hpp"
#include "utils/dialog_utils.h"
#include "model/HostStorage.hpp"
#include "connection_manager.hpp"
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

    // Si ya existe un stream activo para este host, ir directo a la vista de sesión
    if (!this->host.ip.empty() && GameStreamClient::instance().hasActiveStream(this->host.ip)) {
        brls::Logger::info("[SessionAppSelect] Stream activo detectado para {} -> saltando a SessionMainView", this->host.ip);
        RemoteAppInfo running = GameStreamClient::instance().activeAppInfo(this->host.ip);
        if (running.name.empty()) {
            running.name = brls::getStr("moonlight/session/app_select/running_session");
            running.id = "0";
        }
        auto* sessionView = new SessionMainView(this->host, running);
        brls::Application::pushActivity(new brls::Activity(sessionView), brls::TransitionAnimation::NONE);
        // Mostrar menú inmediatamente
        brls::async([](){ brls::Logger::info("[SessionAppSelect] (async) Sesión retomada"); });
        return;
    }

    // Iniciar la carga de la lista de apps si no hay sesión activa
    this->populateAppList();
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
    if (!pairedByFile && !GameStreamClient::instance().isPaired(this->host.ip)) {
        brls::Logger::info("[SessionAppSelect] Host no emparejado - iniciando beginPairing desde gating");
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
