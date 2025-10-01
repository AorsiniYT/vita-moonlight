
/*
    Copyright 2021 natinusala
    Edit for AorsiniYT 2025

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "tab/settings_tab.hpp"
#include "ConfigManager.hpp"
#include "tab/rear_touch_settings_tab.hpp"
#include <cstdlib>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "settings.hpp"
#include "video/render_mode_cache.hpp"
#include "network/NetworkOptimizations.h"

using namespace brls::literals;  // for _i18n

bool radioSelected = false;

static std::vector<std::string> NOTIFICATIONS = {
    "You have cool hair",
    "I like your shoes",
    "borealis is powered by nanovg",
    "The Triforce is an inside job",
    "Pozznx will trigger in one day and twelve hours",
    "Aurora Borealis? At this time of day, at this time of year, in this part of the gaming market, located entirely within your Switch?!",
    "May I see it?",
    "Hmm, Steamed Hams!",
    "Hello\nWorld!"
};

SettingsTab::SettingsTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");

    // Cargar configuración actual
    ConfigManager config;
    config.load();
    StreamConfiguration streamConfig = config.getStreamConfig();
    VideoSettings videoSettings = config.getVideoSettings();
    if (videoSettings.render_mode != 0 && videoSettings.pixel_format_mode != 0) {
        videoSettings.pixel_format_mode = 0;
        config.setVideoSettings(videoSettings);
        config.save();
    }

    // Inicializar flag global para debug logs
    extern bool g_debug_log_enabled;
    g_debug_log_enabled = videoSettings.save_debug_log;

    // Inicializar optimizaciones de red
    vita_netopt_set_enabled(videoSettings.enable_network_optimizations ? 1 : 0);

    // Selector de modo de render (Direct GXM eliminado): 0=Legacy, 1=FFmpeg (futuro)
    std::vector<std::string> renderModes = {
        "Legacy (SceVideodec)",
        "Modern (FFmpeg)"
    };
    auto updatePixelSelectorVisibility = [this](int renderMode, bool persistReset) {
        bool legacyMode = (renderMode == 0);
        if (pixelFormatSelector) {
            pixelFormatSelector->setVisibility(legacyMode ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
            if (!legacyMode) {
                pixelFormatSelector->setSelection(0, true);
                if (persistReset) {
                    ConfigManager cfg;
                    cfg.load();
                    VideoSettings vs = cfg.getVideoSettings();
                    if (vs.pixel_format_mode != 0) {
                        vs.pixel_format_mode = 0;
                        cfg.setVideoSettings(vs);
                        cfg.save();
                    }
                }
            }
        }
    };

    int initialRenderMode = videoSettings.render_mode;
    if (initialRenderMode < 0 || initialRenderMode >= (int)renderModes.size()) initialRenderMode = 0; // clamp si config tiene valor desconocido
    renderModeSelector->init("Modo de Renderizado", renderModes, initialRenderMode, [this, updatePixelSelectorVisibility](int selected) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.render_mode = selected; // 0=legacy,1=ffmpeg
        if (selected != 0 && settings.pixel_format_mode != 0) {
            settings.pixel_format_mode = 0;
        }
        config.setVideoSettings(settings);
        config.save();
        // Actualizar cache atómico sin relectura posterior
        set_render_mode_cached(selected);
        updatePixelSelectorVisibility(selected, true);
        std::string modeName;
        switch (selected) {
            case 0: modeName = "Legacy"; break;
            case 1: modeName = "Modern"; break;
            default: modeName = "Desconocido"; break;
        }
        brls::Application::notify("Modo de renderizado: " + modeName);
    });

    // Selector de formato de pixel (para pruebas RGBA vs YUV)
    std::vector<std::string> pixelFormats = {"RGBA directo", "YUV420 (experimental)"};
    pixelFormatSelector->init("Formato de Pixel", pixelFormats, videoSettings.pixel_format_mode, [this](int selected) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.pixel_format_mode = selected;
        config.setVideoSettings(settings);
        config.save();
        std::string pf = (selected == 0) ? "RGBA" : "YUV420";
        brls::Application::notify("Formato de pixel: " + pf);
    });
    updatePixelSelectorVisibility(initialRenderMode, false);

    // Configurar selectores de resolución con valores permitidos para PS Vita
    std::vector<std::string> resolutions = {
        "960x540 (16:9 QHD)",
        "960x544 (Vita Native)",
        "1024x576 (16:9)",
        "1152x648 (16:9)",
        "1280x540 (21:9)",
        "1280x720 (720p HD)",
        "1366x768 (WXGA)",
        "1600x900 (900p HD+)",
        "1920x1080 (FHD)"
    };
    
    // Resoluciones permitidas para PS Vita (deben ser múltiplos de 16)
    std::vector<std::pair<int, int>> vitaResolutions = {
        {960, 544},   // Vita native (ya ajustada a múltiplo de 16)
        {960, 544},   // Vita native
        {1024, 576},  // 16:9
        {1152, 656},  // Ajustada a múltiplo de 16 (1152x648 -> 1152x656)
        {1280, 544},  // Ajustada a múltiplo de 16 (1280x540 -> 1280x544)
        {1280, 720},  // 16:9, 720p HD
        {1360, 768},  // Ajustada a múltiplo de 16 (1366x768 -> 1360x768)
        {1600, 896},  // Ajustada a múltiplo de 16 (1600x900 -> 1600x896)
        {1920, 1088}  // Ajustada a múltiplo de 16 (1920x1080 -> 1920x1088)
    };
    
    int currentRes = 5; // Default 1280x720 (índice 5)
    if (streamConfig.width == 960 && streamConfig.height == 544) currentRes = 0;
    else if (streamConfig.width == 1024 && streamConfig.height == 576) currentRes = 2;
    else if (streamConfig.width == 1152 && streamConfig.height == 656) currentRes = 3;
    else if (streamConfig.width == 1280 && streamConfig.height == 544) currentRes = 4;
    else if (streamConfig.width == 1360 && streamConfig.height == 768) currentRes = 6;
    else if (streamConfig.width == 1600 && streamConfig.height == 896) currentRes = 7;
    else if (streamConfig.width == 1920 && streamConfig.height == 1088) currentRes = 8;
    
    resolutionSelector->init("Resolución", resolutions, currentRes, [this, vitaResolutions](int selected) {
        ConfigManager config;
        config.load();
        StreamConfiguration streamConfig = config.getStreamConfig();
        
        if (selected >= 0 && selected < (int)vitaResolutions.size()) {
            streamConfig.width = vitaResolutions[selected].first;
            streamConfig.height = vitaResolutions[selected].second;
            // Aplicar validación de PS Vita
            streamConfig.validateAndAdjustResolution();
        }
        
        config.setStreamConfig(streamConfig);
        config.save();
        brls::Application::notify("Resolución guardada");
    });

    // Configurar selector de FPS con valores del legacy
    std::vector<std::string> fpsOptions = {
        "24 FPS (Cine)",
        "30 FPS (Estándar)",
        "40 FPS",
        "50 FPS (PAL)",
        "60 FPS (NTSC)"
    };
    int currentFps = 4; // Default 60 FPS (índice 4)
    if (streamConfig.fps == 24) currentFps = 0;
    else if (streamConfig.fps == 30) currentFps = 1;
    else if (streamConfig.fps == 40) currentFps = 2;
    else if (streamConfig.fps == 50) currentFps = 3;
    
    fpsSelector->init("FPS", fpsOptions, currentFps, [this](int selected) {
        ConfigManager config;
        config.load();
        StreamConfiguration streamConfig = config.getStreamConfig();
        
        switch (selected) {
            case 0: streamConfig.fps = 24; break; // Cine
            case 1: streamConfig.fps = 30; break; // Estándar
            case 2: streamConfig.fps = 40; break;
            case 3: streamConfig.fps = 50; break; // PAL
            case 4: streamConfig.fps = 60; break; // NTSC
        }
        config.setStreamConfig(streamConfig);
        config.save();
        brls::Application::notify("FPS guardado");
    });

    // Configurar selector de bitrate con valores apropiados para PS Vita
    std::vector<std::string> bitrateOptions = {
        "Auto (Recomendado)",
        "2000 Kbps (Bajo)",
        "5000 Kbps (Estándar)",
        "8000 Kbps (HD)",
        "10000 Kbps (HD+)",
        "15000 Kbps (Full HD)",
        "20000 Kbps (Full HD+)",
        "30000 Kbps (Ultra HD)",
        "50000 Kbps (Máximo)"
    };
    
    int currentBitrate = 0; // Auto por defecto
    if (streamConfig.bitrate == 2000) currentBitrate = 1;
    else if (streamConfig.bitrate == 5000) currentBitrate = 2;
    else if (streamConfig.bitrate == 8000) currentBitrate = 3;
    else if (streamConfig.bitrate == 10000) currentBitrate = 4;
    else if (streamConfig.bitrate == 15000) currentBitrate = 5;
    else if (streamConfig.bitrate == 20000) currentBitrate = 6;
    else if (streamConfig.bitrate == 30000) currentBitrate = 7;
    else if (streamConfig.bitrate == 50000) currentBitrate = 8;
    
    bitrateSelector->init("Bitrate (Kbps)", bitrateOptions, currentBitrate, [this](int selected) {
        ConfigManager config;
        config.load();
        StreamConfiguration streamConfig = config.getStreamConfig();
        
        switch (selected) {
            case 0: streamConfig.bitrate = -1; break; // Auto
            case 1: streamConfig.bitrate = 2000; break;
            case 2: streamConfig.bitrate = 5000; break;
            case 3: streamConfig.bitrate = 8000; break;
            case 4: streamConfig.bitrate = 10000; break;
            case 5: streamConfig.bitrate = 15000; break;
            case 6: streamConfig.bitrate = 20000; break;
            case 7: streamConfig.bitrate = 30000; break;
            case 8: streamConfig.bitrate = 50000; break;
        }
        config.setStreamConfig(streamConfig);
        config.save();
        brls::Application::notify("Bitrate guardado");
    });

    // Configurar toggles booleanos
    sopsToggle->init("Optimización de Stream", videoSettings.sops, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.sops = value;
        config.setVideoSettings(settings);
        config.save();
    });

    // Toggle para optimizaciones de red (IDR smart, pacing, etc.)
    networkOptimizationsToggle->init("Optimizaciones de Red", videoSettings.enable_network_optimizations, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_network_optimizations = value;
        config.setVideoSettings(settings);
        config.save();
        // Aplicar inmediatamente
        vita_netopt_set_enabled(value ? 1 : 0);
        brls::Application::notify(value ? "Optimizaciones de red activadas" : "Optimizaciones de red desactivadas");
    });

    localAudioToggle->init("Audio Local", videoSettings.localaudio, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.localaudio = value;
        config.setVideoSettings(settings);
        config.save();
    });

    fullscreenToggle->init("Pantalla Completa", videoSettings.fullscreen, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.fullscreen = value;
        config.setVideoSettings(settings);
        config.save();
    });

    // Low Latency eliminado: toggle suprimido

    framePacerToggle->init("Frame Pacer", videoSettings.enable_frame_pacer, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_frame_pacer = value;
        config.setVideoSettings(settings);
        config.save();
    });

    centerRegionToggle->init("Solo Región Central", videoSettings.center_region_only, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.center_region_only = value;
        config.setVideoSettings(settings);
        config.save();
    });

    showFpsToggle->init("Mostrar FPS", videoSettings.show_fps, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.show_fps = value;
        config.setVideoSettings(settings);
        config.save();
    });

    debugLogToggle->init("Guardar Log de Debug", videoSettings.save_debug_log, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.save_debug_log = value;
        config.setVideoSettings(settings);
        config.save();
        // Actualizar flag global para debug logs
        extern bool g_debug_log_enabled;
        g_debug_log_enabled = value;
    });

    refFrameInvalidationToggle->init("Invalidación de Frame de Referencia", videoSettings.enable_ref_frame_invalidation, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_ref_frame_invalidation = value;
        config.setVideoSettings(settings);
        config.save();
    });

    vblankWaitToggle->init("Esperar VBlank Vita", videoSettings.enable_vita_vblank_wait, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_vita_vblank_wait = value;
        config.setVideoSettings(settings);
        config.save();
    });

    motionControlsToggle->init("Controles de Movimiento", videoSettings.enable_motion_controls, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_motion_controls = value;
        config.setVideoSettings(settings);
        config.save();
    });

    doubleTapSprintToggle->init("Doble Tap para Correr", videoSettings.enable_double_tap_sprint, [this](bool value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.enable_double_tap_sprint = value;
        config.setVideoSettings(settings);
        config.save();
    });

    if (rearTouchSettingsEntry)
    {
        rearTouchSettingsEntry->setDetailText(brls::getStr("moonlight/settings/rear_touch_detail"));
        rearTouchSettingsEntry->registerClickAction([](brls::View*) {
            auto* rearTouchView = new RearTouchSettingsTab();
            brls::Application::pushActivity(new brls::Activity(rearTouchView));
            return true;
        });
    }

    // Configurar selector de modo touchscreen
    std::vector<std::string> touchscreenModes = {"Desactivado", "DS4 Touchpad", "Mouse Absoluto", "Tableta Multitouch"};
    touchscreenModeSelector->init("Modo Touchscreen", touchscreenModes, videoSettings.touchscreen_mode, [this](int selected) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.touchscreen_mode = selected;
        config.setVideoSettings(settings);
        config.save();
        brls::Application::notify("Modo touchscreen guardado");
    });

    // Configurar slider de aceleración del mouse
    mouseAccelerationSlider->init("Aceleración del Mouse", videoSettings.mouse_acceleration, [this](float value) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.mouse_acceleration = (int)value;
        config.setVideoSettings(settings);
        config.save();
        mouseAccelerationSlider->setDetailText(std::to_string((int)value));
    });
    mouseAccelerationSlider->setDetailText(std::to_string(videoSettings.mouse_acceleration));

    // Configurar selector de layout de teclado
    std::vector<std::string> keyboardLayouts = {"EN-US", "ES-ES", "ES-LATAM"};
    keyboardLayoutSelector->init("Layout del Teclado", keyboardLayouts, videoSettings.keyboard_layout, [this](int selected) {
        ConfigManager config;
        config.load();
        VideoSettings settings = config.getVideoSettings();
        settings.keyboard_layout = selected;
        config.setVideoSettings(settings);
        config.save();
        brls::Application::notify("Layout de teclado guardado");
    });

    // Configurar opciones del sistema (originales)
    radio->title->setText("Radio cell");
    radio->setSelected(radioSelected);
    radio->registerClickAction([this](brls::View* view) {
        radioSelected = !radioSelected;
        this->radio->setSelected(radioSelected);
        return true;
    });

    boolean->title->setText("Switcher");

    debug->init("Debug Layer", brls::Application::isDebuggingViewEnabled(), [](bool value){
        brls::Application::enableDebuggingView(value);
        brls::sync([value](){
            brls::Logger::info("{} the debug layer", value ? "Open" : "Close");
        });
    });

    bottomBar->init("Bottom Bar", !brls::AppletFrame::HIDE_BOTTOM_BAR, [](bool value){
        brls::AppletFrame::HIDE_BOTTOM_BAR = !value;
        auto stack = brls::Application::getActivitiesStack();
        for (auto& activity : stack) {
            auto* frame = dynamic_cast<brls::AppletFrame*>(
                activity->getContentView());
            if (!frame) continue;
            frame->setFooterVisibility(!value ? brls::Visibility::GONE
                                             : brls::Visibility::VISIBLE);
        }
    });

    fps->init("FPS", brls::Application::getFPSStatus(), [](bool value){
        brls::Application::setFPSStatus(value);
    });

    swapInterval->init("Swap Interval", {"0", "1", "2", "3", "4"}, 1, [](int selected) {},
        [](int selected) { brls::Application::setSwapInterval(selected); });

    alwaysOnTop->init("Always On Top", false, [](bool value){
        brls::Application::getPlatform()->setWindowAlwaysOnTop(value);
    });

    selector->init("Selector", { "Test 1", "Test 2", "Test 3", "Test 4", "Test 5", "Test 6", "Test 7", "Test 8", "Test 9", "Test 10", "Test 11", "Test 12", "Test 13" }, 0, [](int selected) {
    }, [](int selected) {
        auto dialog = new brls::Dialog(fmt::format("selected {}", selected));
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
    });

    input->init(
        "Input text", "https://github.com", [](std::string text) {

        },
        "Placeholder", "Hint");

    inputNumeric->init(
        "Input number", 2448, [](int number) {

        },
        "Hint");

    ipAddress->setDetailText(brls::Application::getPlatform()->getIpAddress());
    dnsServer->setDetailText(brls::Application::getPlatform()->getDnsServer());

    input->registerAction("hints/open"_i18n, brls::BUTTON_X, [](brls::View* view) {
        brls::DetailCell *cell = dynamic_cast<brls::DetailCell *>(view);
        brls::Application::getPlatform()->openBrowser(cell->detail->getFullText());
        return true;
    }, false, false, brls::SOUND_CLICK);

    float brightness = brls::Application::getPlatform()->getBacklightBrightness();
    slider->init("Brightness", brightness, [this](float value){
        brls::Application::getPlatform()->setBacklightBrightness(value);
        slider->setDetailText(fmt::format("{:.2f}", value));
    });
    slider->setDetailText(fmt::format("{:.2f}", brightness));

    notify->registerClickAction([](...){
        std::string notification = NOTIFICATIONS[std::rand() % NOTIFICATIONS.size()];
        brls::Application::notify(notification);
        return true;
    });

    // Idiomas disponibles
    std::vector<std::string> languages = {"Español", "English"};
    int currentLang = 1; // 0: es, 1: en-US
    std::string currentLocale = brls::Application::getLocale();
    if (currentLocale == "es" || currentLocale == "es-ES") currentLang = 0;
    languageSelector->init(brls::getStr("moonlight/settings/language"), languages, currentLang, [this](int selected) {
        std::string locale = (selected == 0) ? "es" : "en-US";
#ifdef _WIN32
        _putenv_s("LANG", locale.c_str());
        _putenv_s("BOREALIS_LANG", locale.c_str());
#else
        setenv("LANG", locale.c_str(), 1);
        setenv("BOREALIS_LANG", locale.c_str(), 1);
#endif
        
        // Recargar traducciones
        brls::loadTranslations();
        
        // Crear directorio si no existe
        std::string configPath = ConfigManager::getConfigPath();
        size_t pos = configPath.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string configDir = configPath.substr(0, pos);
#ifdef _WIN32
            CreateDirectoryA(configDir.c_str(), NULL);
#else
            mkdir(configDir.c_str(), 0755);
#endif
        }
        
        // Guardar el idioma seleccionado directamente
        ConfigManager config;
        config.set("general", "language", locale);
        config.save();
        
        brls::Application::notify(locale == "es" ? "Idioma cambiado correctamente" : "Language changed successfully");
    });
    SettingsTab::languageSelectorPtr = languageSelector;
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}
