#include "session/streaming_manager.hpp"
#include "Limelight.h"
#include "video/VideoManager.hpp"
#include <borealis/core/thread.hpp>

// Implementación de los callbacks estáticos

void StreamingManager::connection_stage_starting(int stage) {
    brls::Logger::info("[STREAM] Etapa de conexión iniciando: {}", LiGetStageName(stage));
}

void StreamingManager::connection_stage_complete(int stage) {
    brls::Logger::info("[STREAM] Etapa de conexión completa: {}", LiGetStageName(stage));
}

void StreamingManager::connection_stage_failed(int stage, int error_code) {
    brls::Logger::error("[STREAM] Etapa de conexión fallida: {} | Código: {}", LiGetStageName(stage), error_code);
}

void StreamingManager::connection_started() {
    brls::Logger::info("[STREAM] ¡Conexión iniciada!");
}

void StreamingManager::connection_terminated(int error_code) {
    brls::Logger::info("[STREAM] Conexión terminada. Código: {}", error_code);
}

void StreamingManager::log_message(const char* format, ...) {
    // Nota: Sería mejor usar un buffer y vsnprintf para pasar a brls::Logger
    // pero para la depuración inicial, una simple impresión es suficiente.
    // va_list args;
    // va_start(args, format);
    // vprintf(format, args);
    // va_end(args);
}

int StreamingManager::video_decoder_setup(int video_format, int width, int height, int redraw_rate, void* context, int dr_flags) {
    brls::Logger::info("[VIDEO] Configurando decodificador: formato={}, {}x{}, {}fps", video_format, width, height, redraw_rate);
    // Aquí iría la lógica real para inicializar el decodificador de video de la PSVita
    return 0; // Éxito
}

void StreamingManager::video_decoder_start() {
    brls::Logger::info("[VIDEO] Iniciando decodificador.");
}

void StreamingManager::video_decoder_stop() {
    brls::Logger::info("[VIDEO] Deteniendo decodificador.");
}

void StreamingManager::video_decoder_cleanup() {
    brls::Logger::info("[VIDEO] Limpiando decodificador.");
}

int StreamingManager::video_decoder_submit_decode_unit(PDECODE_UNIT decode_unit) {
    // brls::Logger::info("[VIDEO] Recibida unidad de decodificación, frame: {}", decode_unit->frameNumber);
    // Aquí se procesarían los datos de video
    // Liberar la memoria de la unidad de decodificación
    PLENTRY entry = decode_unit->bufferList;
    while (entry) {
        PLENTRY next = entry->next;
        free(entry->data);
        free(entry);
        entry = next;
    }
    free(decode_unit);
    return DR_OK;
}

int StreamingManager::audio_renderer_init(int audio_config, const POPUS_MULTISTREAM_CONFIGURATION opus_config, void* context, int ar_flags) {
    brls::Logger::info("[AUDIO] Inicializando renderer: config={}", audio_config);
    return 0; // Éxito
}

void StreamingManager::audio_renderer_start() {
    brls::Logger::info("[AUDIO] Iniciando renderer.");
}

void StreamingManager::audio_renderer_stop() {
    brls::Logger::info("[AUDIO] Deteniendo renderer.");
}

void StreamingManager::audio_renderer_cleanup() {
    brls::Logger::info("[AUDIO] Limpiando renderer.");
}

void StreamingManager::audio_renderer_decode_and_play_sample(char* sample_data, int sample_length) {
    // brls::Logger::info("[AUDIO] Recibida muestra de audio, tamaño: {}", sample_length);
    free(sample_data);
}

// Implementación de la clase StreamingManager

StreamingManager::StreamingManager() {
    // Inicializar VideoManager
    _videoManager = std::make_unique<VideoManager>();
    if (!_videoManager->initialize()) {
        brls::Logger::error("[StreamingManager] Error inicializando VideoManager");
    }

    // Inicializar los structs de callbacks a cero
    LiInitializeConnectionCallbacks(&_conn_callbacks);
    LiInitializeVideoCallbacks(&_video_callbacks);
    LiInitializeAudioCallbacks(&_audio_callbacks);

    // Asignar las funciones estáticas a los punteros de función
    _conn_callbacks.stageStarting = connection_stage_starting;
    _conn_callbacks.stageComplete = connection_stage_complete;
    _conn_callbacks.stageFailed = connection_stage_failed;
    _conn_callbacks.connectionStarted = connection_started;
    _conn_callbacks.connectionTerminated = connection_terminated;
    _conn_callbacks.logMessage = log_message;

    // Usar callbacks del VideoManager en lugar de los básicos
    if (_videoManager->isInitialized()) {
        _video_callbacks = _videoManager->getDecoderCallbacks();
        brls::Logger::info("[StreamingManager] Usando callbacks del VideoManager");
        // Debug: log pointer addresses assigned to callbacks so they show in device dumps
        brls::Logger::info("[StreamingManager] Video callbacks ptrs - setup: {:#x}, start: {:#x}, stop: {:#x}, cleanup: {:#x}, submit: {:#x}",
                          reinterpret_cast<uintptr_t>(_video_callbacks.setup),
                          reinterpret_cast<uintptr_t>(_video_callbacks.start),
                          reinterpret_cast<uintptr_t>(_video_callbacks.stop),
                          reinterpret_cast<uintptr_t>(_video_callbacks.cleanup),
                          reinterpret_cast<uintptr_t>(_video_callbacks.submitDecodeUnit));
        brls::Logger::info("[StreamingManager] Callbacks configurados - setup: {}, start: {}, stop: {}, cleanup: {}, submit: {}",
                          (_video_callbacks.setup != nullptr ? "OK" : "NULL"),
                          (_video_callbacks.start != nullptr ? "OK" : "NULL"),
                          (_video_callbacks.stop != nullptr ? "OK" : "NULL"),
                          (_video_callbacks.cleanup != nullptr ? "OK" : "NULL"),
                          (_video_callbacks.submitDecodeUnit != nullptr ? "OK" : "NULL"));
    } else {
        // Fallback a callbacks básicos si VideoManager falla
        _video_callbacks.setup = video_decoder_setup;
        _video_callbacks.start = video_decoder_start;
        _video_callbacks.stop = video_decoder_stop;
        _video_callbacks.cleanup = video_decoder_cleanup;
        _video_callbacks.submitDecodeUnit = video_decoder_submit_decode_unit;
        brls::Logger::warning("[StreamingManager] VideoManager no inicializado, usando callbacks básicos");
    }

    _audio_callbacks.init = audio_renderer_init;
    _audio_callbacks.start = audio_renderer_start;
    _audio_callbacks.stop = audio_renderer_stop;
    _audio_callbacks.cleanup = audio_renderer_cleanup;
    _audio_callbacks.decodeAndPlaySample = audio_renderer_decode_and_play_sample;
}

