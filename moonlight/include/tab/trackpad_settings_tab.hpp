#pragma once

#include <borealis.hpp>

class TrackpadSettingsTab : public brls::Box
{
  public:
    TrackpadSettingsTab();

  // Pointer speed
  BRLS_BIND(brls::SliderCell, pointerSpeedSlider, "pointerSpeedSlider");
    
    // Click & Gesture Options Header
    // BRLS_BIND(brls::BooleanCell, tapToClickToggle, "tapToClickToggle");
    
    // Two finger right click
    BRLS_BIND(brls::BooleanCell, twoFingerRightClickToggle, "twoFingerRightClickToggle");
    
    // Two finger scrolling
    BRLS_BIND(brls::BooleanCell, twoFingerScrollToggle, "twoFingerScrollToggle");
    
    // Invert scroll direction
    BRLS_BIND(brls::BooleanCell, invertScrollToggle, "invertScrollToggle");
    
    // Multi-touch gestures
    BRLS_BIND(brls::BooleanCell, multiTouchGesturesToggle, "multiTouchGesturesToggle");
    
    // Edge zones
    BRLS_BIND(brls::SliderCell, edgeZoneSlider, "edgeZoneSlider");
    
    // Dead zone
    BRLS_BIND(brls::SliderCell, deadZoneSlider, "deadZoneSlider");

    static brls::View* create();

private:
    // Método auxiliar para aplicar cambios instantáneos del trackpad
    void applyTrackpadSettingsLive();
};
