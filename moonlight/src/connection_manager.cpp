#include "connection_manager.hpp"
#include "ConfigManager.hpp"
#include <vector>
#include <string>
#include <borealis.hpp>
#include "Data.hpp"
#undef BUTTON_RIGHT
#undef BUTTON_START
#include "client.h"

// Implementación dummy para permitir la compilación
std::vector<RemoteAppInfo> ConnectionManager::fetchRemoteApps(const HostInfo& host) {
    // Inspirado en Moonlight-Switch: conectar y luego pedir la lista de apps
    std::vector<RemoteAppInfo> result;


    // 1. Conectar al host (inicializa la sesión, emparejamiento, etc)
    SERVER_DATA serverData;
    std::string address = host.ip;
    // Construir keyDir igual que en pairing: baseDir + "/" + host.name
    ConfigManager config;
    std::string baseDir = config.getKeysDir();
    std::string safeHostName = host.name;
    for (char& c : safeHostName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    std::string keyDir = baseDir + "/" + safeHostName;

    // 1. Inicializar sesión y obtener serverinfo para el puerto correcto
    int status = gs_init(&serverData, address, keyDir);
    if (status != 0) {
        brls::Logger::error("[ConnectionManager] Error al conectar con el host: %s", host.ip.c_str());
        return result;
    }

    // --- Obtener el puerto HTTPS real del serverinfo (como Moonlight-Switch) ---
    // Si tu gs_init ya hace esto, puedes omitir, pero aquí lo forzamos:
    extern "C" int load_serverinfo(SERVER_DATA* server, bool https); // Declaración C si no está en el header
    int infoStatus = load_serverinfo(&serverData, true);
    if (infoStatus != 0) {
        brls::Logger::error("[ConnectionManager] Error al obtener serverinfo (puerto HTTPS): %s", host.ip.c_str());
        return result;
    }
    // El puerto correcto queda en serverData.httpsPort

    // 2. Obtener la lista de apps remotas usando el puerto correcto
    PAPP_LIST list = nullptr;
    int appStatus = gs_applist(&serverData, &list);
    if (appStatus != 0) {
        brls::Logger::error("[ConnectionManager] Error al obtener la lista de apps: %s", host.ip.c_str());
        return result;
    }

    while (list) {
        RemoteAppInfo info;
        info.id = std::to_string(list->id);
        info.name = list->name;
        info.iconUrl = ""; // Si tienes iconos, asígnalos aquí
        result.push_back(info);
        list = list->next;
    }

    // Ordenar por nombre (opcional)
    std::sort(result.begin(), result.end(), [](const RemoteAppInfo& a, const RemoteAppInfo& b) {
        return a.name < b.name;
    });

    return result;
}

bool ConnectionManager::startConnection(const HostInfo& host, const RemoteAppInfo& app) {
    // TODO: Implementar lógica real de conexión
    return false;
}

void ConnectionManager::selectAndConnect(const HostInfo& host, std::function<void(bool)> onResult) {
    // TODO: Implementar lógica real de selección y conexión
    if (onResult) onResult(false);
}
