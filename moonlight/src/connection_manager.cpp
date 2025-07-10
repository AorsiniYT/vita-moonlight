#include "connection_manager.hpp"
#include <vector>
#include <string>

// Implementación dummy para permitir la compilación
std::vector<RemoteAppInfo> ConnectionManager::fetchRemoteApps(const HostInfo& host) {
    // TODO: Implementar lógica real de consulta de apps remotas
    return {};
}

bool ConnectionManager::startConnection(const HostInfo& host, const RemoteAppInfo& app) {
    // TODO: Implementar lógica real de conexión
    return false;
}

void ConnectionManager::selectAndConnect(const HostInfo& host, std::function<void(bool)> onResult) {
    // TODO: Implementar lógica real de selección y conexión
    if (onResult) onResult(false);
}
