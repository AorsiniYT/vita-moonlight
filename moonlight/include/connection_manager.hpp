#pragma once
#include "model/HostStorage.hpp"
#include <vector>
#include <string>
#include <functional>

struct RemoteAppInfo {
    std::string id;
    std::string name;
    std::string iconUrl;
};

class ConnectionManager {
public:
    // Obtiene la lista de apps disponibles en el host
    static std::vector<RemoteAppInfo> fetchRemoteApps(const HostInfo& host);
    // Inicia la conexión a una app específica
    static bool startConnection(const HostInfo& host, const RemoteAppInfo& app);
    // Flujo completo: selecciona app y conecta (callback para UI)
    static void selectAndConnect(const HostInfo& host, std::function<void(bool)> onResult);
};
