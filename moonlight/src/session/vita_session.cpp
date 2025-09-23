#include "session/vita_session.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <ctime>
#include "video/legacy/vita.h"

// Flag global para forzar un launch fresco (evitar /resume) tras detectar stream HEVC no soportado
bool g_force_fresh_launch_h264 = false;

VitaSession* VitaSession::s_active = nullptr;

VitaSession::VitaSession(const std::string& address, int appId, bool isSunshine)
: m_address(address), m_app_id(appId), m_is_sunshine(isSunshine) {
    s_active = this;
}

VitaSession::~VitaSession() {
    if (s_active == this) s_active = nullptr;
}

VitaSession* VitaSession::active() {
    return s_active;
}

void VitaSession::destroyActive(bool terminateApp) {
    if (!s_active) return;
    VitaSession* tmp = s_active;
    if (terminateApp) tmp->stop(true); else tmp->stop(false);
    delete tmp; // destructor limpia s_active
}

bool VitaSession::start() {
    // Intentar reutilizar la configuración usada en startApp (incluye remoteInputAesKey generado)
    STREAM_CONFIGURATION lastCfg{};
    bool haveLast = GameStreamClient::instance().lastStreamConfig(m_address, lastCfg);
    if (haveLast) {
        m_config = lastCfg; // copia directa
        brls::Logger::info("[VitaSession] Reutilizando configuración previa de startApp (key preservada)");
    } else {
        LiInitializeStreamConfiguration(&m_config);
        m_config.width = 960;
        m_config.height = 544;
        m_config.fps = 30;
        m_config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
        m_config.bitrate = 8000;
        m_config.encryptionFlags = m_is_sunshine ? ENCFLG_ALL : ENCFLG_NONE;
        m_config.packetSize = 1024;
        m_config.streamingRemotely = STREAM_CFG_AUTO;
#ifdef VIDEO_FORMAT_H264
        m_config.supportedVideoFormats = VIDEO_FORMAT_H264; // Forzar siempre solo H.264
#endif
    }
    // Limpieza agresiva de cualquier bit no H.264 por seguridad
    if ((m_config.supportedVideoFormats & ~VIDEO_FORMAT_H264) != 0) {
        brls::Logger::warning("[VitaSession][ForceH264] Limpiando bits no H.264 (antes=0x{:X})", m_config.supportedVideoFormats);
        m_config.supportedVideoFormats = VIDEO_FORMAT_H264;
    }
    brls::Logger::info("[VitaSession][ForceH264] Mask final=0x{:X}", m_config.supportedVideoFormats);
    // Forzar encryption flags si la URL requiere cifrado
    SERVER_DATA& sd = GameStreamClient::instance().serverData(m_address);
    if (sd.serverInfo.rtspSessionUrl && strncmp(sd.serverInfo.rtspSessionUrl, "rtspenc://", 10) == 0) {
        if (m_config.encryptionFlags != ENCFLG_ALL) {
            m_config.encryptionFlags = ENCFLG_ALL;
            brls::Logger::info("[VitaSession] rtspenc detectado -> encryptionFlags=ALL");
        }
    }
    // Derivar IV base si está vacío (rikeyid=0 => dejamos ceros; si en futuro rikeyid cambia se podría integrar)
    bool keyAllZero = true; for (unsigned i=0;i<16;i++) if (m_config.remoteInputAesKey[i]!=0) { keyAllZero=false; break; }
    if (keyAllZero) {
        brls::Logger::warning("[VitaSession] remoteInputAesKey vacío; posible fallo de cifrado RTSP si host espera cifrado");
    }
    else {
        char keyPrev[32];
        snprintf(keyPrev, sizeof(keyPrev), "%02X%02X%02X%02X%02X%02X...",
                 (unsigned char)m_config.remoteInputAesKey[0], (unsigned char)m_config.remoteInputAesKey[1],
                 (unsigned char)m_config.remoteInputAesKey[2], (unsigned char)m_config.remoteInputAesKey[3],
                 (unsigned char)m_config.remoteInputAesKey[4], (unsigned char)m_config.remoteInputAesKey[5]);
        brls::Logger::info("[VitaSession] remoteInputAesKey preview={} (copiada)", keyPrev);
    }
    brls::Logger::info("[VitaSession] Config base: {}x{}@{}fps bitrate={}K packetSize={} streamingRemotely={} formats=0x{:X}",
                       m_config.width, m_config.height, m_config.fps, m_config.bitrate,
                       m_config.packetSize, m_config.streamingRemotely, m_config.supportedVideoFormats);

    // Instrumentación de formato de video soportado/negociado
    if ((m_config.supportedVideoFormats & 0x0001) != 0) {
        brls::Logger::info("[VitaSession][VideoFormat] H.264 habilitado (mask=0x0001)");
    }
    if ((m_config.supportedVideoFormats & 0x0F00) != 0) {
        brls::Logger::warning("[VitaSession][VideoFormat] H.265/HEVC bits presentes en mask=0x{:X} -> NO soportado por PS Vita, se intentará H.264 únicamente", m_config.supportedVideoFormats);
        // Forzar limpieza de bits HEVC si aparecieran
        m_config.supportedVideoFormats &= ~0x0F00; // VIDEO_FORMAT_MASK_H265
    }
    // En caso de que por configuración se haya puesto otro formato no soportado, lo indicamos
    if ((m_config.supportedVideoFormats & ~0x0001) != 0) {
        brls::Logger::warning("[VitaSession][VideoFormat] Se detectaron formatos adicionales no soportados (mask=0x{:X})", m_config.supportedVideoFormats);
        m_config.supportedVideoFormats = 0x0001; // VIDEO_FORMAT_H264
    }

    LiInitializeConnectionCallbacks(&m_conn_callbacks);
    m_conn_callbacks.stageStarting = connection_stage_starting;
    m_conn_callbacks.stageComplete = connection_stage_complete;
    m_conn_callbacks.stageFailed = connection_stage_failed;
    m_conn_callbacks.connectionStarted = connection_started;
    m_conn_callbacks.connectionTerminated = connection_terminated;
    m_conn_callbacks.logMessage = connection_log_message;
    m_conn_callbacks.connectionStatusUpdate = connection_status_update;

    // Usar backend real de video (decoder_callbacks_vita). Copiamos para permitir ajustes futuros sin tocar global.
    LiInitializeVideoCallbacks(&m_video_callbacks);
    m_video_callbacks = decoder_callbacks_vita_new;
    brls::Logger::info("[VitaSession] Video backend habilitado (capabilities=0x{:X})", m_video_callbacks.capabilities);

    LiInitializeAudioCallbacks(&m_audio_callbacks);
    m_audio_callbacks.init = audio_renderer_init;
    m_audio_callbacks.start = audio_renderer_start;
    m_audio_callbacks.stop = audio_renderer_stop;
    m_audio_callbacks.cleanup = audio_renderer_cleanup;
    m_audio_callbacks.decodeAndPlaySample = audio_renderer_decode_and_play_sample;

    return internalStart();
}

