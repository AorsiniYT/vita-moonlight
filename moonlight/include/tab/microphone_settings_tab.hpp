#pragma once

#include <borealis.hpp>
#include <string>
#include <chrono>

class MicrophoneSettingsTab : public brls::Box {
public:
    MicrophoneSettingsTab();
    ~MicrophoneSettingsTab();  // Destructor to stop test on exit
    
    static brls::View* create();
    
private:
    std::string getGainText(float gain);
    void updateConnectionStatus();  // Update LED based on connection status
    
    std::string currentHostAddress;  // Selected host for remote testing
    int currentHostPort = 48100;
    
    BRLS_BIND(brls::Slider, gainSlider, "gainSlider");
    BRLS_BIND(brls::Label, gainLabel, "gainLabel");
    BRLS_BIND(brls::BooleanCell, opusToggle, "opusToggle");
    BRLS_BIND(brls::BooleanCell, testMicrophoneToggle, "testMicrophoneToggle");
    BRLS_BIND(brls::SelectorCell, hostSelector, "hostSelector");
    BRLS_BIND(brls::BooleanCell, testMicrophoneRemoteToggle, "testMicrophoneRemoteToggle");
    BRLS_BIND(brls::DetailCell, portInputCell, "portInputCell");
    BRLS_BIND(brls::Rectangle, connectionLED, "connectionLED");
    BRLS_BIND(brls::Label, hostStatusLabel, "hostStatusLabel");
    
    // Monitor flag for connection status updates
    bool monitoringConnection = false;
    
    // Throttle connection status updates (avoid calling every frame)
    std::chrono::steady_clock::time_point lastConnectionCheck;
    static constexpr int CONNECTION_CHECK_INTERVAL_MS = 500;  // Check every 500ms
    
    // Override frame to check connection status periodically
    void frame(brls::FrameContext* ctx) override {
        brls::Box::frame(ctx);
        
        // Update connection status if monitoring is active (throttled)
        if (monitoringConnection) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastConnectionCheck).count();
            if (elapsed >= CONNECTION_CHECK_INTERVAL_MS) {
                updateConnectionStatus();
                lastConnectionCheck = now;
            }
        }
    }
};
