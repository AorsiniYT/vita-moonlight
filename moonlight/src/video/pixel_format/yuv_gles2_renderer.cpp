#include "pixel_format.hpp"
#include <psp2/gxm.h>
#include <cstring>

namespace PixelFormat {

/**
 * Procesador YUV basado en GPU (GLES2/GXM)
 * El decoder produce YUV420 en hardware.
 * Este procesador sube los planos a GPU y hace conversión RGB con shader.
 * 
 * TODO: Implementar rendering YUV con shaders GXM para mejor rendimiento
 */
class YUVGpuRenderer : public IPixelProcessor {
private:
    int m_width;
    int m_height;
    uint32_t m_alignedWidth;
    uint32_t m_alignedHeight;
    
    // Buffer para YUV del decoder
    uint8_t* m_yuvBuffer;
    size_t m_yuvBufferSize;
    
    // Texturas GXM para los 3 planos YUV (futuro)
    // TODO: Implementar texturas y shaders
    
public:
    YUVGpuRenderer()
        : m_width(0)
        , m_height(0)
        , m_alignedWidth(0)
        , m_alignedHeight(0)
        , m_yuvBuffer(nullptr)
        , m_yuvBufferSize(0)
    {
    }
    
    ~YUVGpuRenderer() override {
        cleanup();
    }
    
    int init(int width, int height, uint32_t alignedWidth, uint32_t alignedHeight) override {
        m_width = width;
        m_height = height;
        m_alignedWidth = alignedWidth;
        m_alignedHeight = alignedHeight;
        
        // Allocate YUV buffer for decoder output
        size_t ySize = (size_t)alignedWidth * alignedHeight;
        size_t uvSize = ySize / 4;
        m_yuvBufferSize = ySize + 2 * uvSize;
        
        m_yuvBuffer = new uint8_t[m_yuvBufferSize + 4096]; // extra para alignment
        if (!m_yuvBuffer) {
            return -1;
        }
        
        // TODO: Crear texturas GXM para Y, U, V planes
        // TODO: Compilar shader YUV→RGB
        
        return 0;
    }
    
    uint32_t getDecoderPixelFormat() const override {
        // El decoder debe producir YUV420 en hardware
        return SCE_AVCDEC_PIXELFORMAT_YUV420_RASTER;
    }
    
    uint8_t* getDecodeTarget(void* frontBuffer, void* backBuffer) override {
        // Decodificar en el buffer YUV interno
        return m_yuvBuffer;
    }
    
    int postProcess(uint8_t* decodedBuffer, void* outputTexture) override {
        // TODO: Subir YUV planes a texturas GXM
        // TODO: Renderizar usando shader YUV→RGB
        
        // Por ahora stub (no implementado)
        return 0;
    }
    
    void cleanup() override {
        if (m_yuvBuffer) {
            delete[] m_yuvBuffer;
            m_yuvBuffer = nullptr;
        }
        m_yuvBufferSize = 0;
        
        // TODO: Liberar texturas GXM y shader resources
    }
    
    const char* getName() const override {
        return "YUV GPU Renderer (Stub)";
    }
    
    bool requiresStagingBuffer() const override {
        return false; // YUV no necesita staging, va directo a GPU
    }
};

// Factory implementation para YUV
IPixelProcessor* createYUVProcessor() {
    return new YUVGpuRenderer();
}

} // namespace PixelFormat
