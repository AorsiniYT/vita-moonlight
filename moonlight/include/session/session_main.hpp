#pragma once

#include <borealis.hpp>
#include <memory>
#include <string>
#include "model/HostStorage.hpp"      // HostInfo
#include "connection_manager.hpp"     // RemoteAppInfo
#include "session/overlay/vita_stream_overlay_view.hpp"

// Vista principal de la sesión de streaming (usando XML)
class SessionMainView : public brls::Box {
public:
    SessionMainView(const HostInfo& host, const RemoteAppInfo& app);
    ~SessionMainView();
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
private:
    void openSessionMenu();
    bool isPauseOverlayOpen() const { return pauseOverlayOpen; }
    HostInfo host;
    RemoteAppInfo app;
    std::unique_ptr<VitaStreamOverlayView> overlayStatsView;
    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, appLabel, "appLabel");
    BRLS_BIND(brls::Label, info, "info");
    BRLS_BIND(brls::Button, endBtn, "endBtn");
    static inline bool pauseOverlayOpen = false;
};

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app);
