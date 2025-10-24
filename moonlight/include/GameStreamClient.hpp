/*
    GameStreamClient.hpp - Cliente para manejar conexiones GameStream
    Patrón basado en Moonlight-Switch para PS Vita
    Autor: aorsini + comunidad
*/
#ifndef GAMESTREAM_CLIENT_HPP
#define GAMESTREAM_CLIENT_HPP

// Incluir Borealis primero para evitar conflictos con constantes BUTTON_*
#include "borealis.hpp"

#include <map>
#include <string>
#include <functional>
#include <vector>

// Headers de Limelight después de Borealis
#include "client.h"
#include "errors.h"
#include "Limelight.h"
#include "connection_manager.hpp" // Para RemoteAppInfo

struct HostInfo; // forward

typedef std::function<void(const std::vector<RemoteAppInfo>&)> AppListCallback;
typedef std::function<void(bool)> BoolCallback;

class GameStreamClient {
public:
    static GameStreamClient& instance();

    // Inicialización del servidor
    bool connect(const std::string& address);
    bool connect(const HostInfo& host); // usa safeId consistente
    bool isConnected(const std::string& address);

    // Obtener datos del servidor
    SERVER_DATA& serverData(const std::string& address);

    // Lanzar aplicación
    bool startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId);
    // Recuperar última configuración usada (incluye remoteInputAesKey/IV si ya se generó)
    bool lastStreamConfig(const std::string& address, STREAM_CONFIGURATION& out) const;

    // Pairing
    bool pair(const std::string& address, const std::string& pin);
    bool unpair(const std::string& address);
    bool isPaired(const std::string& address);
    // Flujo completo de pairing con popup (PIN auto-generado) replicando comportamiento original
    bool beginPairing(const HostInfo& host, std::function<void(bool)> onFinished, std::function<void(const std::string&)> onPinReady = nullptr);

    // Obtener lista de aplicaciones
    void getAppList(const std::string& address, AppListCallback callback);

    // Terminar aplicación
    bool quitApp(const std::string& address);

    // Gestión de sesión activa
    void setActiveStream(const std::string& address, int appId, const std::string& appName);
    void clearActiveStream(const std::string& address);
    bool hasActiveStream(const std::string& address) const;
    RemoteAppInfo activeAppInfo(const std::string& address) const; // devuelve appName/icon genérico si existe

private:
    GameStreamClient();
    ~GameStreamClient();

    std::map<std::string, SERVER_DATA> m_server_data;
    std::map<std::string, STREAM_CONFIGURATION> m_last_stream_cfg; // address -> última config lanzada
    std::map<std::string, std::vector<RemoteAppInfo>> m_app_lists;
    struct ActiveStream {
        int appId;
        std::string appName;
    };
    std::map<std::string, ActiveStream> m_active_streams; // address -> active stream
};

#endif // GAMESTREAM_CLIENT_HPP