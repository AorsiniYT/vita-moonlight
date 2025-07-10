/*
    session_app_select.cpp - Selección de aplicación para iniciar streaming en Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include "session/session_app_select.hpp"
#include "view/pccard.hpp"
#include "model/HostStorage.hpp"

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
    app_grid_container->addView(gridView);

    // Crear y configurar el Spinner y añadirlo al mismo contenedor que el grid
    spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    app_grid_container->addView(spinner);

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
    spinner->setVisibility(brls::Visibility::VISIBLE);
    gridView->setVisibility(brls::Visibility::INVISIBLE);
    app_select_empty->setVisibility(brls::Visibility::GONE);
    gridView->clearViews();

    if (host.name.empty() || host.ip.empty()) {
        spinner->setVisibility(brls::Visibility::GONE);
        app_select_empty->setText("Error: No se encontró la información del host.");
        app_select_empty->setVisibility(brls::Visibility::VISIBLE);
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
            spinner->setVisibility(brls::Visibility::GONE);
            if (apps.empty()) {
                brls::Logger::info("[SessionAppSelect] No se encontraron aplicaciones en este host.");
                app_select_empty->setText("No se encontraron aplicaciones en este host.");
                app_select_empty->setVisibility(brls::Visibility::VISIBLE);
                return;
            }
            gridView->setVisibility(brls::Visibility::VISIBLE);
            for (const auto& app : apps) {
                auto* card = new PCCard(app.name, "img/moonlight/pc.png");
                card->setClickAction([this, app]() { this->AppSelected(app); });
                gridView->addView(card);
            }
        });
    });
}

void SessionAppSelect::AppSelected(const RemoteAppInfo& app) {
    brls::Logger::info("App seleccionada: {} (ID: {})", app.name, app.id);
    // Aquí iría la lógica para iniciar el streaming
    // brls::Application::pushActivity(new StreamingView(this->host, app));
}
