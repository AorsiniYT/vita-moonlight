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

// Vista principal de la sesión de streaming (usando XML)
class SessionMainView : public brls::Box {
public:
    SessionMainView(const HostInfo& host, const RemoteAppInfo& app)
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
private:
    HostInfo host;
    RemoteAppInfo app;
    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, appLabel, "appLabel");
    BRLS_BIND(brls::Label, info, "info");
    BRLS_BIND(brls::Button, endBtn, "endBtn");
};

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app) {
    auto* view = new SessionMainView(host, app);
    brls::Application::pushActivity(new brls::Activity(view));
}
