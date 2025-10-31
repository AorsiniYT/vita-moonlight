#include "session/overlay/vita_pause_overlay.hpp"
#include <borealis.hpp>
// #include <borealis/views/button.hpp>
// #include <borealis/views/label.hpp>
#include "video/VitaVideoRenderer.hpp"
#include "session/vita_session.hpp"
#include "GameStreamClient.hpp"
#include "debug.hpp"
#include "video/legacy/vita.hpp"
#include "tab/settings_tab.hpp"
#include "tab/hosts_tab.hpp"
#include "controller/ControllerInput.hpp"
#include "activity/main_activity.hpp"
#include <thread>
#include <chrono>

// Clase dummy para foco sin indicador visual
class FocusDummy : public brls::View {
public:
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override {
        // No dibujar nada
    }
    void drawFocus(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
        // No dibujar foco
    }
};

VitaPauseOverlay::VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo)
    : onClose(std::move(onClose)), host(hostInfo) {
        // Inicializar labels de botones
    buttonLabels = {
        brls::getStr("moonlight/session/pause/resume"),
        brls::getStr("moonlight/tabs/settings"),
        brls::getStr("moonlight/session/pause/disconnect"),
        brls::getStr("moonlight/session/pause/close_app")
    };

    // Configurar botones y foco
    configureButtons();

    // Hacer el overlay focusable
    this->setFocusable(true);

    // Crear un dummy para foco sin indicador visual (opcional, pero lo dejamos)
    focusDummy = new FocusDummy();
    focusDummy->setFocusable(true);
    focusDummy->setWidth(1);
    focusDummy->setHeight(1);
    focusDummy->setHideHighlight(true); // Ocultar completamente el indicador de foco
    this->addView(focusDummy);

    // Registrar acciones en el dummy de foco para que la vista que recibe el foco
    // maneje directamente la navegación. Esto evita problemas si el foco real
    // está en un view pequeño o externo.
    if (focusDummy) {
        focusDummy->registerAction("", brls::BUTTON_NAV_UP, [this](brls::View*) {
            this->moveFocus(-1);
            return true;
        });
        focusDummy->registerAction("", brls::BUTTON_NAV_DOWN, [this](brls::View*) {
            this->moveFocus(1);
            return true;
        });
        focusDummy->registerAction("", brls::BUTTON_A, [this](brls::View*) {
            this->activateFocused();
            return true;
        });
        focusDummy->registerAction(brls::getStr("global/back"), brls::BUTTON_B, [this](brls::View*) {
            this->resume();
            return true;
        });
        focusDummy->registerAction("Cerrar", brls::BUTTON_START, [this](brls::View*) {
            this->resume();
            return true;
        });
    }

    brls::sync([this]() { brls::Application::giveFocus(focusDummy); });

    vita_debug_log("[VitaPauseOverlay] opened for host=%s", host.ip.c_str());
    // Instrumentación: registrar FPS y estado de video al abrir el overlay
    try {
        int fps_i = (int)std::lround(brls::Application::getFPS());
        bool fpsStatus = brls::Application::getFPSStatus();
        VitaVideoStats vstats{}; vitavideo_get_stats(&vstats);
        vita_debug_log("[VitaPauseOverlay][INST] onOpen FPS=%d fpsStatus=%d video_last_frame=%u presented=%u decoded=%u target=%u",
                       fps_i, fpsStatus ? 1 : 0, vstats.last_frame_number, vstats.frames_presented, vstats.frames_decoded, vstats.target_fps);
    } catch(...) {}
}



void VitaPauseOverlay::configureButtons() {
    // Ahora los actions se registran en el dummy
}

void VitaPauseOverlay::onLayout() {
    // No children to layout, as we're using pure NVG drawing
}

