/*
    session_main.cpp - Pantalla principal de sesión activa (streaming) para Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdlib> // getenv
#include "borealis.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/style.hpp>
#include <borealis/core/frame_context.hpp>
#include <borealis/views/dialog.hpp>
#include "connection_manager.hpp"
#include "model/HostStorage.hpp"
#include "GameStreamClient.hpp"
#include "session/session_main.hpp"
#include "session/hotkey_manager.hpp"
#include "session/overlay/ingame_overlay_view.hpp"
#include "session/overlay/vita_pause_overlay.hpp"
#include "video/legacy/vita.hpp"
#include "video/legacy/modules/vita_globals.hpp"
#include "video/VitaVideoRenderer.hpp"
#include "session/vita_session.hpp"
#include "ConfigManager.hpp"
#include <vita2d.h>
#include <borealis/extern/nanovg/nanovg.h>
#include "controller/ControllerInput.hpp"
#include "debug.hpp"

#include "session/hotkey_manager.hpp"

// Implementación del constructor
SessionMainView::SessionMainView(const HostInfo& host, const RemoteAppInfo& app)
    : brls::Box(), host(host), app(app) {
    this->setFocusable(true);
    this->setHideHighlight(true);
    this->setBackgroundColor(nvgRGBA(0,0,0,255));

    this->inflateFromXMLRes("xml/views/session_main.xml");

    // Asegurar que vita2d esté listo ANTES de cualquier potencial draw (defensivo)
    extern bool vita2d_inited; if (!vita2d_inited) { vita2d_init(); vita2d_inited = true; vita2d_set_vblank_wait(0); }

    // Inicializar input manager
    g_controllerInput = new ControllerInputManager();

    // Registrar callback de pausa en ControllerInputManager (START+L+R)
    // Evitar abrir múltiples overlays si se mantiene la combinación pulsada.
    g_controllerInput->setPauseCallback([this]() {
        if (SessionMainView::pauseOverlayOpen) return;
        SessionMainView::pauseOverlayOpen = true;
        VitaPauseOverlay* overlay = new VitaPauseOverlay([]() {
            // restablecer el flag cuando el overlay se cierre
            SessionMainView::pauseOverlayOpen = false;
        }, this->host);
        brls::Application::pushActivity(new brls::Activity(overlay));
    });

    // Resetear input para evitar estados residuales de la UI anterior
    if (g_controllerInput) g_controllerInput->dropInput();

    // Ocultar UI base para dejar solo video y overlay
    if (title) title->setVisibility(brls::Visibility::GONE);
    if (appLabel) appLabel->setVisibility(brls::Visibility::GONE);
    if (info) info->setVisibility(brls::Visibility::GONE);
    if (endBtn) endBtn->setVisibility(brls::Visibility::GONE);

    // Registrar callback de pausa en HotkeyManager (START+L+R)
    // Reutilizar el mismo comportamiento (abrir overlay lateral) y respetar el flag
    HotkeyManager::instance().setPauseCallback([this]() {
        if (SessionMainView::pauseOverlayOpen) return;
        SessionMainView::pauseOverlayOpen = true;
        VitaPauseOverlay* overlay = new VitaPauseOverlay([]() {
            SessionMainView::pauseOverlayOpen = false;
        }, this->host);
        brls::Application::pushActivity(new brls::Activity(overlay));
    });

    // No hay acción para START solo

    // Intentar forzar loop de render a 60fps si el host negocia >30.
    // Recuperar configuración de streaming y video.
    ConfigManager cfgMgr; cfgMgr.load();
    StreamConfiguration streamCfg = cfgMgr.getStreamConfig();
    VideoSettings videoSettings = cfgMgr.getVideoSettings();

    // Set touchscreen mode
    g_controllerInput->setTouchscreenMode(videoSettings.touchscreen_mode);

    if (!overlayStatsView) {
        overlayStatsView = std::make_unique<VitaStreamOverlayView>();
    }
    if (overlayStatsView) {
        overlayStatsView->setVisible(videoSettings.show_fps);
        this->addView(overlayStatsView.get());
    }
    g_video_settings_snapshot.show_fps = videoSettings.show_fps;

    unsigned targetFps = (unsigned)streamCfg.fps;
    if (targetFps == 0) targetFps = 60;
    if (targetFps > 60) targetFps = 60;
    brls::Application::setLimitedFPS(targetFps); // limitar FPS según configuración
    brls::Application::setSwapInterval(1); // habilitar vsync para estabilidad
    brls::Logger::info("[SessionMainView] Init render config (cfg_fps={} -> limitedFPS={} swapInterval=1)", targetFps, targetFps);

    // Modo Direct GXM eliminado. render_mode se normaliza: 0=legacy,1=ffmpeg (futuro)
    bool settingsChanged = false;
    if (videoSettings.render_mode > 1) {
        videoSettings.render_mode = 0;
        brls::Logger::info("[SessionMainView] render_mode deprecated (>1) normalizado a 0 (legacy)");
        settingsChanged = true;
    }
    if (settingsChanged) {
        cfgMgr.setVideoSettings(videoSettings);
        cfgMgr.save();
    }

    brls::Application::giveFocus(this);
}

void SessionMainView::openSessionMenu() {
    vita_debug_log("[SessionMainView] openSessionMenu llamado - abriendo diálogo de pausa");
    auto dialog = new brls::Dialog("Menú de pausa");

    dialog->addButton("Reanudar", [dialog]() {
        dialog->close();
    });

    dialog->addButton("Desconectar", [this, dialog]() {
        VitaSession::destroyActive(true);
        GameStreamClient::instance().clearActiveStream(this->host.ip);
        dialog->close();
        brls::Application::popActivity();
    });

    dialog->addButton("Cerrar app", [this, dialog]() {
        // Cerrar la app en el host (igual que desconectar por ahora)
        VitaSession::destroyActive(true);
        GameStreamClient::instance().clearActiveStream(this->host.ip);
        dialog->close();
        brls::Application::popActivity();
    });

    dialog->open();
}

void SessionMainView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // Procesar input cada frame
    if (g_controllerInput) g_controllerInput->handleInput();

    // Nueva ruta: usar NanoVG si está disponible para pintar el frame sin tocar el ciclo vita2d
    if (vg) {
        VitaVideoRenderer::instance().drawNVG(vg, width, height, 1.0f);
    } else {
        // Fallback (sin vg): ruta directa vita2d (no debería suceder normalmente en Borealis)
        VitaVideoRenderer::instance().draw(width, height);
    }
    Box::draw(vg, x, y, width, height, style, ctx);
}

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app) {
    auto* view = new SessionMainView(host, app);
    brls::Application::pushActivity(new brls::Activity(view));
}

SessionMainView::~SessionMainView() {
    if (g_controllerInput) {
        delete g_controllerInput;
        g_controllerInput = nullptr;
    }
}
