/*
    session_main.cpp - Pantalla principal de sesión activa (streaming) para Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "borealis.hpp"
#include "connection_manager.hpp"
#include "model/HostStorage.hpp"
#include "GameStreamClient.hpp"
#include "session/session_main.hpp"
#include "session/hotkey_manager.hpp"
#include "session/overlay/ingame_overlay_view.hpp"
#include "ui/vita_stream_overlay_view.hpp"
#include "video/legacy/vita.h"
#include "video/legacy/modules/vita_globals.h"
#include "video/VitaVideoRenderer.hpp"

// Implementación del constructor
SessionMainView::SessionMainView(const HostInfo& host, const RemoteAppInfo& app)
    : brls::Box(), host(host), app(app) {
    this->inflateFromXMLRes("xml/views/session_main.xml");

    // Añadir overlay de estadísticas de video (se puede ocultar más tarde)
    overlayStatsView = new VitaStreamOverlayView();
    this->addView(overlayStatsView);

    // Ocultar elementos de UI base para dejar sólo el video (modo fullscreen)
    if (title) title->setVisibility(brls::Visibility::GONE);
    if (appLabel) appLabel->setVisibility(brls::Visibility::GONE);
    if (info) info->setVisibility(brls::Visibility::GONE);
    if (endBtn) endBtn->setVisibility(brls::Visibility::GONE);
    // Si en el futuro activamos presentación externa, podríamos:
    // vitavideo_enable_external_present(true);

    if (title) title->setText("Conectado a: " + host.name + " (" + host.ip + ")");
    if (appLabel) appLabel->setText("App: " + app.name);
    if (info) info->setText("[Streaming en curso]");
    if (endBtn) {
        endBtn->registerClickAction([this](brls::View*) {
            VitaSession::destroyActive(true);
            GameStreamClient::instance().clearActiveStream(this->host.ip);
            brls::Application::notify("Sesión finalizada");
            brls::Application::popActivity();
            return true;
        });
    }
    // Acción rápida: START abre el menú de sesión (simplificado)
    this->registerAction("menu", brls::ControllerButton::BUTTON_START, [this](brls::View*) {
        this->openSessionMenu();
        return true;
    }, false, false, brls::SOUND_NONE);
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
    dialog->addButton("Toggle Overlay Stats", [this, dialog]() {
        if (overlayStatsView) {
            overlayStatsView->setVisible(!overlayStatsView->isVisible());
            brls::Application::notify(overlayStatsView->isVisible() ? "Overlay ON" : "Overlay OFF");
        }
        dialog->close();
    });
    dialog->addButton("Toggle UI Base", [this, dialog]() {
        auto toggleVis = [](brls::View* v){ if (!v) return; v->setVisibility(v->getVisibility()==brls::Visibility::GONE? brls::Visibility::VISIBLE: brls::Visibility::GONE); };
        toggleVis(title);
        toggleVis(appLabel);
        toggleVis(info);
        toggleVis(endBtn);
        dialog->close();
    });

    dialog->addButton("Atrás", [dialog]() {
        dialog->close();
    });

    dialog->open();
    }

void SessionMainView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // Dibujar video a través del renderer unificado
    VitaVideoRenderer::instance().draw(vg, x, y, width, height, 1.0f);
    // Fallback vita2d deshabilitado para evitar mezcla de contextos GXM que estaba provocando crash.
    // Si se necesita un fallback visual, implementar un blit CPU->NVG en lugar de llamar vita2d aquí.
    // Luego dibujar los elementos de UI de Borealis
    Box::draw(vg, x, y, width, height, style, ctx);
}

// Función para lanzar la pantalla principal de sesión
void showSessionMain(const HostInfo& host, const RemoteAppInfo& app) {
    auto* view = new SessionMainView(host, app);
    brls::Application::pushActivity(new brls::Activity(view));
}
