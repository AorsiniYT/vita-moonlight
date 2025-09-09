/*
    session_main.cpp - Pantalla principal de sesión activa (streaming) para Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <borealis.hpp>
#include "connection_manager.hpp"
#include "model/HostStorage.hpp"
#include "session/session_main.hpp"

// Implementación del constructor
SessionMainView::SessionMainView(const HostInfo& host, const RemoteAppInfo& app)
    : brls::Box(), host(host), app(app) {
    this->inflateFromXMLRes("xml/views/session_main.xml");

    if (title) title->setText("Conectado a: " + host.name + " (" + host.ip + ")");
    if (appLabel) appLabel->setText("App: " + app.name);
    if (info) info->setText("[Streaming en curso]");
    if (endBtn) {
        endBtn->registerClickAction([this](brls::View*) {
            // Aquí deberías llamar a la lógica de desconexión real
            brls::Application::notify("Sesión finalizada");
            brls::Application::popActivity();
            return true;
        });
    }
}

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app) {
    auto* view = new SessionMainView(host, app);
    brls::Application::pushActivity(new brls::Activity(view));
}
