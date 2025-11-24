#include "pixel_format.hpp"
#include <vita2d.h>
#include <cstring>

namespace PixelFormat {

/**
 * Procesador RGBA basado en decodificación hardware
 * El decoder Vita produce RGBA directamente en hardware (rápido).
 * Este procesador solo maneja la copia de memoria.
 */
class RGBAHardwareProcessor : public IPixelProcessor {
private:
    int m_width;
    int m_height;
    uint32_t m_alignedWidth;
    uint32_t m_alignedHeight;
    
public:
    RGBAHardwareProcessor() 
        : m_width(0)
        , m_height(0)
        , m_alignedWidth(0)
        , m_alignedHeight(0)
    {
    }
    
    ~RGBAHardwareProcessor() override {
        cleanup();
    }
    
    int init(int width, int height, uint32_t alignedWidth, uint32_t alignedHeight) override {
        m_width = width;
        m_height = height;
        m_alignedWidth = alignedWidth;
        m_alignedHeight = alignedHeight;
        return 0;
    }
    
    uint32_t getDecoderPixelFormat() const override {
        // El decoder debe producir RGBA directamente en hardware
        return SCE_AVCDEC_PIXELFORMAT_RGBA8888;
    }
    
    uint8_t* getDecodeTarget(void* frontBuffer, void* backBuffer) override {
        // Decodificar directamente en el back buffer de la textura
        vita2d_texture* backTex = static_cast<vita2d_texture*>(backBuffer);
        if (backTex) {
            return static_cast<uint8_t*>(vita2d_texture_get_datap(backTex));
        }
        return nullptr;
    }
    
    int postProcess(uint8_t* decodedBuffer, void* outputTexture) override {
        // Para RGBA directo no hay post-procesamiento necesario
        // El decoder ya escribió directamente en la textura
        return 0;
    }
    
    void cleanup() override {
        // Nada que limpiar, las texturas se manejan externamente
    }
    
    const char* getName() const override {
        return "RGBA Hardware Decoder";
    }
    
    bool requiresStagingBuffer() const override {
        // Puede usar staging buffer si está disponible
        return false;
    }
};

// Factory implementation para RGBA
IPixelProcessor* createRGBAProcessor() {
    return new RGBAHardwareProcessor();
}

} // namespace PixelFormat
