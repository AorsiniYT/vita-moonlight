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
#include "ConfigManager.hpp"
#include <ini.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// Definición fuera de clase para static std::string ConfigManager::getConfigPath();
#ifdef _WIN32
    #include <windows.h>
#endif



std::string ConfigManager::getConfigPath() {
#ifdef _WIN32
    // En la misma carpeta que el exe
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) path = path.substr(0, pos+1);
    return path + "moonlight-vita.conf";
#elif defined(__PSV__) || defined(__PSV__)
    return "ux0:data/moonlight/moonlight-vita.conf";
#else
    return "moonlight-vita.conf";
#endif
}

static int iniHandler(void* user, const char* section, const char* name, const char* value) {
    auto* config = reinterpret_cast<ConfigManager*>(user);
    config->set(section, name, value);
    return 1;
}



ConfigManager::ConfigManager() {}


// Obtiene el directorio de llaves/dispositivos, configurable por archivo o por código
std::string ConfigManager::getKeysDir() const {
    if (keysDirLoaded) return cachedKeysDir;
    // 1. Intentar leer de la config
    std::string dir = get("general", "keys_dir", "");
    if (!dir.empty()) {
        cachedKeysDir = dir;
        keysDirLoaded = true;
        return cachedKeysDir;
    }
    // 2. Si no está en config, usar valor multiplataforma por defecto
#if defined(__PSV__) || defined(__PSV__)
    cachedKeysDir = "ux0:data/moonlight/devices";
#elif defined(_WIN32)
    cachedKeysDir = "moonlight/devices";
#else
    cachedKeysDir = "moonlight/devices";
#endif
    keysDirLoaded = true;
    return cachedKeysDir;
}

void ConfigManager::setKeysDir(const std::string& dir) {
    set("general", "keys_dir", dir);
    cachedKeysDir = dir;
    keysDirLoaded = true;
}

bool ConfigManager::load() {
    data.clear();
    std::string path = getConfigPath();
    std::cout << "[ConfigManager] Cargando configuración desde: " << path << std::endl;
    int result = ini_parse(path.c_str(), iniHandler, this);
    if (result != 0) {
        std::cout << "[ConfigManager] Error al cargar el archivo o no existe." << std::endl;
        return false;
    }
    std::cout << "[ConfigManager] Configuración cargada:" << std::endl;
    for (const auto& sec : data) {
        std::cout << "[" << sec.first << "]" << std::endl;
        for (const auto& kv : sec.second) {
            std::cout << kv.first << "=" << kv.second << std::endl;
        }
    }
    return true;
}

bool ConfigManager::save() const {
    std::string path = getConfigPath();
    std::cout << "[ConfigManager] Guardando configuración en: " << path << std::endl;
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cout << "[ConfigManager] No se pudo abrir el archivo para guardar." << std::endl;
        return false;
    }
    for (const auto& sec : data) {
        file << "[" << sec.first << "]\n";
        for (const auto& kv : sec.second) {
            file << kv.first << "=" << kv.second << "\n";
        }
        file << "\n";
    }
    std::cout << "[ConfigManager] Configuración guardada:" << std::endl;
    for (const auto& sec : data) {
        std::cout << "[" << sec.first << "]" << std::endl;
        for (const auto& kv : sec.second) {
            std::cout << kv.first << "=" << kv.second << std::endl;
        }
    }
    return true;
}

std::string ConfigManager::get(const std::string& section, const std::string& key, const std::string& def) const {
    auto it = data.find(section);
    if (it != data.end()) {
        auto it2 = it->second.find(key);
        if (it2 != it->second.end())
            return it2->second;
    }
    return def;
}

void ConfigManager::set(const std::string& section, const std::string& key, const std::string& value) {
    data[section][key] = value;
}
