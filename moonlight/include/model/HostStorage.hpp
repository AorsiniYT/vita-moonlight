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
#pragma once
#include <string>
#include <vector>
#include <optional>

struct HostInfo {
    std::string name;
    std::string ip;
    int port = 47989; // Puerto por defecto Sunshine
    bool paired = false;
    // Dirección MAC del host (opcional, puede estar vacía)
    std::string mac;
    // Identificador seguro (carpeta) derivado del nombre o IP; se usa como keyDir
    std::string safeId;
    // Puerto del micrófono (por dispositivo, guardado en device.ini)
    int microphone_port = 48100; // Puerto por defecto MoonMic
};

// Genera un identificador "seguro" para usar como nombre de carpeta
std::string makeSafeHostId(const std::string& raw);

class HostStorage {
public:
    // Carga todos los hosts leyendo device.ini de cada carpeta
    static std::vector<HostInfo> loadHosts();
    // No implementado (no se usa con device.ini)
    static bool saveHosts(const std::vector<HostInfo>& hosts);
    // Añade un host (crea device.ini si no existe)
    static bool addHost(const HostInfo& host);
    // Busca un host por nombre
    static std::optional<HostInfo> findHost(const std::string& name);
    // Elimina un host por nombre (borra la carpeta)
    static bool removeHost(const std::string& name);
    // Actualiza la dirección IP de un host existente
    static bool updateHostIp(const std::string& name, const std::string& newIp);
    // Guarda un host tras pairing exitoso (crea device.ini)
    // mac: valor opcional con la dirección MAC del host (ej: "AA:BB:CC:DD:EE:FF").
    static bool savePairedHost(const std::string& name, const std::string& ip, int port, bool paired, const std::string& mac = "");
    // Genera el archivo device.ini en la carpeta del host (llamar tras pairing exitoso)
    // mac: puntero opcional; si es nullptr o cadena vacía, no se escribe la línea mac=
    static bool writeDeviceIni(const std::string& hostDir, const std::string& safeHostName, const char* address, int port, bool paired, const char* mac = nullptr);
};