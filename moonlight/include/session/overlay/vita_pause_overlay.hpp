#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>
#include <string>
#include "model/HostStorage.hpp"
#include <cstdint>
#include "utils/overlay_utils.hpp"

// Overlay lateral para menú de pausa (aparece desde la derecha)
class VitaPauseOverlay : public BaseOverlay {
public:
    // onClose será llamado cuando el overlay se cierre (para que el llamador
    // pueda restablecer su flag de "overlay abierto").
    VitaPauseOverlay(std::function<void()> onClose, const HostInfo& hostInfo);
    ~VitaPauseOverlay() override;
    const char* describe() const { return "VitaPauseOverlay"; }

private:
    void resume();
    void disconnect();
    void closeApp();

    std::function<void()> onClose;
    HostInfo host;
};
