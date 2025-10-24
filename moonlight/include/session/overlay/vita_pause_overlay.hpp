#pragma once

#include <borealis.hpp>
#include <functional>
#include "model/HostStorage.hpp"

// Overlay lateral para menú de pausa (aparece desde la derecha)
class VitaPauseOverlay : public brls::Box {
public:
    // onClose será llamado cuando el overlay se cierre (para que el llamador
    // pueda restablecer su flag de "overlay abierto").
    VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo);
    ~VitaPauseOverlay() override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    bool isTranslucent() override { return true; }

private:
    void configureButtons();
    void resume();
    void disconnect();
    void closeApp();

    std::function<void()> onClose;
    HostInfo host;
    brls::Label* headerLabel = nullptr;
};
