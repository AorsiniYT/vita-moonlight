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

#include "MicrophoneManager.hpp"
#include "ConfigManager.hpp"
#include "GameStreamClient.hpp"
#include "model/HostStorage.hpp"
#include <borealis.hpp>

MicrophoneManager& MicrophoneManager::getInstance() {
    static MicrophoneManager instance;
    return instance;
}

MicrophoneManager::MicrophoneManager() {
    brls::Logger::info("[MicrophoneManager] Initialized");
}

MicrophoneManager::~MicrophoneManager() {
    stop();
    brls::Logger::info("[MicrophoneManager] Destroyed");
}

bool MicrophoneManager::start(const std::string& hostIp, int port, int sampleRate, int channels, int bitrate) {
    if (running_) {
        brls::Logger::warning("[MicrophoneManager] Already running");
        return true;
    }
    
    // Store configuration
    host_ip_ = hostIp;
    port_ = port;
    sample_rate_ = sampleRate;
    channels_ = channels;
    bitrate_ = bitrate;
    
    brls::Logger::info("[MicrophoneManager] Starting microphone ({}:{}, {}Hz, {}ch, {}bps)", 
                       host_ip_, port_, sample_rate_, channels_, bitrate_);
    
    brls::Logger::warning("[MicrophoneManager] Delaying microphone start by 3 seconds to avoid port conflict...");
    
    // WORKAROUND: Delay microphone start to avoid conflict with Moonlight audio initialization
    // The streaming audio (sceAudioOut) seems to interfere with microphone (sceAudioIn) if started simultaneously
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    brls::Logger::info("[MicrophoneManager] Delay complete, attempting to start microphone...");
    
    // Try to start immediately
    if (startInternal()) {
        brls::Logger::info("[MicrophoneManager] Started successfully");
        return true;
    }
    
    // If failed, enable retry thread
    brls::Logger::warning("[MicrophoneManager] Initial start failed, enabling retry every {} seconds", 
                          RETRY_INTERVAL_SECONDS);
    
    retry_enabled_ = true;
    retry_thread_ = std::thread(&MicrophoneManager::retryThreadFunc, this);
    
    return false; // Not running yet, but retry enabled
}

void MicrophoneManager::stop() {
    brls::Logger::info("[MicrophoneManager] Stopping...");
    
    // Stop retry thread first
    retry_enabled_ = false;
    if (retry_thread_.joinable()) {
        retry_thread_.join();
        brls::Logger::debug("[MicrophoneManager] Retry thread stopped");
    }
    
    // Stop microphone client
    if (client_) {
        moonmic_destroy(client_);
        client_ = nullptr;
        running_ = false;
        brls::Logger::info("[MicrophoneManager] Microphone stopped");
    }
}

bool MicrophoneManager::isRunning() const {
    return running_;
}

bool MicrophoneManager::isRetrying() const {
    return retry_enabled_;
}

std::string MicrophoneManager::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

std::string MicrophoneManager::getHostIp() const {
    return host_ip_;
}

int MicrophoneManager::getPort() const {
    return port_;
}

