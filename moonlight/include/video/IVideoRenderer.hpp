// IVideoRenderer.hpp - Interfaz genérica de renderer de video (similar a backends Switch)
#pragma once

class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;
    virtual void draw(float viewportW, float viewportH) = 0; // pinta frame actual
    virtual void onStreamConfig() {} // hook futuro para aplicar config (res/fps)
    virtual void onFrameReady() {}   // hook futuro si se requiere procesamiento adicional
};
