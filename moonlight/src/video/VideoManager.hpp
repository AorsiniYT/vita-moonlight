#pragma once

#include <string>
#include <memory>
#include "Limelight.h"
#include "ConfigManager.hpp"
#include "legacy/vita.h"
#include "ffmpeg/ffmpeg.h"

// VideoManager - Gestiona la selección y configuración del sistema de video
class VideoManager {
public:
    VideoManager();
    ~VideoManager();

    // Inicialización
    bool initialize();

    // Configuración del modo de renderizado
    void setRenderMode(const std::string& mode);
    std::string getRenderMode() const;

    // Callbacks para Limelight
    DECODER_RENDERER_CALLBACKS getDecoderCallbacks();

    // Control del video
    void startVideo();
    void stopVideo();

    // Estado
    bool isInitialized() const { return _initialized; }

private:
    // Contextos de video
    FFmpegVideoContext *_ffmpegContext;

    // Estado
    std::string _currentMode;
    int _currentModeInt;
    bool _initialized;
    bool _videoRunning;

    // ConfigManager
    ConfigManager _config;

    // Callbacks internos
    static int videoSetupCallback(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags);
    static void videoStartCallback(void);
    static void videoStopCallback(void);
    static void videoCleanupCallback(void);
    static int videoSubmitDecodeUnitCallback(PDECODE_UNIT decodeUnit);

    // Instancia singleton para callbacks
    static VideoManager* _instance;
};
