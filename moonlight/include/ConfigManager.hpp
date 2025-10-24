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
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "controller/input_types.hpp"
#include "controller/special_inputs.hpp"

// Estructura de configuración de streaming (similar al legacy)
struct StreamConfiguration {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrate = -1; // Auto-calculado
    int packetSize = 1024;
    int streamingRemotely = 0;
    int audioConfiguration = 2; // AUDIO_CONFIGURATION_STEREO
    int supportedVideoFormats = 1; // VIDEO_FORMAT_H264

    // Método para validar y ajustar resolución para PS Vita
    void validateAndAdjustResolution() {
#if defined(__PSV__) || defined(__psp2__) || defined(__PSP2__)
        // Aplicar restricciones de PS Vita: múltiplos de 16, mínimo 64
        width = (width < 64) ? 64 : ((width + 15) / 16) * 16;
        height = (height < 64) ? 64 : ((height + 15) / 16) * 16;
        
        // Limitar a resoluciones razonables para PS Vita
        if (width > 1920) width = 1920;
        if (height > 1080) height = 1080;
#endif
    }
};

struct RearTouchSettings {
    bool enabled = true;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
    std::uint32_t actionNorthWest = controller::INPUT_TYPE_ANALOG | controller::ANALOG_LEFT_TRIGGER;
    std::uint32_t actionNorthEast = controller::INPUT_TYPE_ANALOG | controller::ANALOG_RIGHT_TRIGGER;
    std::uint32_t actionSouthWest = controller::INPUT_TYPE_GAMEPAD | controller::GAMEPAD_FLAG_LS;
    std::uint32_t actionSouthEast = controller::INPUT_TYPE_GAMEPAD | controller::GAMEPAD_FLAG_RS;
};

struct VideoSettings {
    bool sops = true; // Stream Optimization
    bool localaudio = false;
    bool fullscreen = true;
    bool enable_frame_pacer = true;
    bool center_region_only = false;
    bool show_fps = false;
    bool save_debug_log = false;
    bool enable_ref_frame_invalidation = false;
    bool enable_vita_vblank_wait = false;
    bool enable_motion_controls = false;
    bool enable_double_tap_sprint = false;
    bool absolute_mouse = false;
    bool enable_network_optimizations = true; // Optimizaciones de red (IDR smart, pacing, etc.)
    int touchscreen_mode = 0; // 0=Off, 1=DS4 Touchpad, 2=Mouse Absoluto, 3=Tableta Multitouch
    int double_tap_sprint_step_time = 200;
    float motion_controls_scalar_x = 1.2f;
    float motion_controls_scalar_y = 0.8f;
    int mouse_acceleration = 150;
    int keyboard_layout = 0; // 0=EN_US, 1=ES_ES, 2=ES_LATAM
    
    // Nueva opción: modo de renderizado
    int render_mode = 0; // 0=Legacy, 1=Modern (FFmpeg)
    int pixel_format_mode = 0; // 0=RGBA directo, 1=YUV420 (pruebas)

    RearTouchSettings rear_touch;
};

class ConfigManager {
public:
    ConfigManager();
    bool load();
    bool save() const;
    std::string get(const std::string& section, const std::string& key, const std::string& def = "") const;
    void set(const std::string& section, const std::string& key, const std::string& value);
    static std::string getConfigPath();

    // --- Gestión de directorio de llaves/dispositivos ---
    std::string getKeysDir() const;
    void setKeysDir(const std::string& dir);

    // Helpers seguros para crear/asegurar directorios relacionados con keys
    // Garantiza que la ruta exista (mkdir -p behaviour). Devuelve true si
    // la ruta existe tras la llamada o false en error.
    bool ensureDirExists(const std::string& path) const;
    bool ensureKeysDirExists() const;
    bool ensureKeyDirExists(const std::string& safeId) const;

    // --- Configuración de streaming (modo legacy) ---
    StreamConfiguration getStreamConfig() const;
    void setStreamConfig(const StreamConfiguration& config);
    VideoSettings getVideoSettings() const;
    void setVideoSettings(const VideoSettings& settings);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
    mutable std::string cachedKeysDir;
    mutable bool keysDirLoaded = false;

};