void VitaPauseOverlay::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // Dibujar todo con NanoVG para eficiencia máxima, sin widgets Borealis
    using namespace std::chrono;
    auto t_start = high_resolution_clock::now();

    // Dimensiones del panel (lado derecho, un poco más a la derecha)
    float panelW = 480.0f;
    float panelH = 544.0f;
    float panelX = 800.0f; // Un poco más a la derecha: 960 - 480 - 270 = 210px margen derecho
    float panelY = 0.0f;

    // Dibujar panel background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, panelX, panelY, panelW, panelH, 8.0f);
    nvgFillColor(vg, nvgRGBA(18, 20, 24, 255));
    nvgFill(vg);

    // Dibujar header
    float headerX = panelX + 28.0f;
    float headerY = panelY + 50.0f; // Igual que test
    nvgFontSize(vg, 28.0f);
    nvgFontFaceId(vg, 0); // Usar font ID como en test
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
    nvgText(vg, headerX, headerY, brls::getStr("moonlight/session/pause/title").c_str(), nullptr);

    // Dibujar botones (igual que test)
    float btnW = 424.0f;
    float btnH = 56.0f;
    float btnX = panelX + 28.0f;
    float btnY = panelY + 80.0f;
    float btnMargin = 12.0f;
    nvgFontSize(vg, 22.0f);
    for (size_t i = 0; i < buttonLabels.size(); ++i) {
        // Fondo del botón
        nvgBeginPath(vg);
        nvgRoundedRect(vg, btnX, btnY, btnW, btnH, 10.0f);
        NVGcolor bgColor = (i == focusedIndex) ? nvgRGBA(100, 180, 255, 240) : nvgRGBA(64, 64, 64, 220);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);

        // Borde tenue para todos
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 120));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);

        // Texto
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        nvgText(vg, btnX + 16, btnY + 36, buttonLabels[i].c_str(), nullptr);

        btnY += btnH + btnMargin;
    }

    auto t_end = high_resolution_clock::now();
    auto dur_us = duration_cast<microseconds>(t_end - t_start).count();
    uint64_t now_ms = duration_cast<milliseconds>(t_start.time_since_epoch()).count();
    if (now_ms - this->lastDrawLogMs > 500) {
        this->lastDrawLogMs = now_ms;
        vita_debug_log("[VitaPauseOverlay][PERF] draw time=%lld us", (long long)dur_us);
    }

    // Dibujar hijos (como el focusDummy)
    Box::draw(vg, x, y, width, height, style, ctx);
}

void VitaPauseOverlay::drawFocus(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    // No dibujar el foco por defecto de Borealis, ya que manejamos el foco manualmente en draw()
}

void VitaPauseOverlay::willAppear(bool resetState) {
    Box::willAppear(resetState);
    // Resetear el índice de foco
    focusedIndex = 0;
    // Dar foco al dummy para que reciba inputs
    brls::Application::giveFocus(focusDummy);
    vita_debug_log("[VitaPauseOverlay] willAppear called");
}

void VitaPauseOverlay::moveFocus(int delta) {
    vita_debug_log("[VitaPauseOverlay] moveFocus called with delta=%d", delta);
    int numButtons = buttonLabels.size();
    focusedIndex = (focusedIndex + delta + numButtons) % numButtons;
    vita_debug_log("[VitaPauseOverlay] focusedIndex now=%d", focusedIndex);
}

void VitaPauseOverlay::activateFocused() {
    vita_debug_log("[VitaPauseOverlay] activateFocused called, focusedIndex=%d", focusedIndex);
    switch (focusedIndex) {
        case 0: // Resume
            this->resume();
            break;
        case 1: // Settings
            {
                // Crear la vista de settings y envolverla en un AppletFrame para conservar
                // el comportamiento estándar de BACK/CIRCLE (como en rearTouchSettingsEntry).
                brls::View* settingsView = SettingsTab::create();
                auto* frame = new brls::AppletFrame(settingsView);
                frame->setTitle(brls::getStr("moonlight/tabs/settings"));
                auto* activity = new brls::Activity(frame);
                // Usar FADE para evitar la animación de slide en raw views
                brls::Application::pushActivity(activity, brls::TransitionAnimation::FADE);
            }
            break;
        case 2: // Disconnect
            this->disconnect();
            break;
        case 3: // Close App
            this->closeApp();
            break;
    }
}

