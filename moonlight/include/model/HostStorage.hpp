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
    int port = 47989; // Sunshine Default Port
    bool paired = false;
    // Host MAC address (optional, can be empty)
    std::string mac;
    // Secure identifier (folder) derived from name or IP; used as keyDir
    std::string safeId;
    // Microphone port (per device, saved in device.ini)
    int microphone_port = 48100; // MoonMic default port
};

// Generate a "safe" identifier to use as a folder name
std::string makeSafeHostId(const std::string& raw);

class HostStorage {
public:
    // Load all hosts by reading device.ini from each folder
    static std::vector<HostInfo> loadHosts();
    // Not implemented (not used with device.ini)
    static bool saveHosts(const std::vector<HostInfo>& hosts);
    // Add a host (create device.ini if ​​it doesn't exist)
    static bool addHost(const HostInfo& host);
    // Search for a host by name
    static std::optional<HostInfo> findHost(const std::string& name);
    // Delete a host by name (delete folder)
    static bool removeHost(const std::string& name);
    // Update the IP address of an existing host
    static bool updateHostIp(const std::string& name, const std::string& newIp);
    // Save a host after successful pairing (create device.ini)
    // mac: optional value with the MAC address of the host (ex: "AA:BB:CC:DD:EE:FF").
    static bool savePairedHost(const std::string& name, const std::string& ip, int port, bool paired, const std::string& mac = "");
    // Generate device.ini file in host folder (call after successful pairing)
    // mac: optional pointer; if nullptr or empty string, the mac= line is not written
    static bool writeDeviceIni(const std::string& hostDir, const std::string& safeHostName, const char* address, int port, bool paired, const char* mac = nullptr);
};