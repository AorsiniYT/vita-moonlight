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
#include "../../third_party/moonmic/moonmic.h"  // For MOONMIC_DEFAULT_* constants

#include "controller/input_types.hpp"
#include "controller/special_inputs.hpp"

// Streaming configuration structure (similar to legacy)
struct StreamConfiguration {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrate = -1; // Self-calculated
    int packetSize = 1024;
    int streamingRemotely = 0;
    int audioConfiguration = 2; // AUDIO_CONFIGURATION_STEREO
    int supportedVideoFormats = 1; // VIDEO_FORMAT_H264

    // Method to validate and adjust resolution for PS Vita
    void validateAndAdjustResolution() {
#if defined(__PSV__) || defined(__psp2__) || defined(__PSP2__)
        // Apply PS Vita restrictions: multiples of 16, minimum 64
        width = (width < 64) ? 64 : ((width + 15) / 16) * 16;
        height = (height < 64) ? 64 : ((height + 15) / 16) * 16;
        
        // Limit to reasonable resolutions for PS Vita
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

// Enum for supported gamepad types
enum GamepadType : int {
    GAMEPAD_TYPE_XBOX = 0,    // Emular Xbox 360 (A/B/X/Y, LB/RB)
    GAMEPAD_TYPE_PS4 = 1,     // Emular PS4 (Cross/Circle/Square/Triangle, L1/R1)
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
    bool enable_network_optimizations = true; // Network optimizations (IDR smart, pacing, etc.)
    int touchscreen_mode = 1; // 0=Off, 1=Trackpad relativo, 2=DS4 Touchpad, 3=Mouse Absoluto, 4=Tableta Multitouch
    int double_tap_sprint_step_time = 200;
    float motion_controls_scalar_x = 1.2f;
    float motion_controls_scalar_y = 0.8f;
    int mouse_acceleration = 150;
    int keyboard_layout = 0; // 0=EN_US, 1=ES_ES, 2=ES_LATAM, 3=FR, 4=DE, 5=IT, 6=PT_BR, 7=UK
    int keyboard_mode = 0;    // 0=Legacy (SCE IME), 1=Modern (Borealis overlay)
    bool keyboard_input_mode = false; // false=VK (default), true=force UTF-8 text input
    bool keyboard_numbers_row = true; // Modern: show numeric row
    bool keyboard_show_arrows = true; // Modern: show arrow keys
    
    // Trackpad Settings (Specific)
    int trackpad_pointer_speed = 100;       // 0-200
    int trackpad_dead_zone = 50;            // 0-200px
    bool trackpad_tap_to_click = true;
    bool trackpad_two_finger_right_click = true;
    bool trackpad_two_finger_scroll = true;
    bool trackpad_invert_scroll = false;
    bool trackpad_multi_touch = true;
    int trackpad_edge_zone = 15;            // 0-50%
    
    // New option: render mode
    int render_mode = 0; // 0=Legacy, 1=Modern (FFmpeg)
    int pixel_format_mode = 1; // 0=RGBA direct, 1=YUV420 (testing)
    
    // New option: gamepad type
    GamepadType gamepad_type = GAMEPAD_TYPE_XBOX; // 0=Xbox, 1=PS4 (to emulate controller)

    // Swap shoulder buttons: L1/R1 <-> L2/R2 (quick shoulder adjustment)
    bool swap_shoulder_buttons = false;

    // Swap interval (V-Sync): 0 = Sin V-Sync, 1 = 60 FPS, 2 = 30 FPS, etc.
    int swap_interval = 0;

    // Buffer mode: 0 = Single (minimal latency), 1 = Double, 2 = Triple
    int buffer_mode = 0;

    // Front Touch Settings (4 corner zones on front screen)
    bool enable_front_touchzones = true;
    int front_touch_offset = 0;    // px from screen edge
    int front_touch_size = 150;    // px square zone size
    std::uint32_t front_action_northwest = controller::INPUT_TYPE_SPECIAL | controller::INPUT_SPECIAL_KEY_PAUSE;
    std::uint32_t front_action_northeast = 0; // None by default
    std::uint32_t front_action_southwest = controller::INPUT_TYPE_GAMEPAD | controller::GAMEPAD_FLAG_SPECIAL;
    std::uint32_t front_action_southeast = 0; // None by default
    
    // Microphone settings
    bool enable_microphone = false;         // Enable/disable microphone transmission
    bool enable_microphone_compression = false;  // true=Opus, false=RAW PCM (controlled by UI toggle)
    std::string microphone_host_ip = "";    // Host IP (empty = use stream host)
    int microphone_port = MOONMIC_DEFAULT_PORT;            // UDP port for mic transmission
    int microphone_sample_rate = MOONMIC_DEFAULT_SAMPLE_RATE;     // Sample rate (Hz) - Vita hardware native
    int microphone_channels = MOONMIC_DEFAULT_CHANNELS;            // 1=mono, 2=stereo
    int microphone_bitrate = MOONMIC_DEFAULT_BITRATE;         // Opus bitrate (bps) - optimal for 16kHz mono VOIP
    float microphone_gain = 10.0f;          // Gain multiplier (1.0 - 100.0, default 10.0)

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

    // --- Key/device directory management ---
    std::string getKeysDir() const;
    void setKeysDir(const std::string& dir);

    // Secure helpers to create/secure key-related directories
    // Ensure that the route exists (mkdir -p behavior). Returns true if
    // the route exists after the call or false on error.
    bool ensureDirExists(const std::string& path) const;
    bool ensureKeysDirExists() const;
    bool ensureKeyDirExists(const std::string& safeId) const;

    // --- Streaming configuration (legacy mode) ---
    StreamConfiguration getStreamConfig() const;
    void setStreamConfig(const StreamConfiguration& config);
    VideoSettings getVideoSettings() const;
    void setVideoSettings(const VideoSettings& settings);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
    mutable std::string cachedKeysDir;
    mutable bool keysDirLoaded = false;

};