void VitaPauseOverlay::resume() {
    vita_debug_log("[VitaPauseOverlay] resume pressed");
    // Log FPS/state when resuming UI
    try {
        int fps_i = (int)std::lround(brls::Application::getFPS());
        VitaVideoStats vstats{}; vitavideo_get_stats(&vstats);
        vita_debug_log("[VitaPauseOverlay][INST] resume FPS=%d video_presented=%u decoded=%u target=%u", fps_i, vstats.frames_presented, vstats.frames_decoded, vstats.target_fps);
    } catch(...) {}
    // Cerrar con animación y notificar al llamador
    using namespace std::chrono;
    auto tstart = high_resolution_clock::now();
    // Use NONE transition to avoid the fade animation cost causing a frame
    // hitch on resume; this reduces risk of a long pop delaying UI scheduling.
    // brls::Application::popActivity(brls::TransitionAnimation::NONE); // removed
    auto tpop = high_resolution_clock::now();
    if (onClose) {
        auto cb = std::move(onClose);
        onClose = nullptr;
        auto tcb_start = high_resolution_clock::now();
        cb();
        auto tcb_end = high_resolution_clock::now();
        auto pop_us = duration_cast<microseconds>(tpop - tstart).count();
        auto cb_us = duration_cast<microseconds>(tcb_end - tcb_start).count();
        auto total_us = duration_cast<microseconds>(tcb_end - tstart).count();
        vita_debug_log("[VitaPauseOverlay][PERF] resume pop=%lld us cb=%lld us total=%lld us", (long long)pop_us, (long long)cb_us, (long long)total_us);
    } else {
        auto tnow = high_resolution_clock::now();
        auto pop_us = duration_cast<microseconds>(tnow - tstart).count();
        vita_debug_log("[VitaPauseOverlay][PERF] resume pop=%lld us (no callback)", (long long)pop_us);
    }
}

