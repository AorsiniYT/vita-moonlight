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
#include "utils/host_search.hpp"
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <borealis/core/logger.hpp>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#endif

#ifdef __PSV__
#include <psp2/kernel/clib.h>
#define VITALOG sceClibPrintf
#else
#define VITALOG(...) ((void)0)
#endif

#ifdef __PSV__
// Implementación Vita: loadHostsVita() con struct HostInfoVita (char[])
std::vector<HostInfoVita> loadHostsVita() {
    std::vector<HostInfoVita> hosts;
    VITALOG("[loadHostsVita] Dirección de hosts vector: %p\n", (void*)&hosts);
    std::string baseDir = "ux0:data/moonlight/devices/";
    DIR* dir = opendir(baseDir.c_str());
    if (!dir) return hosts;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        char folder[64] = {0};
        strncpy(folder, entry->d_name, sizeof(folder)-1);
        VITALOG("[loadHostsVita] Leyendo carpeta: '%s', longitud: %d\n", folder, (int)strlen(folder));
        if (strlen(folder) == 0 || strcmp(folder, ".") == 0 || strcmp(folder, "..") == 0 || strlen(folder) > 60) {
            VITALOG("[loadHostsVita] Carpeta ignorada: '%s'\n", folder);
            continue;
        }
        std::string path = baseDir + folder;
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            VITALOG("[loadHostsVita] '%s' no es un directorio válido\n", path.c_str());
            continue;
        }
        std::string iniPath = path + "/device.ini";
        std::ifstream ini(iniPath);
        if (!ini.is_open()) {
            VITALOG("[loadHostsVita] No se pudo abrir INI: %s\n", iniPath.c_str());
            continue;
        }
        HostInfoVita host = {0};
        strncpy(host.name, folder, sizeof(host.name)-1);
        std::string line;
        while (std::getline(ini, line)) {
            VITALOG("[loadHostsVita] Línea INI: '%s'\n", line.c_str());
            if (line.find("internal = ") == 0) {
                size_t prefixLen = strlen("internal = ");
                std::string ip = line.substr(prefixLen);
                strncpy(host.ip, ip.c_str(), sizeof(host.ip)-1);
            } else if (line.find("prefer_external = ") == 0) {
                size_t prefixLen = strlen("prefer_external = ");
                std::string val = line.substr(prefixLen);
                host.preferExternal = (val == "true");
            }
        }
        VITALOG("[loadHostsVita] Host cargado: name='%s', ip='%s', preferExternal=%d\n", host.name, host.ip, host.preferExternal);
        VITALOG("[loadHostsVita] push_back: dirección hosts=%p, size=%lu, último host='%s'\n", (void*)&hosts, (unsigned long)hosts.size(), host.name);
        hosts.push_back(host);
        VITALOG("[loadHostsVita] tras push_back: dirección hosts=%p, size=%lu\n", (void*)&hosts, (unsigned long)hosts.size());
    }
    unsigned long safeSize = (unsigned long)hosts.size();
    if (safeSize > 1000) {
        VITALOG("[loadHostsVita] ERROR: Tamaño de hosts.size() corrupto: %lu\n", safeSize);
        return std::vector<HostInfoVita>();
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "[loadHostsVita] Total hosts cargados: %lu", safeSize);
    VITALOG("%s\n", buf);
    closedir(dir);
    return hosts;
}
#else
// Implementación multiplataforma (no Vita): loadHosts() con std::string
std::vector<SearchHostInfo> loadHosts() {
    std::vector<SearchHostInfo> hosts;
    std::string baseDir = "devices/";
    DIR* dir = opendir(baseDir.c_str());
    if (!dir) return hosts;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string folder = entry->d_name;
        if (folder.empty() || folder == "." || folder == ".." || folder.length() > 60)
            continue;
        std::string path = baseDir + folder;
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        std::string iniPath = path + "/device.ini";
        std::ifstream ini(iniPath);
        if (!ini.is_open())
            continue;
        SearchHostInfo host;
        host.name = folder;
        std::string line;
        while (std::getline(ini, line)) {
            if (line.find("internal = ") == 0) {
                size_t prefixLen = strlen("internal = ");
                host.ip = line.substr(prefixLen);
            } else if (line.find("prefer_external = ") == 0) {
                size_t prefixLen = strlen("prefer_external = ");
                std::string val = line.substr(prefixLen);
                host.preferExternal = (val == "true");
            }
        }
        hosts.push_back(host);
    }
    return hosts;
}
#endif



