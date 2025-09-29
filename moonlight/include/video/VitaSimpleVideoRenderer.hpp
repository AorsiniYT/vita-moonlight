// VitaSimpleVideoRenderer.hpp - Interfaz minimal propia (evita colisión con referencia Moonlight-Switch)
#pragma once

class VitaSimpleVideoRenderer {
public:
    virtual ~VitaSimpleVideoRenderer() = default;
    virtual void draw(float viewportW, float viewportH) = 0;
    virtual void onStreamConfig() {}
    virtual void onFrameReady() {}
};