StreamingManager::~StreamingManager() {
    if (_is_running) {
        stop();
    }
}

bool StreamingManager::start(SERVER_DATA& server, STREAM_CONFIGURATION& streamConfig) {
    brls::Logger::info("[STREAM] Iniciando conexión de streaming...");

    // Debug: Verificar parámetros de entrada
    const char* input_addr = server.serverInfo.address ? server.serverInfo.address : "NULL";
    brls::Logger::info("[STREAM] Parámetros de entrada - Server address: '{}', HTTPS port: {}",
                      input_addr, server.httpsPort);
    brls::Logger::info("[STREAM] Parámetros de entrada - Stream: {}x{} @ {}fps, {}kbps",
                      streamConfig.width, streamConfig.height, streamConfig.fps, streamConfig.bitrate);

    // Iniciar video si VideoManager está disponible
    if (_videoManager && _videoManager->isInitialized()) {
            // Iniciar video según la configuración que haya seleccionado el usuario
            _videoManager->startVideo();
    }

    // La llamada a LiStartConnection es bloqueante, por lo que la ejecutamos en un hilo
    // Usar referencia directa en lugar de copia superficial (patrón Moonlight-Switch)
    brls::async([this, &server, &streamConfig]() mutable {
        brls::Logger::info("[STREAM] Llamando a LiStartConnection con parámetros:");
        const char* server_addr = server.serverInfo.address ? server.serverInfo.address : "NULL";
        brls::Logger::info("[STREAM] - Server: {}:{}", server_addr, server.httpsPort);
        brls::Logger::info("[STREAM] - Stream: {}x{} @ {}fps, {}kbps",
                          streamConfig.width, streamConfig.height, streamConfig.fps, streamConfig.bitrate);
        brls::Logger::info("[STREAM] - Callbacks válidos: conn={}, video={}, audio={}",
                          (_conn_callbacks.connectionStarted != nullptr ? "OK" : "NULL"),
                          (_video_callbacks.setup != nullptr ? "OK" : "NULL"),
                          (_audio_callbacks.init != nullptr ? "OK" : "NULL"));

        // Verificar que serverInfo esté inicializada
        const char* srv_addr = server.serverInfo.address ? server.serverInfo.address : "NULL";
        const char* srv_url = server.serverInfo.rtspSessionUrl ? server.serverInfo.rtspSessionUrl : "NULL";
        brls::Logger::info("[STREAM] - ServerInfo address: {}", srv_addr);
        brls::Logger::info("[STREAM] - ServerInfo rtspSessionUrl: {}", srv_url);

        void* renderContext = nullptr;
        if (_videoManager) {
            renderContext = _videoManager->getRenderContext();
        }

        int result = LiStartConnection(
            &server.serverInfo,
            &streamConfig,
            &_conn_callbacks,
            &_video_callbacks,
            &_audio_callbacks,
            renderContext, // renderContext (may be FFmpeg context or NULL)
            0,       // drFlags
            nullptr, // audioContext
            0        // arFlags
        );

        if (result == 0) {
            brls::Logger::info("[STREAM] LiStartConnection finalizó correctamente.");
        } else {
            brls::Logger::error("[STREAM] LiStartConnection falló con el código: {}", result);
        }
        _is_running = false;
    });

    _is_running = true;
    return true; // La función devuelve inmediatamente
}

void StreamingManager::stop() {
    brls::Logger::info("[STREAM] Deteniendo conexión de streaming...");

    // Detener video
    if (_videoManager && _videoManager->isInitialized()) {
        _videoManager->stopVideo();
    }

    LiStopConnection();
    _is_running = false;
}
