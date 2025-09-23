/*
    GameStreamClient.cpp - Implementación del cliente GameStream
    Patrón basado en Moonlight-Switch para PS Vita
    Autor: aorsini + comunidad
*/
#include "GameStreamClient.hpp"
#include <cstring>
#include "ConfigManager.hpp"
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

GameStreamClient& GameStreamClient::instance() {
    static GameStreamClient instance;
    return instance;
}

GameStreamClient::GameStreamClient() {
    // Constructor vacío
}

GameStreamClient::~GameStreamClient() {
    // Limpiar datos de servidores
    m_server_data.clear();
    m_app_lists.clear();
}

bool GameStreamClient::connect(const std::string& address) {
    HostInfo h; h.ip = address; h.name = address; h.safeId = makeSafeHostId(address);
    return connect(h);
}

bool GameStreamClient::connect(const HostInfo& host) {
    if (host.ip.empty()) {
        brls::Logger::error("[GameStreamClient] Dirección vacía en connect(host)");
        return false;
    }
    std::string addr = host.ip;
    if (m_server_data.count(addr) > 0) {
        brls::Logger::info("[GameStreamClient] Ya conectado a {}", addr);
        return true;
    }
    SERVER_DATA serverData{};
    ConfigManager cfg; cfg.load();
    std::string baseDevices = cfg.getKeysDir();
    std::string safe = host.safeId.empty()? makeSafeHostId(host.name.empty()? host.ip : host.name) : host.safeId;
    // Resolver posible keyDir existente (migración de versiones anteriores)
    auto resolveExistingKeyDir = [&](const std::string& base, const HostInfo& h)->std::string {
        std::vector<std::string> candidates;
        // 1. Nuevo esquema (safeId derivado del nombre)
        candidates.push_back(base + "/" + safe);
        // 2. Nombre sin dominio (antes quizá se guardaba sin .local)
        if (!h.name.empty()) {
            auto pos = h.name.find('.');
            if (pos != std::string::npos) {
                std::string shortName = h.name.substr(0, pos);
                candidates.push_back(base + "/" + makeSafeHostId(shortName));
            }
        }
        // 3. IP cruda
        if (!h.ip.empty()) candidates.push_back(base + "/" + h.ip);
        // 4. safeId generado de la IP
        if (!h.ip.empty()) candidates.push_back(base + "/" + makeSafeHostId(h.ip));
        // El primero que contenga device.ini lo usamos
        for (auto& c : candidates) {
            struct stat st{}; if (stat(c.c_str(), &st)==0 && S_ISDIR(st.st_mode)) {
                std::string devIni = c + "/device.ini";
                if (stat(devIni.c_str(), &st)==0) {
                    brls::Logger::info("[GameStreamClient][keyDir-resolver] Reutilizando keyDir existente '{}' (device.ini encontrado)", c);
                    return c;
                }
            }
        }
        // Si ninguno tiene device.ini, devolvemos el nuevo esquema
        return candidates.front();
    };
    std::string keyDir = resolveExistingKeyDir(baseDevices, host);
    brls::Logger::info("[GameStreamClient] keyDir elegido para '{}' => '{}' (safeId base='{}')", addr, keyDir, safe);
    // Diagnóstico keyDir
    {
        struct stat st{};
        if (stat(keyDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            brls::Logger::info("[GameStreamClient][keyDir] EXISTE '{}'", keyDir);
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                brls::Logger::info("[GameStreamClient][keyDir] Archivos: {}", files.empty()? "(vacío)" : files);
            } else {
                brls::Logger::error("[GameStreamClient][keyDir] No se pudo abrir directorio '{}'", keyDir);
            }
        } else {
            brls::Logger::warning("[GameStreamClient][keyDir] NO EXISTE '{}' antes de gs_init", keyDir);
        }
    }
    int status = gs_init(&serverData, addr, keyDir);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] Error al inicializar servidor: {}", status);
        return false;
    }
    m_server_data[addr] = serverData;
    brls::Logger::info("[GameStreamClient] Conexión exitosa a {}", addr);
    const char* srv_addr = serverData.serverInfo.address ? serverData.serverInfo.address : "NULL";
    brls::Logger::info("[GameStreamClient] ServerInfo address: {}", srv_addr);
    return true;
}

bool GameStreamClient::isConnected(const std::string& address) {
    return m_server_data.count(address) > 0;
}

