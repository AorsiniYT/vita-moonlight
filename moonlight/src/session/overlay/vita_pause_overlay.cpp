#include "session/overlay/vita_pause_overlay.hpp"
#include <borealis.hpp>
#include <borealis/views/button.hpp>
#include <borealis/views/label.hpp>
#include "session/vita_session.hpp"
#include "GameStreamClient.hpp"
#include "debug.hpp"
#include "tab/settings_tab.hpp"

VitaPauseOverlay::VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo)
    : onClose(std::move(onClose)), host(hostInfo) {
    // Construir layout: fila con un spacer a la izquierda y un panel fijo a la derecha
    // Hacer el root transparente para no pintar un fondo gris sobre toda la pantalla.
    auto* root = new brls::Box(brls::Axis::ROW);
    root->setGrow(1.0f);
    root->setBackgroundColor(nvgRGBA(0,0,0,0));
    this->setBackgroundColor(nvgRGBA(0,0,0,0));

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    spacer->setBackgroundColor(nvgRGBA(0,0,0,0));
    root->addView(spacer);

    auto* panel = new brls::Box(brls::Axis::COLUMN);
    // ancho aproximado: la mitad de la pantalla (544x960 en Vita -> usar 480px)
    panel->setSize(brls::Size(480, 544));
    panel->setPadding(28);

    headerLabel = new brls::Label();
    headerLabel->setText(brls::getStr("moonlight/session/pause/title"));
    headerLabel->setFontSize(28.0f);
    headerLabel->setMarginBottom(16);
    panel->addView(headerLabel);

    // Botones: los organizamos en una columna con separaciones para evitar que queden pegados
    auto* buttonsBox = new brls::Box(brls::Axis::COLUMN);
    buttonsBox->setMarginTop(20);
    buttonsBox->setAlignItems(brls::AlignItems::STRETCH);

    auto makeBtn = [&](const std::string& key, std::function<bool(brls::View*)> cb) {
        auto* b = new brls::Button();
        b->setText(brls::getStr(key));
        b->setHeight(56);
        b->setMarginBottom(12);
        b->setCornerRadius(10);
        b->registerClickAction(cb);
        return b;
    };

    auto* resumeBtn = makeBtn("moonlight/session/pause/resume", [this](brls::View*) {
        this->resume();
        return true;
    });
    buttonsBox->addView(resumeBtn);

    // Settings button: abrir la vista de Settings como Activity completa
    // Use the global tabs label for Settings so translations are consistent
    auto* settingsBtn = makeBtn("moonlight/tabs/settings", [this](brls::View*) {
        // Crear la vista de settings y envolverla en un AppletFrame para conservar
        // el comportamiento estándar de BACK/CIRCLE (como en rearTouchSettingsEntry).
        brls::View* settingsView = SettingsTab::create();
        auto* frame = new brls::AppletFrame(settingsView);
        frame->setTitle(brls::getStr("moonlight/tabs/settings"));
        auto* activity = new brls::Activity(frame);
        // Usar FADE para evitar la animación de slide en raw views
        brls::Application::pushActivity(activity, brls::TransitionAnimation::FADE);
        return true;
    });
    buttonsBox->addView(settingsBtn);

    auto* disconnectBtn = makeBtn("moonlight/session/pause/disconnect", [this](brls::View*) {
        this->disconnect();
        return true;
    });
    buttonsBox->addView(disconnectBtn);

    auto* closeBtn = makeBtn("moonlight/session/pause/close_app", [this](brls::View*) {
        this->closeApp();
        return true;
    });
    buttonsBox->addView(closeBtn);

    panel->addView(buttonsBox);

    root->addView(panel);

    // Añadir el root al propio widget (sin header/footer) para que el activity
    // muestre solo la caja derecha.
    this->addView(root);

    // Registrar acciones de botones físicos (CÍRCULO para cerrar)
    configureButtons();

    // Enfoque al primer botón
    brls::sync([this]() { brls::Application::giveFocus(this); });

    vita_debug_log("[VitaPauseOverlay] opened for host=%s", host.ip.c_str());
}



void VitaPauseOverlay::configureButtons() {
    // Registrar CÍRCULO (BUTTON_B) como atajo para "resume/close overlay"
    this->registerAction(brls::getStr("global/back"), brls::BUTTON_B, [this](brls::View*) {
        this->resume();
        return true;
    });
    // También permitir START para cerrar el overlay (comodidad)
    this->registerAction("Cerrar", brls::BUTTON_START, [this](brls::View*) {
        this->resume();
        return true;
    });
}

void VitaPauseOverlay::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // Dibujar solo el panel semitransparente en la mitad derecha (similar a VitaStreamOverlayView)
    if (vg) {
        float panelWidth = width * 0.5f; // ocupar la mitad derecha
        float panelX = width - panelWidth;
        float panelY = 0.0f;
        float panelH = height;

        nvgSave(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, panelX + 10.0f, panelY + 10.0f, panelWidth - 20.0f, panelH - 20.0f, 8.0f);
        nvgFillColor(vg, nvgRGBA(18, 20, 24, 230));
        nvgFill(vg);
        nvgRestore(vg);
    }

    // Luego delegar al Box para dibujar hijos (el panel con botones)
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

// isTranslucent() is implemented inline in the header to force translucency.

void VitaPauseOverlay::resume() {
    vita_debug_log("[VitaPauseOverlay] resume pressed");
    // Cerrar con animación y notificar al llamador
    brls::Application::popActivity(brls::TransitionAnimation::FADE);
    if (onClose) {
        auto cb = std::move(onClose);
        onClose = nullptr;
        cb();
    }
}

void VitaPauseOverlay::disconnect() {
    vita_debug_log("[VitaPauseOverlay] disconnect pressed");
    VitaSession::destroyActive(true);
    GameStreamClient::instance().clearActiveStream(this->host.ip);
    brls::Application::notify(brls::getStr("moonlight/session/pause/notify_disconnected"));
    // cerrar overlay y la vista de sesión
    brls::Application::popActivity(brls::TransitionAnimation::FADE); // overlay
    brls::Application::popActivity(brls::TransitionAnimation::NONE); // session view
    if (onClose) {
        auto cb = std::move(onClose);
        onClose = nullptr;
        cb();
    }
}

void VitaPauseOverlay::closeApp() {
    vita_debug_log("[VitaPauseOverlay] close app pressed");
    VitaSession::destroyActive(true);
    GameStreamClient::instance().clearActiveStream(this->host.ip);
    brls::Application::notify(brls::getStr("moonlight/session/pause/notify_app_closed"));
    // cerrar overlay y la vista de sesión
    brls::Application::popActivity(brls::TransitionAnimation::FADE); // overlay
    brls::Application::popActivity(brls::TransitionAnimation::NONE); // session view
    if (onClose) {
        auto cb = std::move(onClose);
        onClose = nullptr;
        cb();
    }
}

VitaPauseOverlay::~VitaPauseOverlay() {
    // Si por alguna razón el overlay fue destruido sin llamar a resume()/disconnect()/closeApp(),
    // asegurar que onClose se invoque exactamente una vez para restablecer el flag en el llamador.
    if (onClose) {
        try {
            auto cb = std::move(onClose);
            onClose = nullptr;
            cb();
        } catch (...) {
            vita_debug_log("[VitaPauseOverlay] exception calling onClose in dtor");
        }
    }
}
