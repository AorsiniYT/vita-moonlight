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
#include "video/legacy/vita.h"
#include "video/legacy/modules/vita_globals.h"
#include "video/VitaVideoRenderer.hpp"
#include "session/vita_session.hpp"
#include "ConfigManager.hpp"
#include <vita2d.h>
#include <borealis/extern/nanovg/nanovg.h>

// Implementación del constructor
SessionMainView::SessionMainView(const HostInfo& host, const RemoteAppInfo& app)
    : brls::Box(), host(host), app(app) {
    this->inflateFromXMLRes("xml/views/session_main.xml");

    // Asegurar que vita2d esté listo ANTES de cualquier potencial draw (defensivo)
    extern bool vita2d_inited; if (!vita2d_inited) { vita2d_init(); vita2d_inited = true; vita2d_set_vblank_wait(0); }

    // Fase1/Fase2: sin BorealisVideoView -> draw centralizado en VitaSession / VitaVideoRenderer
    // Crear sesión básica si no existe (stub fase1)
    if (!VitaSession::active()) {
        // app.id podría estar en RemoteAppInfo; si no, usar 0 temporalmente
        int appId = 0;
        bool isSunshine = true; // Asumimos Sunshine por ahora (ajustar según host datos)
        auto vs = new VitaSession(host.ip, appId, isSunshine);
        vs->start();
    }
    // TODO: Reintroducir VitaStreamOverlayView cuando estabilicemos compilación

    // UI base deshabilitada temporalmente mientras estabilizamos el pipeline de video.
    // (Si se requiere mostrar títulos/botón fin, descomentar bloque siguiente)
    // if (title) title->setVisibility(brls::Visibility::GONE);
    // if (appLabel) appLabel->setVisibility(brls::Visibility::GONE);
    // if (info) info->setVisibility(brls::Visibility::GONE);
    // if (endBtn) endBtn->setVisibility(brls::Visibility::GONE);
    // Acción rápida: START abre el menú de sesión (simplificado)
    this->registerAction("menu", brls::ControllerButton::BUTTON_START, [this](brls::View*) {
        this->openSessionMenu();
        return true;
    }, false, false);

    // Intentar forzar loop de render a 60fps si el host negocia >30.
    // Recuperar configuración de streaming y video.
    ConfigManager cfgMgr; cfgMgr.load();
    StreamConfiguration streamCfg = cfgMgr.getStreamConfig();
    VideoSettings videoSettings = cfgMgr.getVideoSettings();

    if (!overlayStatsView) {
        overlayStatsView = std::make_unique<VitaStreamOverlayView>();
    }
    if (overlayStatsView) {
        overlayStatsView->setVisible(videoSettings.show_fps);
    }
    g_video_settings_snapshot.show_fps = videoSettings.show_fps;

    unsigned targetFps = (unsigned)streamCfg.fps;
    if (targetFps == 0) targetFps = 60;
    if (targetFps > 60) targetFps = 60;
    brls::Application::setLimitedFPS(0); // eliminar limit interno
    brls::Application::setSwapInterval(0); // intentar desbloquear vsync Borealis
    brls::Logger::info("[SessionMainView] Init render config (cfg_fps={} -> limitedFPS=off swapInterval=0)", targetFps);

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
}

void SessionMainView::openSessionMenu() {
    auto dialog = new brls::Dialog("Menú de sesión");

    dialog->addButton("Reanudar", [dialog]() {
        dialog->close();
    });

    dialog->addButton("Reconectar", [this, dialog]() {
        brls::Logger::info("[SessionMainView] Intentando reconectar (VitaSession) a {}", this->host.ip);
        auto vs = VitaSession::active();
        if (vs) {
            if (!vs->attemptReconnect()) brls::Application::notify("Sin intentos disponibles");
            else brls::Application::notify("Intento de reconexión lanzado");
        } else {
            // Fallback: intentar reconectar base
            if (!GameStreamClient::instance().connect(this->host.ip))
                brls::Application::notify("Falló reconexión base");
            else
                brls::Application::notify("Reconectado base");
        }
        dialog->close();
    });

    dialog->addButton("Desconectar", [this, dialog]() {
        VitaSession::destroyActive(true);
        GameStreamClient::instance().clearActiveStream(this->host.ip);
        dialog->close();
        brls::Application::popActivity();
    });

    dialog->addButton("Opciones", [dialog]() {
        dialog->close();
        auto overlay = new IngameOverlayView(VitaSession::active());
        brls::Application::pushActivity(new brls::Activity(overlay));
    });

    dialog->addButton("Alternar overlay FPS", [this, dialog]() {
        bool newVisibility = true;
        if (overlayStatsView) {
            newVisibility = !overlayStatsView->isVisible();
            overlayStatsView->setVisible(newVisibility);
        }
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.show_fps = newVisibility;
        config.setVideoSettings(settings);
        config.save();
        g_video_settings_snapshot.show_fps = newVisibility;
        brls::Application::notify(newVisibility ? "Overlay FPS activado" : "Overlay FPS oculto");
        dialog->close();
    });

    // Botón temporalmente deshabilitado porque los bound views están desactivados arriba.
    // dialog->addButton("Toggle UI Base", [this, dialog]() {
    //     auto toggleVis = [](brls::View* v){ if (!v) return; v->setVisibility(v->getVisibility()==brls::Visibility::GONE? brls::Visibility::VISIBLE: brls::Visibility::GONE); };
    //     toggleVis(title);
    //     toggleVis(appLabel);
    //     toggleVis(info);
    //     toggleVis(endBtn);
    //     dialog->close();
    // });

    dialog->addButton("Atrás", [dialog]() {
        dialog->close();
    });

    dialog->open();
}

void SessionMainView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // Nueva ruta: usar NanoVG si está disponible para pintar el frame sin tocar el ciclo vita2d
    if (vg) {
        VitaVideoRenderer::instance().drawNVG(vg, width, height, 1.0f);
    } else {
        // Fallback (sin vg): ruta directa vita2d (no debería suceder normalmente en Borealis)
        VitaVideoRenderer::instance().draw(width, height);
    }
    Box::draw(vg, x, y, width, height, style, ctx);
    if (overlayStatsView && vg && overlayStatsView->isVisible()) {
        overlayStatsView->draw(vg, x, y, width, height, style, ctx);
    }
}

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app) {
    auto* view = new SessionMainView(host, app);
    brls::Application::pushActivity(new brls::Activity(view));
}
