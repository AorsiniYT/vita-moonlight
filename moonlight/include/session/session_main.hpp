#pragma once

#include "borealis.hpp"
#include <string>

// Forward declarations para evitar dependencias circulares pesadas
struct HostInfo; // definido en model/HostStorage.hpp
struct RemoteAppInfo; // definido en connection_manager.hpp

#include "model/HostStorage.hpp"
#include "connection_manager.hpp" // RemoteAppInfo

// Vista principal de la sesión de streaming (usando XML)
class SessionMainView : public brls::Box {
public:
    SessionMainView(const HostInfo& host, const RemoteAppInfo& app);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
private:
    void openSessionMenu();
    HostInfo host;
    RemoteAppInfo app;
    class VitaStreamOverlayView* overlayStatsView = nullptr; // overlay de estadísticas de video
    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, appLabel, "appLabel");
    BRLS_BIND(brls::Label, info, "info");
    BRLS_BIND(brls::Button, endBtn, "endBtn");
};

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app);
