#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>
#include <string>
#include "model/HostStorage.hpp"
#include <cstdint>

// Overlay lateral para menú de pausa (aparece desde la derecha)
class VitaPauseOverlay : public brls::Box {
public:
    // onClose será llamado cuando el overlay se cierre (para que el llamador
    // pueda restablecer su flag de "overlay abierto").
    VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo);
    ~VitaPauseOverlay() override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    void drawFocus(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx);
    void onLayout() override;
    void willAppear(bool resetState = false) override;
    brls::View* getDefaultFocus() override { return focusDummy; }
    const char* describe() const { return "VitaPauseOverlay"; }
    bool isTranslucent() override { return true; }

private:
    void configureButtons();
    void resume();
    void disconnect();
    void closeApp();
    void moveFocus(int delta);
    void activateFocused();

    std::function<void()> onClose;
    HostInfo host;
    int focusedIndex = 0;
    std::vector<std::string> buttonLabels;
    // Último timestamp (ms) al que registramos un log de draw para evitar spam
    uint64_t lastDrawLogMs = 0;
    // Dummy para foco sin indicador visual
    brls::View* focusDummy = nullptr;
};