bool VitaSession::internalStart() {
    SERVER_DATA& data = GameStreamClient::instance().serverData(m_address);
    // Instrumentación: log de callbacks de video antes de iniciar conexión
    brls::Logger::info("[VitaSession][DBG] video callbacks ptrs: setup=%p start=%p stop=%p cleanup=%p submit=%p caps=0x%X",
                       (void*)m_video_callbacks.setup,
                       (void*)m_video_callbacks.start,
                       (void*)m_video_callbacks.stop,
                       (void*)m_video_callbacks.cleanup,
                       (void*)m_video_callbacks.submitDecodeUnit,
                       m_video_callbacks.capabilities);
    int result = LiStartConnection(&data.serverInfo, &m_config, &m_conn_callbacks,
                                   &m_video_callbacks, &m_audio_callbacks, NULL, 0, NULL, 0);
    if (result != 0) {
        brls::Logger::error("[VitaSession] LiStartConnection fallo: {}", result);
        LiStopConnection();
        return false;
    }
    return true;
}

void VitaSession::stop(bool terminateApp) {
    if (terminateApp) {
        GameStreamClient::instance().quitApp(m_address);
    }
    LiStopConnection();
    m_is_active = false;
    m_is_terminated = true;
}

bool VitaSession::attemptReconnect() {
    if (m_reconnect_attempts >= m_reconnect_limit) return false;
    m_reconnect_attempts++;
    brls::Logger::info("[VitaSession] Intento de reconexión {}", m_reconnect_attempts);
    // Reiniciar conexión sin recrear config
    return internalStart();
}

// ==== Static callbacks ====
void VitaSession::connection_stage_starting(int stage) {
    brls::Logger::info("[VitaSession] stage starting {}", stage);
}
void VitaSession::connection_stage_complete(int stage) {
    brls::Logger::info("[VitaSession] stage complete {}", stage);
}
void VitaSession::connection_stage_failed(int stage, int error_code) {
    brls::Logger::error("[VitaSession] stage failed {} code {}", stage, error_code);
}
void VitaSession::connection_started() {
    if (s_active) s_active->m_is_active = true;
    brls::Logger::info("[VitaSession] connection started");
}
void VitaSession::connection_terminated(int error_code) {
    brls::Logger::info("[VitaSession] connection terminated code {}", error_code);
    if (!s_active) return;
    if (error_code != 0) {
        bool ok = s_active->attemptReconnect();
        if (!ok) {
            s_active->m_is_active = false;
            s_active->m_is_terminated = true;
            brls::Application::notify("Conexión terminada");
        }
    } else {
        s_active->m_is_active = false;
        s_active->m_is_terminated = true;
    }
}
void VitaSession::connection_log_message(const char* format, ...) {
    va_list args; va_start(args, format);
    char buf[512]; vsnprintf(buf, sizeof(buf), format, args); va_end(args);
    brls::Logger::info(std::string("[VitaSession] ")+buf);
}
void VitaSession::connection_status_update(int status) {
    if (s_active) s_active->m_poor = (status == CONN_STATUS_POOR);
}

// Stubs de video eliminados: se utiliza implementación real en video/vita.cpp

// Audio stubs
int VitaSession::audio_renderer_init(int cfg, const POPUS_MULTISTREAM_CONFIGURATION, void*, int flags) {
    brls::Logger::info("[VitaSession] audio init cfg={} flags={}", cfg, flags);
    // FIXME: usar código de retorno correcto cuando integremos renderer real
    return 0; // equivalente a éxito
}
void VitaSession::audio_renderer_start() { brls::Logger::info("[VitaSession] audio start"); }
void VitaSession::audio_renderer_stop() { brls::Logger::info("[VitaSession] audio stop"); }
void VitaSession::audio_renderer_cleanup() { brls::Logger::info("[VitaSession] audio cleanup"); }
void VitaSession::audio_renderer_decode_and_play_sample(char*, int) {}

uint64_t VitaSession::monotonicMs() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull;
}
