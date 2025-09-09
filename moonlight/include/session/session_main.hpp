#pragma once

#include <borealis.hpp>
#include "model/HostStorage.hpp"

// Vista principal de la sesión de streaming (usando XML)
class SessionMainView : public brls::Box {
public:
    SessionMainView(const HostInfo& host, const RemoteAppInfo& app);
private:
    HostInfo host;
    RemoteAppInfo app;
    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, appLabel, "appLabel");
    BRLS_BIND(brls::Label, info, "info");
    BRLS_BIND(brls::Button, endBtn, "endBtn");
};

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app);