SERVER_DATA& GameStreamClient::serverData(const std::string& address) {
    if (m_server_data.count(address) == 0) {
        brls::Logger::error("[GameStreamClient] No hay datos para servidor {}", address);
        // Esto debería manejarse mejor, pero por ahora devolveremos una referencia inválida
        static SERVER_DATA empty;
        return empty;
    }
    return m_server_data[address];
}

bool GameStreamClient::startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId) {
    if (m_server_data.count(address) == 0) {
        brls::Logger::error("[GameStreamClient] No conectado a {}", address);
        return false;
    }

    brls::Logger::info("[GameStreamClient] Iniciando aplicación {} en {}", appId, address);

    int status = gs_start_app(&m_server_data[address], &config, appId, true, true, 0x1);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] Error al iniciar aplicación: {}", status);
        return false;
    }

    // Guardar copia de la configuración (incluye remoteInputAesKey rellenada por gs_start_app)
    m_last_stream_cfg[address] = config;
    brls::Logger::info("[GameStreamClient] Aplicación iniciada correctamente (remoteInputKey set)");
    return true;
}

bool GameStreamClient::lastStreamConfig(const std::string& address, STREAM_CONFIGURATION& out) const {
    auto it = m_last_stream_cfg.find(address);
    if (it == m_last_stream_cfg.end()) return false;
    out = it->second;
    return true;
}

bool GameStreamClient::pair(const std::string& address, const std::string& pin) {
    if (m_server_data.count(address) == 0) {
        brls::Logger::error("[GameStreamClient] pair: no conectado a {}", address);
        return false;
    }
    SERVER_DATA& s = m_server_data[address];
    if (s.paired) {
        brls::Logger::info("[GameStreamClient] pair: ya emparejado");
        return true;
    }
    char pinBuf[16];
    memset(pinBuf, 0, sizeof(pinBuf));
    strncpy(pinBuf, pin.c_str(), sizeof(pinBuf)-1);
    int status = gs_pair(&s, pinBuf);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] pair fallo status={}", status);
        return false;
    }
    brls::Logger::info("[GameStreamClient] pairing OK");
    return true;
}

bool GameStreamClient::unpair(const std::string& address) {
    if (m_server_data.count(address) == 0) return false;
    SERVER_DATA& s = m_server_data[address];
    int status = gs_unpair(&s);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] unpair fallo status={}", status);
        return false;
    }
    s.paired = false;
    return true;
}

bool GameStreamClient::isPaired(const std::string& address) {
    if (m_server_data.count(address) == 0) return false;
    return m_server_data[address].paired;
}

