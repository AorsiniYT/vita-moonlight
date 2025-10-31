#include "session/overlay/vita_pause_overlay.hpp"
#include <borealis.hpp>
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

VitaPauseOverlay::VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo)
    : BaseOverlay(), onClose(std::move(onClose)), host(hostInfo) {

    // Configurar header
    setHeaderText(brls::getStr("moonlight/session/pause/title"));

    // Configurar botones
    std::vector<std::string> labels = {
        brls::getStr("moonlight/session/pause/resume"),
        brls::getStr("moonlight/tabs/settings"),
        brls::getStr("moonlight/session/pause/disconnect"),
        brls::getStr("moonlight/session/pause/close_app")
    };
    setButtons(labels);

    // Configurar callback de activación
    setActivateCallback([this](int index) {
        switch (index) {
            case 0: // Resume
                this->resume();
                break;
            case 1: // Settings
                {
                    brls::View* settingsView = SettingsTab::create();
                    auto* frame = new brls::AppletFrame(settingsView);
                    frame->setTitle(brls::getStr("moonlight/tabs/settings"));
                    auto* activity = new brls::Activity(frame);
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
    });

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
    if (onClose) {
        auto cb = std::move(onClose);
        onClose = nullptr;
        auto tcb_start = high_resolution_clock::now();
        cb();
        auto tcb_end = high_resolution_clock::now();
        auto cb_us = duration_cast<microseconds>(tcb_end - tcb_start).count();
        auto total_us = duration_cast<microseconds>(tcb_end - tstart).count();
        vita_debug_log("[VitaPauseOverlay][PERF] resume cb=%lld us total=%lld us", (long long)cb_us, (long long)total_us);
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
    // Ejecutar la destrucción de la sesión en background
    std::string addr = this->host.ip;
    auto storedOnClose = std::move(onClose);
    onClose = nullptr;
    brls::Application::notify(brls::getStr("moonlight/session/pause/notify_disconnected"));
    std::thread([addr, storedOnClose]() mutable {
        try {
            brls::sync([]() { VitaVideoRenderer::instance().destroyImage(brls::Application::getNVGContext()); });
        } catch (...) {}
        try {
            VitaSession* s = VitaSession::active();
            if (s) s->stop(false);
        } catch (...) {}
        GameStreamClient::instance().clearActiveStream(addr);
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        brls::sync([storedOnClose]() mutable {
            if (storedOnClose) {
                try { storedOnClose(); } catch(...) {}
            }
            vita_debug_log("[VitaPauseOverlay] After onClose, g_controllerInput: %p", g_controllerInput);
            try {
                int fps2 = (int)std::lround(brls::Application::getFPS());
                VitaVideoStats vstats2{}; vitavideo_get_stats(&vstats2);
                vita_debug_log("[VitaPauseOverlay][INST] afterOnClose FPS=%d video_presented=%u decoded=%u target=%u", fps2, vstats2.frames_presented, vstats2.frames_decoded, vstats2.target_fps);
            } catch(...) {}
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
        try {
            GameStreamClient::instance().quitApp(addr);
        } catch (...) {}
        try {
            brls::sync([]() { VitaVideoRenderer::instance().destroyImage(brls::Application::getNVGContext()); });
        } catch (...) {}
        try {
            VitaSession* s = VitaSession::active();
            if (s) s->stop(false);
        } catch (...) {}
        GameStreamClient::instance().clearActiveStream(addr);
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        brls::sync([storedOnClose]() mutable {
            if (storedOnClose) {
                try { storedOnClose(); } catch(...) {}
            }
            HostsTab::requestGlobalRefresh();
        });
    }).detach();
}

VitaPauseOverlay::~VitaPauseOverlay() {
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