void VitaPauseOverlay::disconnect() {
    vita_debug_log("[VitaPauseOverlay] disconnect pressed");
    // Instrumentación: registrar FPS/estado antes de iniciar la secuencia de stop
    try {
        int fps_i = (int)std::lround(brls::Application::getFPS());
        VitaVideoStats vstats{}; vitavideo_get_stats(&vstats);
        vita_debug_log("[VitaPauseOverlay][INST] disconnect start FPS=%d video_last_frame=%u presented=%u decoded=%u target=%u",
                       fps_i, vstats.last_frame_number, vstats.frames_presented, vstats.frames_decoded, vstats.target_fps);
    } catch(...) {}
    // Ejecutar la destrucción de la sesión en background y sólo cuando se
    // haya liberado el renderer/popular buffers, volver a la UI principal.
    // Esto evita que el popActivity ocurra mientras el driver aún procesa
    // recursos GXM y provoque SCE_GXM_ERROR_DRIVER.
    std::string addr = this->host.ip;
    auto storedOnClose = std::move(onClose);
    onClose = nullptr;
    // Notificar inmediatamente al usuario
    brls::Application::notify(brls::getStr("moonlight/session/pause/notify_disconnected"));
    std::thread([addr, storedOnClose]() mutable {
        // Before stopping the session, ensure UI-level NVG images that
        // reference the video texture are released. This removes NVG image
        // references to the underlying vita2d/GXM textures so we can avoid
        // leaving stale handles that would block the UI when we perform
        // the video soft-clean.
        try {
            brls::sync([]() { VitaVideoRenderer::instance().destroyImage(brls::Application::getNVGContext()); });
        } catch (...) {}

        // Parar la sesión localmente sin forzar el cierre de la app remota.
        // Llamar a stop(false) invoca LiStopConnection para detener el streaming
        // y limpiar buffers, pero NO mandará quitApp al host.
        try {
            VitaSession* s = VitaSession::active();
            if (s) {
                s->stop(false);
            }
        } catch (...) {
            // Ignorar fallos al intentar parar la sesión localmente
        }
        // Limpiar estado de stream activo en el cliente (marca local)
        GameStreamClient::instance().clearActiveStream(addr);
        // esperar un breve margen para asegurar que LiStopConnection / video threads
        // hayan terminado y no intenten usar recursos gráficos liberados.
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        // Volver al hilo UI para cerrar overlay y mostrar Hosts limpia (sin forzar pop de la session view)
        brls::sync([storedOnClose]() mutable {
            // Pop solo el overlay. No hacemos pop de la session view porque
            // en algunas condiciones el driver GXM aún puede estar liberando
            // recursos y esto provocaba SCE_GXM_ERROR_DRIVER al hacer pop
            // inmediatamente tras la liberación. En su lugar, solicitamos una
            // recarga segura del MainActivity (Hosts) que empuja una nueva
            // actividad encima y evita el crasheo.
            // brls::Application::popActivity(brls::TransitionAnimation::FADE); // overlay // removed
            // Restaurar estado desde el callback del overlay (reactivar inputs, etc)
            if (storedOnClose) {
                try { storedOnClose(); } catch(...) {}
            }
            vita_debug_log("[VitaPauseOverlay] After onClose, g_controllerInput: %p", g_controllerInput);
            // Log FPS/state right after restoring inputs and before leaving session
            try {
                int fps2 = (int)std::lround(brls::Application::getFPS());
                VitaVideoStats vstats2{}; vitavideo_get_stats(&vstats2);
                vita_debug_log("[VitaPauseOverlay][INST] afterOnClose FPS=%d video_presented=%u decoded=%u target=%u", fps2, vstats2.frames_presented, vstats2.frames_decoded, vstats2.target_fps);
            } catch(...) {}
            // Cerrar la actividad de la sesión para regresar completamente al main
            // brls::Application::popActivity(brls::TransitionAnimation::NONE); // session // removed
            // Pedir recarga global segura del Home/Hosts (push de nueva MainActivity)
            HostsTab::requestGlobalRefresh();
        });
    }).detach();
}

void VitaPauseOverlay::closeApp() {
    vita_debug_log("[VitaPauseOverlay] close app pressed");
    std::string addr = this->host.ip;
    auto storedOnClose = std::move(onClose);
    onClose = nullptr;
    brls::Application::notify(brls::getStr("moonlight/session/pause/notify_app_closed"));
    std::thread([addr, storedOnClose]() mutable {
        // Intentar pedir al host que cierre la app, pero no destruir localmente
        // la sesión hasta que LiStopConnection haya limpiado recursos.
        try {
            GameStreamClient::instance().quitApp(addr);
        } catch (...) {
            // Ignorar errores de quitApp
        }

        // Release any NVG image referencing the video texture before stopping
        // the session. This prevents the UI from holding references to video
        // GXM textures during the soft-clean path.
        try {
            brls::sync([]() { VitaVideoRenderer::instance().destroyImage(brls::Application::getNVGContext()); });
        } catch (...) {}

        // Parar la sesión localmente sin forzar doble limpieza por destructor
        try {
            VitaSession* s = VitaSession::active();
            if (s) s->stop(false);
        } catch (...) {}
        GameStreamClient::instance().clearActiveStream(addr);
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        brls::sync([storedOnClose]() mutable {
            // Pop solo el overlay y pedir refresh de Hosts
            // brls::Application::popActivity(brls::TransitionAnimation::FADE); // overlay // removed
            if (storedOnClose) {
                try { storedOnClose(); } catch(...) {}
            }
            HostsTab::requestGlobalRefresh();
        });
    }).detach();
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