bool GameStreamClient::beginPairing(const HostInfo& host, std::function<void(bool)> onFinished) {
    HostInfo localHost = host;
    if (localHost.safeId.empty()) localHost.safeId = makeSafeHostId(localHost.name.empty()? localHost.ip : localHost.name);
    std::string addr = localHost.ip;
    if (addr.empty()) {
        brls::Logger::error("[GameStreamClient] beginPairing: host ip vacío");
        if (onFinished) onFinished(false); return false;
    }

    // Fast-path: si ya existe un device.ini en algún candidato, saltar pairing
    {
        ConfigManager cfg; cfg.load();
        std::string base = cfg.getKeysDir();
        std::vector<std::string> candidates;
        candidates.push_back(base + "/" + localHost.safeId);
        if (!localHost.name.empty()) {
            auto pos = localHost.name.find('.');
            if (pos != std::string::npos)
                candidates.push_back(base + "/" + makeSafeHostId(localHost.name.substr(0,pos)));
        }
        if (!localHost.ip.empty()) {
            candidates.push_back(base + "/" + localHost.ip);
            candidates.push_back(base + "/" + makeSafeHostId(localHost.ip));
        }
        struct stat st{};
        for (auto& c : candidates) {
            std::string di = c + "/device.ini";
            if (stat(di.c_str(), &st)==0) {
                brls::Logger::info("[Pairing][fast-path] device.ini existente en '{}' -> omitir pairing", c);
                if (onFinished) onFinished(true);
                return true;
            }
        }
    }

    // Crear diálogo (spinner + label) no cancelable como antes
    auto* holder = new brls::Box(brls::Axis::COLUMN);
    auto* label = new brls::Label();
    label->setText(brls::getStr("host_dialog/connecting"));
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setMarginBottom(18);
    auto* spinner = new brls::ProgressSpinner(brls::ProgressSpinnerSize::LARGE);
    spinner->View::setSize(brls::Size(92,92));
    holder->addView(label); holder->addView(spinner);
    holder->setAlignItems(brls::AlignItems::CENTER);
    holder->setJustifyContent(brls::JustifyContent::CENTER);
    holder->setPadding(28,28,28,28);
    auto* dialog = new brls::Dialog(holder);
    dialog->setCancelable(false);
    dialog->open();
    brls::Application::blockInputs();

    // Flags
    auto finished = std::make_shared<std::atomic<bool>>(false);
    auto cancelled = std::make_shared<std::atomic<bool>>(false); // por compatibilidad si en futuro se agrega cancelar

    // Hilo principal pairing
    std::thread([=]() mutable {
        auto t_start = std::chrono::high_resolution_clock::now();
        ConfigManager cfg; cfg.load();
        std::string base = cfg.getKeysDir();
        std::string keyDir = base + "/" + localHost.safeId;
        // Asegurar directorio (simplificado, sin borrar recursivamente)
        struct stat st{};
        // Resolución de keyDir reutilizando lógica de connect para evitar duplicado
        ConfigManager tmpCfg; tmpCfg.load();
        std::string baseDir = tmpCfg.getKeysDir();
        // Reusar mini-resolver simple: probar safeId, nombre sin dominio, ip
        std::vector<std::string> candidates;
        candidates.push_back(baseDir + "/" + localHost.safeId);
        if (!localHost.name.empty()) {
            auto pos = localHost.name.find('.');
            if (pos != std::string::npos) {
                candidates.push_back(baseDir + "/" + makeSafeHostId(localHost.name.substr(0,pos)));
            }
        }
        if (!localHost.ip.empty()) {
            candidates.push_back(baseDir + "/" + localHost.ip);
            candidates.push_back(baseDir + "/" + makeSafeHostId(localHost.ip));
        }
        // (fast-path ya evaluado antes de crear el diálogo)
        // Si no existe, procedemos flujo normal
        stat(keyDir.c_str(), &st); // refrescar st para la ruta principal
        if (stat(keyDir.c_str(), &st) != 0) {
            int mk = mkdir(keyDir.c_str(), 0777);
            if (mk != 0) brls::Logger::error("[Pairing] mkdir fallo en {} errno={}", keyDir, errno);
            else brls::Logger::info("[Pairing] keyDir creado {}", keyDir);
        } else {
            brls::Logger::info("[Pairing] keyDir ya existe {} (no se borra)" , keyDir);
        }
        // Listado inicial
        {
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                brls::Logger::info("[Pairing][keyDir] Inicial archivos: {}", files.empty()? "(vacío)" : files);
            }
        }

        SERVER_DATA serverData{};
        // gs_init (bloqueante aquí, podemos optimizar con hilo nativo Vita más adelante)
        int initRes = gs_init(&serverData, addr, keyDir);
        if (initRes != GS_OK) {
            brls::Logger::error("[Pairing] gs_init fallo {}", initRes);
            brls::sync([dialog, label]() { label->setText(brls::getStr("host_dialog/pairing_error_init")); });
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            *finished = true;
            if (onFinished) onFinished(false);
            return;
        }
        // Listado tras gs_init (deberían existir certificados)
        {
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                brls::Logger::info("[Pairing][keyDir] Post-gs_init archivos: {}", files.empty()? "(vacío)" : files);
            }
        }
        // Generar PIN
        char pin[5];
        srand((unsigned int)time(nullptr));
        snprintf(pin, sizeof(pin), "%d%d%d%d", rand()%10, rand()%10, rand()%10, rand()%10);
        std::string pinStr(pin);
        brls::Logger::info("[Pairing] PIN generado {}", pinStr);
        brls::sync([label, pinStr, spinner]() {
            std::string msg = brls::getStr("host_dialog/pairing_enter_pin");
            size_t pos = msg.find("$(pin)"); if (pos != std::string::npos) msg.replace(pos, 6, pinStr);
            label->setText(msg);
            spinner->setVisibility(brls::Visibility::GONE);
        });
        // Llamar gs_pair
        int pairRes = gs_pair(&serverData, pin);
        if (pairRes == GS_OK) {
            brls::Logger::info("[Pairing] Emparejamiento OK");
            HostStorage::savePairedHost(localHost.safeId, addr, serverData.httpPort, serverData.paired);
            HostStorage::writeDeviceIni(base + "/" + localHost.safeId, localHost.safeId, addr.c_str(), serverData.httpPort, serverData.paired);
            // Guardar serverData en mapa para futuro connect() reutilizable
            m_server_data[addr] = serverData;
            brls::sync([label]() { label->setText(brls::getStr("host_dialog/pairing_success")); });
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            *finished = true; if (onFinished) onFinished(true);
        } else {
            brls::Logger::error("[Pairing] gs_pair fallo {}", pairRes);
            std::string err = brls::getStr("host_dialog/pairing_error");
            brls::sync([label, err]() { label->setText(err); });
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            *finished = true; if (onFinished) onFinished(false);
        }
    }).detach();

    // Observador cierre
    std::thread([dialog, finished]() {
        while (!finished->load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));
        brls::sync([dialog]() {
            dialog->dismiss();
            brls::Application::unblockInputs();
        });
    }).detach();
    return true;
}