bool MicrophoneManager::startInternal() {
    // Clean up existing client if any
    if (client_) {
        moonmic_destroy(client_);
        client_ = nullptr;
    }
    
    // Clear previous error
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_.clear();
    }
    
    // Load compression mode from config
    ConfigManager cfg;
    cfg.load();
    VideoSettings settings = cfg.getVideoSettings();
    
    // raw_mode is INVERSE of enable_microphone_compression
    // Opus compression ON (true) → raw_mode = false (use Opus)
    // Opus compression OFF (false) → raw_mode = true (use RAW PCM)
    bool raw_mode = !settings.enable_microphone_compression;
    
    brls::Logger::info("[MicrophoneManager] Compression mode: {} (raw_mode={})", 
                       settings.enable_microphone_compression ? "Opus" : "RAW PCM", 
                       raw_mode);
    
    // Perform Sunshine validation
    // IMPORTANT: We need to find the host in HostStorage to get the proper keyDir
    std::vector<HostInfo> hosts = HostStorage::loadHosts();
    std::string keyDir;
    
    for (const auto& host : hosts) {
        if (host.ip == host_ip_) {
            // Found matching host - try to connect to populate GameStreamClient map
            if (!GameStreamClient::instance().isConnected(host.ip)) {
                brls::Logger::info("[MicrophoneManager] Connecting to {} to load keyDir", host.name);
                GameStreamClient::instance().connect(host);
            }
            
            keyDir = GameStreamClient::instance().getKeyDirFor(host.ip);
            break;
        }
    }
    
    if (keyDir.empty()) {
        brls::Logger::warning("[MicrophoneManager] No keyDir found for {}, validation will use PairStatus=0", host_ip_);
    }
    
    int pair_status = 0;
    if (!keyDir.empty()) {
        pair_status = GameStreamClient::instance().getSunshinePairStatus(host_ip_);
    } else {
        brls::Logger::warning("[MicrophoneManager] Skipping Sunshine validation (no keyDir)");
    }
    
    // Get uniqueid and devicename
    std::string uniqueid;
    if (!keyDir.empty()) {
        std::string uniqueidPath = keyDir + "/uniqueid.dat";
        FILE* f = fopen(uniqueidPath.c_str(), "r");
        if (f) {
            char buf[32] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                uniqueid = buf;
                while (!uniqueid.empty() && (uniqueid.back() == '\n' || uniqueid.back() == '\r')) uniqueid.pop_back();
            }
            fclose(f);
        }
    }
    
    // Device name (default to "Vita" if not set)
    std::string devicename = "Vita"; 
    // Ideally we should get this from settings, but for now hardcode or use hostname
    
    brls::Logger::info("[MicrophoneManager] Sunshine validation result: PairStatus={}", pair_status);
    brls::Logger::info("[MicrophoneManager] uniqueid='{}' len={}", uniqueid, uniqueid.length());
    brls::Logger::info("[MicrophoneManager] devicename='{}' len={}", devicename, devicename.length());

    // Configure libmoonmic
    moonmic_config_t config = {\
        .host_ip = host_ip_.c_str(),
        .port = static_cast<uint16_t>(port_),
        .sample_rate = static_cast<uint32_t>(sample_rate_),
        .channels = static_cast<uint8_t>(channels_),
        .bitrate = static_cast<uint32_t>(bitrate_),
        .raw_mode = raw_mode,  // Set based on UI toggle
        .auto_start = true,  // Start capturing immediately
        .gain = settings.microphone_gain,  // Gain multiplier from config
        
        // Sunshine validation
        .uniqueid = uniqueid.c_str(),
        .devicename = devicename.c_str(),
        .sunshine_https_port = 47984, // Default Sunshine port
        .pair_status = pair_status
    };
    
    // Create client
    client_ = moonmic_create(&config);
    
    if (!client_) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "Failed to create microphone client";
        brls::Logger::error("[MicrophoneManager] {}", last_error_);
        return false;
    }
    
    // Set error callback
    moonmic_set_error_callback(client_, &MicrophoneManager::errorCallback, this);
    
    // Success
    running_ = true;
    retry_enabled_ = false; // Stop retry thread if it was running
    
    brls::Logger::info("[MicrophoneManager] Client created successfully");
    return true;
}

void MicrophoneManager::retryThreadFunc() {
    brls::Logger::info("[MicrophoneManager] Retry thread started");
    
    while (retry_enabled_) {
        // Sleep for retry interval
        for (int i = 0; i < RETRY_INTERVAL_SECONDS && retry_enabled_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!retry_enabled_) break;
        
        brls::Logger::info("[MicrophoneManager] Retrying connection to {}:{}", host_ip_, port_);
        
        // Attempt to start
        if (startInternal()) {
            brls::Logger::info("[MicrophoneManager] Retry successful - microphone now running");
            break; // Exit retry loop
        } else {
            brls::Logger::warning("[MicrophoneManager] Retry failed, will try again in {} seconds", 
                                  RETRY_INTERVAL_SECONDS);
        }
    }
    
    brls::Logger::info("[MicrophoneManager] Retry thread stopped");
}

void MicrophoneManager::errorCallback(const char* error, void* userData) {
    auto* mgr = static_cast<MicrophoneManager*>(userData);
    if (!mgr) return;
    
    {
        std::lock_guard<std::mutex> lock(mgr->error_mutex_);
        mgr->last_error_ = error ? error : "Unknown error";
    }
    
    brls::Logger::error("[MicrophoneManager] libmoonmic error: {}", 
                        error ? error : "Unknown error");
}

void MicrophoneManager::setGain(float gain) {
    // Update gain for future startInternal() calls
    ConfigManager config;
    config.load();
    VideoSettings settings = config.getVideoSettings();
    settings.microphone_gain = gain;
    config.setVideoSettings(settings);
    config.save();
    
    // If currently running, update the client's gain immediately
    if (running_ && client_) {
        moonmic_set_gain(client_, gain);
        brls::Logger::info("[MicrophoneManager] Updated gain to {:.1f}x during transmission", gain);
    }
}

bool MicrophoneManager::isConnected() const {
    if (!running_ || !client_) {
        return false;
    }
    return moonmic_is_connected(client_);
}
