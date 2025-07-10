/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#include "model/HostStorage.hpp"
#include "ConfigManager.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <filesystem>
#include <iostream>



namespace fs = std::filesystem;

// Genera el archivo device.ini en la carpeta del host
bool HostStorage::writeDeviceIni(const std::string& hostDir, const std::string& safeHostName, const char* address, int port, bool paired) {
    std::string deviceIniPath = hostDir + "/device.ini";
    FILE* f = fopen(deviceIniPath.c_str(), "w");
    if (!f) return false;

    fprintf(f, "[Device]\n");
    // uuid: contenido de uniqueid.dat (hex, sin espacios ni saltos)
    std::string uniqueidPath = hostDir + "/uniqueid.dat";
    FILE* uf = fopen(uniqueidPath.c_str(), "rb");
    if (uf) {
        unsigned char buf[32];
        size_t n = fread(buf, 1, sizeof(buf), uf);
        fclose(uf);
        fprintf(f, "uuid=");
        for (size_t i = 0; i < n; ++i) fprintf(f, "%02x", buf[i]);
        fprintf(f, "\n");
    } else {
        fprintf(f, "uuid=\n");
    }
    // name: nombre del dispositivo (hostName local, o algo identificable)
    fprintf(f, "name=%s\n", safeHostName.c_str());
    // tipo de dispositivo
    fprintf(f, "type=psvita\n");
    // paired
    fprintf(f, "paired=%s\n", paired ? "true" : "false");
    // internal: IP del host
    fprintf(f, "internal=%s\n", address);
    // external: dejar vacío por ahora
    fprintf(f, "external=\n");
    // port: puerto externo usado
    fprintf(f, "port=%d\n", port);
    // prefer_external: por defecto false
    fprintf(f, "prefer_external=false\n");
    fclose(f);
    return true;
}
// Guarda un host tras pairing exitoso
bool HostStorage::savePairedHost(const std::string& name, const std::string& ip, int port, bool paired) {
    HostInfo host;
    host.name = name;
    host.ip = ip;
    host.port = port;
    host.paired = paired;
    return HostStorage::addHost(host);
}




// Carga todos los hosts escaneando las carpetas y leyendo device.ini
std::vector<HostInfo> HostStorage::loadHosts() {
    std::vector<HostInfo> hosts;
    ConfigManager config;
    std::string baseDir = config.getKeysDir();
    std::cout << "[DEBUG][HostStorage] baseDir usado para hosts: '" << baseDir << "'\n";
    DIR* dir = opendir(baseDir.c_str());
    if (!dir) {
        std::cout << "[DEBUG][HostStorage] No se pudo abrir el directorio: '" << baseDir << "'\n";
        return hosts;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string folder = entry->d_name;
        std::cout << "[DEBUG][HostStorage] Carpeta encontrada: '" << folder << "'\n";
        if (folder.empty() || folder == "." || folder == ".." || folder.length() > 60)
            continue;
        std::string path = baseDir + "/" + folder;
        struct stat st;
        if (::stat(path.c_str(), &st) != 0 || !(st.st_mode & S_IFDIR))
            continue;
        std::string iniPath = path + "/device.ini";
        std::ifstream ini(iniPath);
        if (!ini.is_open()) {
            std::cout << "[DEBUG][HostStorage] No se pudo abrir: '" << iniPath << "'\n";
            continue;
        }
        HostInfo host;
        host.name = folder;
        std::cout << "[DEBUG][HostStorage] Leyendo device.ini para host: '" << folder << "'\n";
        std::string line;
        while (std::getline(ini, line)) {
            std::cout << "[DEBUG][HostStorage] device.ini: " << line << "\n";
            if (line.find("internal=") == 0) {
                host.ip = line.substr(strlen("internal="));
            } else if (line.find("port=") == 0) {
                host.port = std::stoi(line.substr(strlen("port=")));
            } else if (line.find("paired=") == 0) {
                std::string val = line.substr(strlen("paired="));
                host.paired = (val == "true");
            }
        }
        hosts.push_back(host);
    }
    closedir(dir);
    std::cout << "[DEBUG][HostStorage] Total hosts encontrados: " << hosts.size() << "\n";
    return hosts;
}

// saveHosts: no implementado, no se usa con device.ini
bool HostStorage::saveHosts(const std::vector<HostInfo>&) { return false; }


// addHost: usa la ruta configurable de dispositivos
bool HostStorage::addHost(const HostInfo& host) {
    ConfigManager config;
    std::string baseDir = config.getKeysDir();
    std::string keyDirStr = baseDir + "/" + host.name;
    std::string deviceIniPath = keyDirStr + "/device.ini";
    if (fs::exists(deviceIniPath)) return false;
    fs::create_directories(keyDirStr);
    return HostStorage::writeDeviceIni(keyDirStr, host.name, host.ip.c_str(), host.port, host.paired);
}

std::optional<HostInfo> HostStorage::findHost(const std::string& name) {
    auto hosts = loadHosts();
    for (const auto& h : hosts) {
        if (h.name == name) return h;
    }
    return std::nullopt;
}

bool HostStorage::removeHost(const std::string& name) {
    ConfigManager config;
    std::string baseDir = config.getKeysDir();
    std::string keyDirStr = baseDir + "/" + name;
    if (!fs::exists(keyDirStr)) return false;
    return fs::remove_all(keyDirStr) > 0;
}
