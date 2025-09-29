/*
    Copyright 2025 AorsiniYT

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
#include "ConfigManager.hpp"
#include <ini.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// Definición fuera de clase para static std::string ConfigManager::getConfigPath();
#ifdef _WIN32
    #include <windows.h>
#endif



std::string ConfigManager::getConfigPath() {
#ifdef _WIN32
    // En la misma carpeta que el exe
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) path = path.substr(0, pos+1);
    return path + "moonlight-vita.conf";
#elif defined(__PSV__) || defined(__psp2__) || defined(__PSP2__)
    return "ux0:data/moonlight/moonlight-vita.conf";
#else
    return "moonlight-vita.conf";
#endif
}

static int iniHandler(void* user, const char* section, const char* name, const char* value) {
    auto* config = reinterpret_cast<ConfigManager*>(user);
    config->set(section, name, value);
    return 1;
}



ConfigManager::ConfigManager() {}


// Obtiene el directorio de llaves/dispositivos, configurable por archivo o por código
std::string ConfigManager::getKeysDir() const {
    if (keysDirLoaded) return cachedKeysDir;
    // 1. Intentar leer de la config
    std::string dir = get("general", "keys_dir", "");
    if (!dir.empty()) {
        cachedKeysDir = dir;
        keysDirLoaded = true;
        return cachedKeysDir;
    }
    // 2. Si no está en config, usar valor multiplataforma por defecto
#if defined(__PSV__) || defined(__psp2__) || defined(__PSP2__)
    cachedKeysDir = "ux0:data/moonlight/devices";
#elif defined(_WIN32)
    cachedKeysDir = "moonlight/devices";
#else
    cachedKeysDir = "moonlight/devices";
#endif
    keysDirLoaded = true;
    return cachedKeysDir;
}

void ConfigManager::setKeysDir(const std::string& dir) {
    set("general", "keys_dir", dir);
    cachedKeysDir = dir;
    keysDirLoaded = true;
}

bool ConfigManager::load() {
    data.clear();
    std::string path = getConfigPath();
    std::cout << "[ConfigManager] Cargando configuración desde: " << path << std::endl;
    int result = ini_parse(path.c_str(), iniHandler, this);
    if (result != 0) {
        std::cout << "[ConfigManager] Error al cargar el archivo o no existe." << std::endl;
        return false;
    }
    std::cout << "[ConfigManager] Configuración cargada:" << std::endl;
    for (const auto& sec : data) {
        std::cout << "[" << sec.first << "]" << std::endl;
        for (const auto& kv : sec.second) {
            std::cout << kv.first << "=" << kv.second << std::endl;
        }
    }
    return true;
}


bool ConfigManager::save() const {
    std::string path = getConfigPath();
    std::cout << "[ConfigManager] Guardando configuración en: " << path << std::endl;
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cout << "[ConfigManager] No se pudo abrir el archivo para guardar." << std::endl;
        return false;
    }
    for (const auto& sec : data) {
        file << "[" << sec.first << "]\n";
        for (const auto& kv : sec.second) {
            file << kv.first << "=" << kv.second << "\n";
        }
        file << "\n";
    }
    std::cout << "[ConfigManager] Configuración guardada:" << std::endl;
    for (const auto& sec : data) {
        std::cout << "[" << sec.first << "]" << std::endl;
        for (const auto& kv : sec.second) {
            std::cout << kv.first << "=" << kv.second << std::endl;
        }
    }
    return true;
}

std::string ConfigManager::get(const std::string& section, const std::string& key, const std::string& def) const {
    auto it = data.find(section);
    if (it != data.end()) {
        auto it2 = it->second.find(key);
        if (it2 != it->second.end())
            return it2->second;
    }
    return def;
}

void ConfigManager::set(const std::string& section, const std::string& key, const std::string& value) {
    data[section][key] = value;
}

// --- Implementación de configuración de streaming ---

StreamConfiguration ConfigManager::getStreamConfig() const {
    StreamConfiguration config;
    config.width = std::stoi(get("stream", "width", "1280"));
    config.height = std::stoi(get("stream", "height", "720"));
    config.fps = std::stoi(get("stream", "fps", "60"));
    config.bitrate = std::stoi(get("stream", "bitrate", "-1"));
    config.packetSize = std::stoi(get("stream", "packetsize", "1024"));
    config.streamingRemotely = std::stoi(get("stream", "streaming_remotely", "0"));
    config.audioConfiguration = std::stoi(get("stream", "audio_config", "2"));
    config.supportedVideoFormats = std::stoi(get("stream", "video_formats", "1"));
    return config;
}

void ConfigManager::setStreamConfig(const StreamConfiguration& config) {
    set("stream", "width", std::to_string(config.width));
    set("stream", "height", std::to_string(config.height));
    set("stream", "fps", std::to_string(config.fps));
    set("stream", "bitrate", std::to_string(config.bitrate));
    set("stream", "packetsize", std::to_string(config.packetSize));
    set("stream", "streaming_remotely", std::to_string(config.streamingRemotely));
    set("stream", "audio_config", std::to_string(config.audioConfiguration));
    set("stream", "video_formats", std::to_string(config.supportedVideoFormats));
}

VideoSettings ConfigManager::getVideoSettings() const {
    VideoSettings settings;
    settings.sops = get("video", "sops", "true") == "true";
    settings.localaudio = get("video", "localaudio", "false") == "true";
    settings.fullscreen = get("video", "fullscreen", "true") == "true";
    settings.enable_frame_pacer = get("video", "enable_frame_pacer", "true") == "true";
    settings.center_region_only = get("video", "center_region_only", "false") == "true";
    settings.show_fps = get("video", "show_fps", "false") == "true";
    settings.save_debug_log = get("video", "save_debug_log", "false") == "true";
    settings.enable_ref_frame_invalidation = get("video", "enable_ref_frame_invalidation", "false") == "true";
    settings.enable_vita_vblank_wait = get("video", "enable_vita_vblank_wait", "false") == "true";
    settings.enable_motion_controls = get("video", "enable_motion_controls", "false") == "true";
    settings.enable_double_tap_sprint = get("video", "enable_double_tap_sprint", "false") == "true";
    settings.absolute_mouse = get("video", "absolute_mouse", "false") == "true";
    settings.touchscreen_mode = get("video", "touchscreen_mode", "false") == "true";
    settings.enable_network_optimizations = get("video", "enable_network_optimizations", "true") == "true";
    settings.double_tap_sprint_step_time = std::stoi(get("video", "double_tap_sprint_step_time", "200"));
    settings.motion_controls_scalar_x = std::stof(get("video", "motion_controls_scalar_x", "1.2"));
    settings.motion_controls_scalar_y = std::stof(get("video", "motion_controls_scalar_y", "0.8"));
    settings.mouse_acceleration = std::stoi(get("video", "mouse_acceleration", "150"));
    settings.keyboard_layout = std::stoi(get("video", "keyboard_layout", "0"));
    settings.render_mode = std::stoi(get("video", "render_mode", "0"));
    settings.pixel_format_mode = std::stoi(get("video", "pixel_format_mode", "0"));
    return settings;
}

void ConfigManager::setVideoSettings(const VideoSettings& settings) {
    set("video", "sops", settings.sops ? "true" : "false");
    set("video", "localaudio", settings.localaudio ? "true" : "false");
    set("video", "fullscreen", settings.fullscreen ? "true" : "false");
    set("video", "enable_frame_pacer", settings.enable_frame_pacer ? "true" : "false");
    set("video", "center_region_only", settings.center_region_only ? "true" : "false");
    set("video", "show_fps", settings.show_fps ? "true" : "false");
    set("video", "save_debug_log", settings.save_debug_log ? "true" : "false");
    set("video", "enable_ref_frame_invalidation", settings.enable_ref_frame_invalidation ? "true" : "false");
    set("video", "enable_vita_vblank_wait", settings.enable_vita_vblank_wait ? "true" : "false");
    set("video", "enable_motion_controls", settings.enable_motion_controls ? "true" : "false");
    set("video", "enable_double_tap_sprint", settings.enable_double_tap_sprint ? "true" : "false");
    set("video", "absolute_mouse", settings.absolute_mouse ? "true" : "false");
    set("video", "touchscreen_mode", settings.touchscreen_mode ? "true" : "false");
    set("video", "enable_network_optimizations", settings.enable_network_optimizations ? "true" : "false");
    set("video", "double_tap_sprint_step_time", std::to_string(settings.double_tap_sprint_step_time));
    set("video", "motion_controls_scalar_x", std::to_string(settings.motion_controls_scalar_x));
    set("video", "motion_controls_scalar_y", std::to_string(settings.motion_controls_scalar_y));
    set("video", "mouse_acceleration", std::to_string(settings.mouse_acceleration));
    set("video", "keyboard_layout", std::to_string(settings.keyboard_layout));
    set("video", "render_mode", std::to_string(settings.render_mode));
    set("video", "pixel_format_mode", std::to_string(settings.pixel_format_mode));
}