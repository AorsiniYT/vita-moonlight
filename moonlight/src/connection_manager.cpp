/*
    connection_manager.cpp - Lógica de conexión y selección de aplicación para Moonlight PSVita/Windows
    Autor: aorsini + comunidad
*/
#include "connection_manager.hpp"
#include "model/HostStorage.hpp"
#include "ConfigManager.hpp"
#include <vector>
#include <string>
#include <functional>
#include <borealis/core/logger.hpp>
#include <sys/stat.h>



#include <cstring>
extern "C" {
#include "libgamestream/client.h"
#include "libgamestream/errors.h"
}
#include <memory>

std::vector<RemoteAppInfo> ConnectionManager::fetchRemoteApps(const HostInfo& host) {
    static ConfigManager config;
    std::string keysBaseDir = config.getKeysDir();
    brls::Logger::info("[fetchRemoteApps] Usando keysDir base: {}", keysBaseDir);
    std::vector<RemoteAppInfo> apps;
    SERVER_DATA server = {};
    // Determinar key_dir según la estructura de carpetas (keysBaseDir/<host.name>)
    // Forzar ruta absoluta PSVita (corrige ux0:data/ -> /ux0/data/)
    // Normalizar ruta PSVita: ux0:/data/moonlight/devices/...
    std::string keyDir = keysBaseDir;
    if (keyDir.rfind("ux0:", 0) == 0) {
        // Ya tiene ux0: al inicio
        if (keyDir.length() == 4 || keyDir[4] != '/')
            keyDir.insert(4, "/"); // ux0: -> ux0:/
    } else if (keyDir.rfind("ux0", 0) == 0) {
        // Empieza con ux0 pero sin :
        keyDir.insert(3, ":/"); // ux0 -> ux0:/
    }
    keyDir += "/" + host.name;
    // Comprobar existencia de archivos de pairing
    std::string certPath = keyDir + "/client.p12";
    std::string uniqueidPath = keyDir + "/uniqueid.dat";
    struct stat st_cert, st_uid;
    bool cert_ok = (stat(certPath.c_str(), &st_cert) == 0);
    bool uid_ok = (stat(uniqueidPath.c_str(), &st_uid) == 0);
    brls::Logger::info("[fetchRemoteApps] ¿Existe {}? {} | ¿Existe {}? {}", certPath, cert_ok, uniqueidPath, uid_ok);
    if (!cert_ok || !uid_ok) {
        brls::Logger::error("[fetchRemoteApps] Faltan archivos de pairing para el host: {} (ip: {})", host.name, host.ip);
        return apps;
    }
    // Depuración: imprimir valores antes de gs_init
    brls::Logger::info("[fetchRemoteApps] host.ip='{}' host.port={} host.name='{}'", host.ip, host.port, host.name);
    brls::Logger::info("[fetchRemoteApps] keyDir='{}'", keyDir);
    brls::Logger::info("[fetchRemoteApps] server.serverInfo.address='{}' server.httpsPort={}", host.ip, host.port);
    // Inicializar el servidor con gs_init, pasando la IP y el puerto correctos
    int ret_init = gs_init(&server, (char*)host.ip.c_str(), host.port, keyDir.c_str(), 0, false);
    if (ret_init != GS_OK) {
        const char* gs_err = gs_error;
        brls::Logger::error("[fetchRemoteApps] gs_init falló para host: {} (ip: {}) | gs_error: {}", host.name, host.ip, gs_err ? gs_err : "(null)");
        if (server.serverInfo.address) free((void*)server.serverInfo.address);
        return apps;
    }

    PAPP_LIST app_list = nullptr;
    int ret = gs_applist(&server, &app_list);
    const char* gs_err = gs_error; // variable global de client.c
    brls::Logger::info("[fetchRemoteApps] gs_applist retornó {}", ret);
    if (ret == 0 && app_list) {
        for (PAPP_LIST app = app_list; app != nullptr; app = app->next) {
            RemoteAppInfo info;
            info.id = std::to_string(app->id);
            info.name = app->name ? app->name : "";
            info.iconUrl = "";
            brls::Logger::info("[fetchRemoteApps] App encontrada: id='{}', name='{}'", info.id, info.name);
            apps.push_back(info);
        }
    } else {
        brls::Logger::error("[fetchRemoteApps] Error al obtener la lista de apps del host: {} (ip: {}) | gs_error: {}", host.name, host.ip, gs_err ? gs_err : "(null)");
    }
    // Liberar memoria de la lista C correctamente
    PAPP_LIST cur = app_list;
    while (cur) {
        PAPP_LIST next = cur->next;
        if (cur->name) free(cur->name);
        free(cur);
        cur = next;
    }
    if (server.serverInfo.address) free((void*)server.serverInfo.address);
    brls::Logger::info("[fetchRemoteApps] Total apps devueltas: {}", apps.size());
    return apps;
}

// Lógica para iniciar la conexión a una app específica
bool ConnectionManager::startConnection(const HostInfo& host, const RemoteAppInfo& app) {
    // Inicializar estructura SERVER_DATA
    SERVER_DATA server = {};
    server.serverInfo.address = strdup(host.ip.c_str());
    server.httpsPort = host.port;
    // Determinar key_dir según la estructura de carpetas (devices/<host.name>)
    std::string keyDir = "devices/" + host.name;
    // gs_init espera: (PSERVER_DATA, char* address, unsigned short httpPort, const char* keyDirectory, int logLevel, bool unsupported)
    int ret = gs_init(&server, nullptr, 0, keyDir.c_str(), 0, false);
    if (ret != GS_OK) {
        if (server.serverInfo.address) free((void*)server.serverInfo.address);
        return false;
    }

    // Iniciar la app remota (app.id debe ser int)
    int appId = 0;
    try {
        appId = std::stoi(app.id);
    } catch (...) {
        if (server.serverInfo.address) free((void*)server.serverInfo.address);
        return false;
    }
    ret = gs_start_app(&server, nullptr, appId, false, false, 0);
    if (ret != GS_OK) {
        if (server.serverInfo.address) free((void*)server.serverInfo.address);
        return false;
    }

    // Aquí deberías iniciar la conexión de streaming real (LiStartConnection o equivalente)
    // Por ahora, solo simulamos éxito si llegamos aquí

    // Limpieza
    if (server.serverInfo.address) free((void*)server.serverInfo.address);
    return true;
}

// Callback para UI: mostrar lista de apps y devolver la elegida
void ConnectionManager::selectAndConnect(const HostInfo& host, std::function<void(bool)> onResult) {
    auto apps = fetchRemoteApps(host);
    // Aquí deberías mostrar un diálogo/lista de selección de apps al usuario
    // Por simplicidad, seleccionamos la primera
    if (!apps.empty()) {
        bool ok = startConnection(host, apps[0]);
        onResult(ok);
    } else {
        onResult(false);
    }
}
