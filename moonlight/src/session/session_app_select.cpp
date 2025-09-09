/*
    session_app_select.cpp - Selección de aplicación para iniciar streaming en Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include "session/session_app_select.hpp"
#include "view/pccard.hpp"
#include "model/HostStorage.hpp"
#include "connection_manager.hpp"
#include "session/streaming_manager.hpp"
#include "session/session_main.hpp"
#include "ConfigManager.hpp"
#include "client.h"
#include "Limelight.h"

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
    app_select_subtitle->setText("Selecciona una aplicación para iniciar");

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

    // Iniciar la carga de la lista de apps
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
            app_select_empty->setText("Error: No se encontró la información del host.");
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
                    app_select_empty->setText("No se encontraron aplicaciones en este host.");
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
    
    // Configuración básica para PS Vita
    streamConfig.width = 960;      // Resolución nativa PS Vita
    streamConfig.height = 544;
    streamConfig.fps = 30;
    streamConfig.bitrate = 8000;   // 8 Mbps
    streamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    streamConfig.encryptionFlags = ENCFLG_NONE;  // Sin encriptación por ahora

    // Inicializar servidor
    SERVER_DATA serverData;
    ConfigManager config;
    std::string baseDir = config.getKeysDir();
    std::string safeHostName = this->host.name;
    for (char& c : safeHostName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    std::string keyDir = baseDir + "/" + safeHostName;

    int initStatus = gs_init(&serverData, this->host.ip, keyDir);
    if (initStatus != 0) {
        brls::Logger::error("[SessionAppSelect] Error al inicializar servidor: {}", initStatus);
        brls::Application::notify("Error al conectar con el servidor");
        return;
    }

    // Lanzar la aplicación en el servidor
    int appId = std::stoi(app.id);
    int startStatus = gs_start_app(&serverData, &streamConfig, appId, true, true, 0x1);
    if (startStatus != 0) {
        brls::Logger::error("[SessionAppSelect] Error al iniciar app: {}", startStatus);
        brls::Application::notify("Error al iniciar la aplicación");
        return;
    }

    // Iniciar streaming
    StreamingManager* streamingManager = new StreamingManager();
    if (streamingManager->start(serverData, streamConfig)) {
        brls::Logger::info("[SessionAppSelect] Streaming iniciado correctamente");
        
        // Crear vista de sesión activa y transicionar como en Moonlight Switch
        auto* sessionView = new SessionMainView(this->host, app);
        auto* frame = new brls::AppletFrame(sessionView);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        
        brls::Application::pushActivity(new brls::Activity(frame));
    } else {
        brls::Logger::error("[SessionAppSelect] Error al iniciar streaming");
        brls::Application::notify("Error al iniciar streaming");
        delete streamingManager;
    }
}