void GameStreamClient::getAppList(const std::string& address, AppListCallback callback) {
    if (m_server_data.count(address) == 0) {
        brls::Logger::error("[GameStreamClient] No conectado a {}", address);
        callback(std::vector<RemoteAppInfo>());
        return;
    }

    // Verificar si ya tenemos la lista en caché
    if (m_app_lists.count(address) > 0) {
        callback(m_app_lists[address]);
        return;
    }

    // Obtener lista de aplicaciones del servidor
    PAPP_LIST appList;
    int status = gs_applist(&m_server_data[address], &appList);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] Error al obtener lista de aplicaciones: {}", status);
        callback(std::vector<RemoteAppInfo>());
        return;
    }

    // Guardar referencia a la cabeza de la lista para liberación posterior
    PAPP_LIST appListHead = appList;

    // Convertir a vector
    std::vector<RemoteAppInfo> apps;
    PAPP_LIST current = appList;
    while (current) {
        RemoteAppInfo app;
        app.id = std::to_string(current->id);
        app.name = current->name ? std::string(current->name) : "Unknown App";
        app.iconUrl = ""; // No tenemos iconUrl en la estructura PAPP_LIST
        apps.push_back(app);
        current = current->next;
    }

    // Liberar la memoria de la lista de aplicaciones
    current = appListHead;
    while (current) {
        PAPP_LIST next = current->next;
        if (current->name) {
            free(current->name);
        }
        free(current);
        current = next;
    }

    // Ordenar por nombre
    std::sort(apps.begin(), apps.end(),
              [](const RemoteAppInfo& a, const RemoteAppInfo& b) {
                  return a.name < b.name;
              });

    // Guardar en caché
    m_app_lists[address] = apps;

    callback(apps);
}

bool GameStreamClient::quitApp(const std::string& address) {
    if (m_server_data.count(address) == 0) {
        brls::Logger::error("[GameStreamClient] No conectado a {}", address);
        return false;
    }

    brls::Logger::info("[GameStreamClient] Terminando aplicación en {}", address);

    int status = gs_quit_app(&m_server_data[address]);
    if (status != GS_OK) {
        brls::Logger::error("[GameStreamClient] Error al terminar aplicación: {}", status);
        return false;
    }

    brls::Logger::info("[GameStreamClient] Aplicación terminada correctamente");
    // Limpiar estado de stream activo si coincide
    auto it = m_active_streams.find(address);
    if (it != m_active_streams.end()) {
        m_active_streams.erase(it);
    }
    return true;
}

void GameStreamClient::setActiveStream(const std::string& address, int appId, const std::string& appName) {
    m_active_streams[address] = { appId, appName };
}

void GameStreamClient::clearActiveStream(const std::string& address) {
    m_active_streams.erase(address);
}

bool GameStreamClient::hasActiveStream(const std::string& address) const {
    return m_active_streams.count(address) > 0;
}

RemoteAppInfo GameStreamClient::activeAppInfo(const std::string& address) const {
    RemoteAppInfo info;
    auto it = m_active_streams.find(address);
    if (it != m_active_streams.end()) {
        info.id = std::to_string(it->second.appId);
        info.name = it->second.appName;
        info.iconUrl = ""; // placeholder
    }
    return info;
}