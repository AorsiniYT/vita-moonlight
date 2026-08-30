#include "debug.hpp"
#include "GameStreamClient.hpp"
#include <cstring>
#include "ConfigManager.hpp"
#include "model/HostStorage.hpp"
#include "crypto/CryptoManager.hpp"
#include "audio/MicrophoneManager.hpp"
#include "moonmic/MoonmicBridge.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <curl/curl.h>

// Directory creation is centralized in ConfigManager (ensureDirExists / ensureKeyDirExists)

GameStreamClient& GameStreamClient::instance() {
    static GameStreamClient instance;
    return instance;
}

GameStreamClient::GameStreamClient() {
    // empty constructor
}

GameStreamClient::~GameStreamClient() {
    // Clean server data
    m_server_data.clear();
    m_app_lists.clear();
}

bool GameStreamClient::connect(const std::string& address) {
    HostInfo h; h.ip = address; h.name = address; h.safeId = makeSafeHostId(address);
    return connect(h);
}

bool GameStreamClient::connect(const HostInfo& host) {
    if (host.ip.empty()) {
        vita_log::error("[GameStreamClient] Dirección vacía en connect(host)");
        return false;
    }
    std::string addr = host.ip;
    if (m_server_data.count(addr) > 0) {
        vita_log::info("[GameStreamClient] Ya conectado a %s", addr.c_str());
        return true;
    }
    SERVER_DATA serverData{};
    ConfigManager cfg; cfg.load();
    std::string baseDevices = cfg.getKeysDir();
    std::string safe = host.safeId.empty()? makeSafeHostId(host.name.empty()? host.ip : host.name) : host.safeId;
    // Resolve possible existing keyDir (migration from previous versions)
    auto resolveExistingKeyDir = [&](const std::string& base, const HostInfo& h)->std::string {
        std::vector<std::string> candidates;
        // 1. New scheme (safeId derived from name)
        candidates.push_back(base + "/" + safe);
        // 2. Name without domain (previously it may have been saved without .local)
        if (!h.name.empty()) {
            auto pos = h.name.find('.');
            if (pos != std::string::npos) {
                std::string shortName = h.name.substr(0, pos);
                candidates.push_back(base + "/" + makeSafeHostId(shortName));
            }
        }
        // 3. Cruda IP
        if (!h.ip.empty()) candidates.push_back(base + "/" + h.ip);
        // 4. generated safeId from IP
        if (!h.ip.empty()) candidates.push_back(base + "/" + makeSafeHostId(h.ip));
        // The first one that contains device.ini we use it
        for (auto& c : candidates) {
            struct stat st{}; if (stat(c.c_str(), &st)==0 && S_ISDIR(st.st_mode)) {
                std::string devIni = c + "/device.ini";
                if (stat(devIni.c_str(), &st)==0) {
                    vita_log::info("[GameStreamClient][keyDir-resolver] Reutilizando keyDir existente '%s' (device.ini encontrado)", c.c_str());
                    return c;
                }
            }
        }
        // If none have device.ini, we return the new schema
        return candidates.front();
    };
    std::string keyDir = resolveExistingKeyDir(baseDevices, host);
    vita_log::info("[GameStreamClient] keyDir elegido para '%s' => '%s' (safeId base='%s')", addr.c_str(), keyDir.c_str(), safe.c_str());
    // keyDir diagnostics
    {
        struct stat st{};
        if (stat(keyDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            vita_log::info("[GameStreamClient][keyDir] EXISTE '%s'", keyDir.c_str());
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                vita_log::info("[GameStreamClient][keyDir] Archivos: %s", files.empty()? "(vacío)" : files.c_str());
            } else {
                vita_log::error("[GameStreamClient][keyDir] No se pudo abrir directorio '%s'", keyDir.c_str());
            }
        } else {
            vita_log::warning("[GameStreamClient][keyDir] NO EXISTE '%s' antes de gs_init", keyDir.c_str());
        }
    }
    int status = gs_init(&serverData, addr, keyDir);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] Error al inicializar servidor: %d", status);
        return false;
    }
    // Save keyDir used for this server for diagnostics and checks
    m_key_dirs[addr] = keyDir;
    m_server_data[addr] = serverData;
    vita_log::info("[GameStreamClient] Conexión exitosa a %s", addr.c_str());
    const char* srv_addr = serverData.serverInfo.address ? serverData.serverInfo.address : "NULL";
    vita_log::info("[GameStreamClient] ServerInfo address: %s", srv_addr);

    // Diagnosis: compare device.ini.paired (if it exists) with what the
    // server (serverData.paired). This helps detect desynchronizations.
    {
        std::string deviceIni = keyDir + "/device.ini";
        struct stat st{};
        if (stat(deviceIni.c_str(), &st) == 0) {
            // we try to read the informational paired field already loaded by HostStorage
            vita_log::info("[GameStreamClient] Diagnóstico: device.ini presente en '%s' y server.paired=%d", deviceIni.c_str(), serverData.paired);
        } else {
            vita_log::info("[GameStreamClient] Diagnóstico: device.ini NO presente en '%s' y server.paired=%d", keyDir.c_str(), serverData.paired);
        }
    }

    // If uniqueid.dat exists in keyDir, we rewrite device.ini to ensure that
    // the file exists and reflects current values ​​(paired/ip/port). NOTE: no
    // we write the `uuid` field in device.ini — that is intentional and maintains
    // support for Moonlight-Switch, which does not persist `uuid` in device.ini.
    // Only create/update device.ini if ​​it does NOT exist. If it already exists (for example
    // after a previous pairing), we should not overwrite it with the value
    // `serverData.paired` which can report the server in temporary states
    // (this caused device.ini to go from paired=true to paired=false).
    std::string deviceIniPath = keyDir + "/device.ini";
    struct stat st{};
        if (::stat(deviceIniPath.c_str(), &st) != 0) {
        try {
            HostStorage::writeDeviceIni(keyDir, safe, addr.c_str(), serverData.httpPort, serverData.paired, serverData.mac.empty() ? nullptr : serverData.mac.c_str());
        } catch (...) {
            vita_log::warning("[GameStreamClient] No se pudo crear device.ini para %s", addr.c_str());
        }
    } else {
        vita_log::info("[GameStreamClient] device.ini ya existe en '%s' -> no se sobreescribe paired", deviceIniPath.c_str());
        // If device.ini already exists but the server reports a valid MAC,
        // we try to update only the mac= line without overwriting the rest.
        if (!serverData.mac.empty() && serverData.mac != "00:00:00:00:00:00") {
            vita_log::info("[GameStreamClient] Observado MAC '%s' para %s -> (sin acción: persistencia gestionada por flujo de pairing)", serverData.mac.c_str(), addr.c_str());
        }
    }
    return true;
}

bool GameStreamClient::isConnected(const std::string& address) {
    return m_server_data.count(address) > 0;
}

SERVER_DATA& GameStreamClient::serverData(const std::string& address) {
    if (m_server_data.count(address) == 0) {
        vita_log::error("[GameStreamClient] No hay datos para servidor %s", address.c_str());
        // Keep the reference valid when no server data is available.
        static SERVER_DATA empty;
        return empty;
    }
    return m_server_data[address];
}

std::string GameStreamClient::getKeyDirFor(const std::string& address) const {
    auto it = m_key_dirs.find(address);
    if (it == m_key_dirs.end()) return std::string();
    return it->second;
}

bool GameStreamClient::startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId) {
    return startApp(address, config, appId, StartMode::AUTO, 0, 0);
}

bool GameStreamClient::startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId, int displayWidth, int displayHeight) {
    return startApp(address, config, appId, StartMode::AUTO, displayWidth, displayHeight);
}

bool GameStreamClient::startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId, StartMode mode, int displayWidth, int displayHeight) {
    if (m_server_data.count(address) == 0) {
        vita_log::error("[GameStreamClient] No conectado a %s", address.c_str());
        return false;
    }
    vita_log::info("[GameStreamClient] Iniciando aplicación %d en %s (mode=%d) display=%dx%d", appId, address.c_str(), (int)mode, displayWidth, displayHeight);

    // If we ask for RESUME_ONLY, check that there is currentGame
    if (mode == StartMode::RESUME_ONLY) {
        SERVER_DATA& sd = m_server_data[address];
        if (sd.currentGame == 0) {
            vita_log::warning("[GameStreamClient] Resume solicitado pero server.currentGame==0 para %s", address.c_str());
            return false;
        }
    }

    // If we are attempting a summary requested by the user, mark it so that
    // probeActiveSession do not show the "Active Session" dialog again
    // while the resume attempt is in progress.
    bool markedResume = false;
    if (mode == StartMode::RESUME_ONLY) {
        vita_log::info("[GameStreamClient] Marcando resume en progreso para %s", address.c_str());
        m_resume_in_progress.insert(address);
        markedResume = true;
        // Record attempted resume with expiration to cover cases where
        // the UI is recreated before setActiveStream is called.
        m_resume_attempts[address] = std::chrono::steady_clock::now();
    }

    // If we ask for NEW_ONLY, we force fresh launch through the global flag used by libgamestream
    extern bool g_force_fresh_launch_h264; // defined in vita_session_globals.cpp
    bool prevForce = g_force_fresh_launch_h264;
    if (mode == StartMode::NEW_ONLY) g_force_fresh_launch_h264 = true;

    // Load SOPS (Stream Optimization) setting from configuration
    // This enables Sunshine to automatically change host display resolution/FPS/HDR
    ConfigManager sopsConfig;
    sopsConfig.load();
    VideoSettings videoSettings = sopsConfig.getVideoSettings();
    bool sops = videoSettings.sops; // Default is true
    vita_log::info("[GameStreamClient] SOPS (Optimize game settings) = %d", sops);
    vita_log::info("[GameStreamClient] Local audio playback on host = %d", videoSettings.localaudio);

    // Pre-start Moonmic resolution handshake (Plan A)
    uint16_t targetWidth = 0;
    uint16_t targetHeight = 0;
    {
        ConfigManager micCfg;
        micCfg.load();
        VideoSettings micSettings = micCfg.getVideoSettings();

        std::string micHost = micSettings.microphone_host_ip.empty() ? address : micSettings.microphone_host_ip;
        int micPort = micSettings.microphone_port;

        auto& bridge = moonmic::MoonmicBridge::getInstance();
        bridge.loadConfig();
        auto [w, h] = bridge.getTargetResolution();
        targetWidth = w;
        targetHeight = h;

        auto handshakeResult = bridge.sendResolutionHandshake(micHost, micPort);
        vita_log::info("[GameStreamClient] Moonmic pre-start handshake %s to %s:%d (target %dx%d)", handshakeResult.success ? "sent" : "failed", micHost.c_str(), micPort, targetWidth, targetHeight);
    }

    // If caller did not override display dimensions, use the Moonmic target
    if (displayWidth == 0 && displayHeight == 0 && targetWidth > 0 && targetHeight > 0) {
        displayWidth = targetWidth;
        displayHeight = targetHeight;
        vita_log::info("[GameStreamClient] Applying Moonmic target resolution to gs_start_app: %dx%d", displayWidth, displayHeight);
    }

    int status = gs_start_app(&m_server_data[address], &config, appId, sops,
                              videoSettings.localaudio, 0x1, displayWidth, displayHeight);


    // Reset global flag
    if (mode == StartMode::NEW_ONLY) g_force_fresh_launch_h264 = prevForce;

    if (status != GS_OK) {
        // In case of failure, clear the resume flag to allow subsequent retries
        if (markedResume) {
            vita_log::info("[GameStreamClient] Resume falló para %s -> limpiando resume en progreso (status=%d)", address.c_str(), status);
            m_resume_in_progress.erase(address);
            m_resume_attempts.erase(address);
        }
        vita_log::error("[GameStreamClient] Error al iniciar aplicación: %d", status);
        return false;
    }

    // NOTE: in case of success we leave the `m_resume_in_progress` flag active until
    // the view itself confirms that the session has started. This avoids conditions of
    // race where probeActiveSession re-announces the active session between the
    // return of gs_start_app and the effective creation of VitaSession.

    // Save copy of configuration (includes remoteInputAesKey populated by gs_start_app)
    m_last_stream_cfg[address] = config;
    vita_log::info("[GameStreamClient] Aplicación iniciada correctamente (remoteInputKey set)");
    
    // Start microphone if enabled in settings
    ConfigManager micConfig;
    micConfig.load();
    VideoSettings micSettings = micConfig.getVideoSettings();
    
    if (micSettings.enable_microphone) {
        vita_log::info("[GameStreamClient] Microphone enabled in settings - starting transmission");
        
        // Use custom host IP if specified, otherwise use stream host
        std::string micHost = micSettings.microphone_host_ip.empty() 
                              ? address 
                              : micSettings.microphone_host_ip;
        
        bool micStarted = MicrophoneManager::getInstance().start(
            micHost,
            micSettings.microphone_port,
            micSettings.microphone_sample_rate,
            micSettings.microphone_channels,
            micSettings.microphone_bitrate
        );
        
        if (micStarted) {
            vita_log::info("[GameStreamClient] Microphone started successfully to %s:%d", 
                               micHost.c_str(), micSettings.microphone_port);
        } else {
            vita_log::warning("[GameStreamClient] Microphone failed to start - will retry every 10s");
        }
    } else {
        vita_log::debug("[GameStreamClient] Microphone disabled in settings");
    }
    
    return true;
}

bool GameStreamClient::resumeApp(const std::string& address, STREAM_CONFIGURATION& config, int appId) {
    auto server = m_server_data.find(address);
    if (server == m_server_data.end() || server->second.currentGame == 0) {
        vita_log::warning("[GameStreamClient] No hay sesión activa para reanudar en %s", address.c_str());
        return false;
    }

    ConfigManager configManager;
    configManager.load();
    VideoSettings settings = configManager.getVideoSettings();
    int status = gs_start_app(&server->second, &config, appId, settings.sops,
                              settings.localaudio, 0x1, 0, 0);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] Reanudación de sesión falló: %d", status);
        return false;
    }

    m_last_stream_cfg[address] = config;
    vita_log::info("[GameStreamClient] Sesión reanudada y parámetros RTSP renovados");
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
        vita_log::error("[GameStreamClient] pair: no conectado a %s", address.c_str());
        return false;
    }
    SERVER_DATA& s = m_server_data[address];
    if (s.paired) {
        vita_log::info("[GameStreamClient] pair: ya emparejado");
        return true;
    }
    char pinBuf[16];
    memset(pinBuf, 0, sizeof(pinBuf));
    strncpy(pinBuf, pin.c_str(), sizeof(pinBuf)-1);
    int status = gs_pair(&s, pinBuf);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] pair fallo status=%d", status);
        return false;
    }
    vita_log::info("[GameStreamClient] pairing OK");
    // NOTE: we do not clear `currentGame` here to avoid discrepancies with the
    // actual status reported by the server. Before it was forced to 0 to avoid
    // that the UI would show an active session immediately after pairing,
    // but that can cause `gs_start_app` to fail with "An app is already
    // running" if the server actually has an active session. Now
    // we keep the value provided by the server.
        // Invalidate applist cache for this host: after pairing we want to force
        // The next app fetch asks the server for the current list.
        if (m_app_lists.count(address) > 0) m_app_lists.erase(address);
        // If after the pair the server provided us with a valid MAC, update device.ini
        std::string kd = getKeyDirFor(address);
        if (!kd.empty() && !s.mac.empty() && s.mac != "00:00:00:00:00:00") {
            vita_log::info("[GameStreamClient] pair(): MAC '%s' detectado -> (sin acción: persistencia gestionada por flujo de pairing)", s.mac.c_str(), kd.c_str());
        }
    return true;
}

bool GameStreamClient::unpair(const std::string& address) {
    if (m_server_data.count(address) == 0) return false;
    SERVER_DATA& s = m_server_data[address];
    int status = gs_unpair(&s);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] unpair fallo status=%d", status);
        return false;
    }
    s.paired = false;
    return true;
}

bool GameStreamClient::isPaired(const std::string& address) {
    if (m_server_data.count(address) == 0) return false;
    return m_server_data[address].paired;
}

bool GameStreamClient::beginPairing(const HostInfo& host, std::function<void(bool)> onFinished, std::function<void(const std::string&)> onPinReady) {
    HostInfo localHost = host;
    if (localHost.safeId.empty()) localHost.safeId = makeSafeHostId(localHost.name.empty()? localHost.ip : localHost.name);
    std::string addr = localHost.ip;
    if (addr.empty()) {
        vita_log::error("[GameStreamClient] beginPairing: host ip vacío");
        if (onFinished) onFinished(false); return false;
    }

    // Fast-path: if device.ini exists we try to connect and verify if the
    // server considers the client matched. If the server already recognizes it,
    // we can return success immediately; If not, we continue with the flow
    // normal pairing (gs_init + PIN + gs_pair) to update the status.
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
        bool foundDeviceIni = false;
        for (auto& c : candidates) {
            std::string di = c + "/device.ini";
            if (stat(di.c_str(), &st)==0) {
                vita_log::info("[Pairing][fast-path] device.ini existente en '%s' -> verificar estado en servidor", c.c_str());
                foundDeviceIni = true;
                break;
            }
        }
        if (foundDeviceIni) {
            // Try to use the exact keyDir where device.ini exists to check
            // if the server recognizes the pairing. This reproduces the
            // Moonlight-Switch behavior: use the same keyDir as
            // contains device.ini instead of letting connect() resolve another
            // candidate (avoids mismatches between device.ini and certs/uniqueid).
            std::string foundKeyDir;
            for (auto &c : candidates) {
                std::string di = c + "/device.ini";
                struct stat st{};
                if (stat(di.c_str(), &st) == 0) {
                    foundKeyDir = c;
                    break;
                }
            }
            if (!foundKeyDir.empty()) {
                try {
                    SERVER_DATA tmp;
                    vita_log::info("[Pairing][fast-path] intentando gs_init con keyDir='%s' para comprobar paired en servidor", foundKeyDir.c_str());
                    int initRes = gs_init(&tmp, localHost.ip, foundKeyDir);
                    if (initRes == GS_OK) {
                        if (tmp.paired) {
                            vita_log::info("[Pairing][fast-path] servidor reconoce emparejamiento usando keyDir='%s' -> omitir pairing", foundKeyDir.c_str());
                            // Save minimal serverData to local map for future use
                            m_server_data[localHost.ip] = tmp;
                            if (onFinished) onFinished(true);
                            return true;
                        } else {
                            vita_log::info("[Pairing][fast-path] server PairStatus=0 usando keyDir='%s' -> continuar pairing", foundKeyDir.c_str());
                        }
                    } else {
                        vita_log::warning("[Pairing][fast-path] gs_init falló (%d ) con keyDir='%s' -> continuar pairing", initRes, foundKeyDir.c_str());
                    }
                } catch (...) {
                    // In case of error, do not block the pairing flow
                    vita_log::error("[Pairing][fast-path] excepción al intentar gs_init con keyDir='%s'", foundKeyDir.c_str());
                }
            }
        }
    }

    // Create dialog (spinner + label) non-cancelable as before
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
    auto cancelled = std::make_shared<std::atomic<bool>>(false); // For compatibility if cancel is added in the future

    // Hilo principal pairing
    std::thread([=]() mutable {
        auto t_start = std::chrono::high_resolution_clock::now();
        ConfigManager cfg; cfg.load();
        std::string base = cfg.getKeysDir();
        std::string keyDir = base + "/" + localHost.safeId;
        // Secure directory (simplified, no recursive deletion)
        struct stat st{};
        // Resolving keyDir by reusing connect logic to avoid duplicate
        ConfigManager tmpCfg; tmpCfg.load();
        std::string baseDir = tmpCfg.getKeysDir();
        // Reuse simple mini-resolver: test safeId, non-domain name, ip
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
        // (fast-path already evaluated before creating the dialog)
        // If it does not exist, we proceed as normal flow
        // Ensure directory existence using centralized ConfigManager
        if (!ConfigManager().ensureKeyDirExists(localHost.safeId)) {
            vita_log::error("[Pairing] No se pudo crear/asegurar keyDir '%s'", keyDir.c_str());
        } else {
            vita_log::info("[Pairing] keyDir asegurado %s", keyDir.c_str());
        }
        // Initial listing
        {
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                vita_log::info("[Pairing][keyDir] Inicial archivos: %s", files.empty()? "(vacío)" : files.c_str());
            }
        }

        SERVER_DATA serverData{};
        // Before initializing, clear any cached certificates to
        // force regeneration from scratch. This prevents m_cert/m_key from
        // OpenSSLCryptoManager remain in memory and prevent generation
        // of new certificates when pairing again.
        try {
            // Clear client.pem, key.pem and uniqueid.dat to force full regeneration
            namespace fs = std::filesystem;
            std::string clientPem = keyDir + "/client.pem";
            std::string keyPem = keyDir + "/key.pem";
            std::string uid = keyDir + "/uniqueid.dat";
            std::error_code ec;
            if (fs::exists(clientPem)) fs::remove(clientPem, ec);
            if (fs::exists(keyPem)) fs::remove(keyPem, ec);
            if (fs::exists(uid)) fs::remove(uid, ec);
            // Clear cache in memory and any remains on disk
            CryptoManager::remove_cert_key_pair(keyDir);
        } catch (...) {
            vita_log::warning("[Pairing] No se pudo limpiar cache de certificados para keyDir='%s'", keyDir.c_str());
        }
        // gs_init (blocking here, we can optimize with native Vita thread later)
    int initRes = gs_init(&serverData, addr, keyDir);
        if (initRes != GS_OK) {
            vita_log::error("[Pairing] gs_init fallo %d", initRes);
            brls::sync([dialog, label]() { label->setText(brls::getStr("host_dialog/pairing_error_init")); });
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            *finished = true;
            if (onFinished) onFinished(false);
            return;
        }
        // Listed after gs_init (certificates should exist)
        {
            DIR* d = opendir(keyDir.c_str());
            if (d) {
                struct dirent* ent; std::string files;
                while ((ent = readdir(d)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(d);
                vita_log::info("[Pairing][keyDir] Post-gs_init archivos: %s", files.empty()? "(vacío)" : files.c_str());
            }
        }
        // Generate PIN
        // If the host is busy (currentGame != 0) we will force pairing.
        // Legacy clients sometimes allowed force pairing even with an active session.
        bool forced = false;
        int oldCurrent = serverData.currentGame;
        if (serverData.currentGame != 0) {
            forced = true;
            vita_log::warning("[Pairing] host ocupado (currentGame=%d) -> forzando emparejado (puede fallar)", serverData.currentGame);
            brls::sync([label]() {
                label->setText(brls::getStr("host_dialog/pairing_force_warning"));
            });
            // Force gs_pair not to reject for local state
            serverData.currentGame = 0;
            // Give a short delay for the user to see the message
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        }

        char pin[5];
        srand((unsigned int)time(nullptr));
        snprintf(pin, sizeof(pin), "%d%d%d%d", rand()%10, rand()%10, rand()%10, rand()%10);
        std::string pinStr(pin);
        vita_log::info("[Pairing] PIN generado %s", pinStr.c_str());
        brls::sync([label, pinStr, spinner]() {
            std::string msg = brls::getStr("host_dialog/pairing_enter_pin");
            size_t pos = msg.find("$(pin)"); if (pos != std::string::npos) msg.replace(pos, 6, pinStr);
            label->setText(msg);
            spinner->setVisibility(brls::Visibility::GONE);
        });
        // Notify the caller that the PIN is ready (for example AddHostTab) to
        // that can unlock inputs or show the PIN in external UI.
        if (onPinReady) onPinReady(pinStr);
        // If there is an onPinReady callback (passed by the caller), invoke it with the PIN
        // out of sync to not touch UI from non-UI threads.
        if (onFinished) {
            // There is no onPinReady parameter here; let the caller use onFinished or
            // added overhead in header (caller can pass onPinReady)
        }
        // Llamar gs_pair
            int pairRes = gs_pair(&serverData, pin);
        // Restore currentGame if we modify it
        if (forced) {
            serverData.currentGame = oldCurrent;
        }
        if (pairRes == GS_OK) {
            vita_log::info("[Pairing] Emparejamiento OK");
            // Force paired=true on serverData regardless of HTTPS requery,
            // since the gs_pair handshake was successful. This prevents the UI from displaying
            // "not paired" after successful pairing if /serverinfo HTTPS timeout.
            serverData.paired = true;
            // After a successful pairing we keep the value of
            // `serverData.currentGame` that the server has reported (if
            // libgamestream did an HTTPS re-query probably already
            // contains the correct value). We do not force 0 to avoid
            // inconsistencies when requesting an app to start immediately
            // after pairing.
            HostStorage::savePairedHost(localHost.safeId, addr, serverData.httpPort, serverData.paired, serverData.mac);
            // Update device.ini (not including uuid) so that the UI/storage
            // have the paired/ip/port information. This reproduces the
            // Moonlight-Switch behavior where device.ini does not contain the
            // uniqueid used in requests.
            HostStorage::writeDeviceIni(base + "/" + localHost.safeId, localHost.safeId, addr.c_str(), serverData.httpPort, serverData.paired, serverData.mac.empty() ? nullptr : serverData.mac.c_str());
            // Save serverData in map for future reusable connect()
            m_server_data[addr] = serverData;
            brls::sync([label]() { label->setText(brls::getStr("host_dialog/pairing_success")); });
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            *finished = true; if (onFinished) onFinished(true);
        } else {
            // Log gs_error from libgamestream for diagnostics
            std::string gse_str = gs_error();
            const char* gse = gse_str.empty() ? "(empty)" : gse_str.c_str();
            vita_log::error("[Pairing] gs_pair fallo %d -> gs_error='%s'", pairRes, gse);
            // Also log keyDir content to help playback
            std::string files;
            DIR* dchk = opendir(keyDir.c_str());
            if (dchk) {
                struct dirent* ent;
                while ((ent = readdir(dchk)) != nullptr) {
                    if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0) continue;
                    files += ent->d_name; files += ",";
                }
                closedir(dchk);
            } else {
                files = "(no-open)";
            }
            vita_log::info("[Pairing][keyDir] Estado actual archivos: %s", files.empty()? "(vacío)" : files.c_str());
            std::string err = brls::getStr("host_dialog/pairing_error");
            brls::sync([label, err]() { label->setText(err); });
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            *finished = true; if (onFinished) onFinished(false);
        }
    }).detach();

    // Close observer
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
        vita_log::error("[GameStreamClient] No conectado a %s", address.c_str());
        callback(std::vector<RemoteAppInfo>());
        return;
    }

    // Check if we already have the list cached
    if (m_app_lists.count(address) > 0) {
        callback(m_app_lists[address]);
        return;
    }

    // Get list of applications from server
    PAPP_LIST appList;
    int status = gs_applist(&m_server_data[address], &appList);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] Error al obtener lista de aplicaciones: %d", status);
        callback(std::vector<RemoteAppInfo>());
        return;
    }

    // Save reference to the head of the list for later release
    PAPP_LIST appListHead = appList;

    // Convert to vector
    std::vector<RemoteAppInfo> apps;
    PAPP_LIST current = appList;
    while (current) {
        RemoteAppInfo app;
        app.id = std::to_string(current->id);
        app.name = current->name ? std::string(current->name) : "Unknown App";
        app.iconUrl = ""; // We don't have iconUrl in PAPP_LIST structure
        apps.push_back(app);
        current = current->next;
    }

    // Free up app list memory
    current = appListHead;
    while (current) {
        PAPP_LIST next = current->next;
        if (current->name) {
            free(current->name);
        }
        free(current);
        current = next;
    }

    // Sort by name
    std::sort(apps.begin(), apps.end(),
              [](const RemoteAppInfo& a, const RemoteAppInfo& b) {
                  return a.name < b.name;
              });

    // Save to cache
    m_app_lists[address] = apps;

    callback(apps);
}

bool GameStreamClient::getAppBoxart(const std::string& address, int appId, Data& outData) {
    if (m_server_data.count(address) == 0) {
        vita_log::error("[GameStreamClient] getAppBoxart: no conectado a %s", address.c_str());
        return false;
    }
    int status = gs_app_boxart(&m_server_data[address], appId, &outData);
    if (status != GS_OK) {
        vita_log::warning("[GameStreamClient] gs_app_boxart falló para appId=%d con status=%d", appId, status);
        return false;
    }
    return true;
}

bool GameStreamClient::quitApp(const std::string& address) {
    if (m_server_data.count(address) == 0) {
        vita_log::error("[GameStreamClient] No conectado a %s", address.c_str());
        return false;
    }

    vita_log::info("[GameStreamClient] Terminando aplicación en %s", address.c_str());

    int status = gs_quit_app(&m_server_data[address]);
    if (status != GS_OK) {
        vita_log::error("[GameStreamClient] Error al terminar aplicación: %d", status);
        return false;
    }

    vita_log::info("[GameStreamClient] Aplicación terminada correctamente");
    
    // Stop microphone transmission
    MicrophoneManager::getInstance().stop();
    vita_log::info("[GameStreamClient] Microphone stopped");
    
    // Clear active stream status if match
    auto it = m_active_streams.find(address);
    if (it != m_active_streams.end()) {
        m_active_streams.erase(it);
    }
    // Update currentGame to 0 after finishing the app
    m_server_data[address].currentGame = 0;
    return true;
}

void GameStreamClient::setActiveStream(const std::string& address, int appId, const std::string& appName) {
    m_active_streams[address] = { appId, appName };
    // If there was a resume in progress for this host, we considered the
    // session has already started correctly and we cleared the flag to allow
    // future detections/dialogues where appropriate.
    if (m_resume_in_progress.count(address) > 0) {
        vita_log::info("[GameStreamClient] setActiveStream: limpiando resume en progreso para %s", address.c_str());
        m_resume_in_progress.erase(address);
    }
}

void GameStreamClient::clearActiveStream(const std::string& address) {
    m_active_streams.erase(address);
    
    // Stop microphone when stream ends (safety measure)
    MicrophoneManager::getInstance().stop();
    vita_log::debug("[GameStreamClient] clearActiveStream: microphone stopped");
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
        info.iconUrl = "";
    }
    return info;
}

bool GameStreamClient::probeActiveSession(const HostInfo& host, RemoteAppInfo& outRunning) {
    outRunning = RemoteAppInfo();
    if (host.ip.empty()) return false;
    const std::string& address = host.ip;

    vita_log::info("[GameStreamClient] probeActiveSession ENTRY for %s (thread=%lld)", address.c_str(), (long long)std::hash<std::thread::id>()(std::this_thread::get_id()));

    // Try to connect to the host (gs_init) if we are not connected
    bool connected = isConnected(address);
    if (!connected) {
        connected = connect(host);
    }
    if (!connected) return false;

    // Inspect serverData.currentGame and refresh it directly from Sunshine
    SERVER_DATA& sd = serverData(address);
    bool sunshineRefreshed = false;
    {
        std::string serverinfo;
        if (fetchSunshineServerinfo(address, serverinfo)) {
            int liveCurrentGame = 0;
            if (parseSunshineCurrentGame(serverinfo, liveCurrentGame)) {
                sunshineRefreshed = true;
                sd.currentGame = liveCurrentGame;
                vita_log::info("[GameStreamClient] probeActiveSession: Sunshine reports currentGame=%d for %s", liveCurrentGame, address.c_str());
                if (liveCurrentGame == 0) {
                    m_active_streams.erase(address);
                }
            } else {
                vita_log::warning("[GameStreamClient] probeActiveSession: Sunshine serverinfo missing/invalid currentGame for %s", address.c_str());
            }
        } else {
            vita_log::warning("[GameStreamClient] probeActiveSession: Sunshine serverinfo fetch failed for %s", address.c_str());
        }
    }

    // Memory fast-path only if we couldn't refresh from Sunshine
    if (!sunshineRefreshed) {
        auto it = m_active_streams.find(address);
        if (it != m_active_streams.end()) {
            outRunning.id = std::to_string(it->second.appId);
            outRunning.name = it->second.appName;
            outRunning.iconUrl = "";
            return true;
        }
    }
    // Diagnostic: check that the keyDir used contains certificates/uniqueid
    {
        std::string kd = getKeyDirFor(address);
        if (!kd.empty()) {
            struct stat st;
            std::string clientPem = kd + "/client.pem";
            std::string keyPem = kd + "/key.pem";
            std::string uniqueid = kd + "/uniqueid.dat";
            bool hasClient = (stat(clientPem.c_str(), &st) == 0);
            bool hasKey = (stat(keyPem.c_str(), &st) == 0);
            bool hasUid = false;
            if (stat(uniqueid.c_str(), &st) == 0 && st.st_size > 0) hasUid = true;
            if (!hasClient || !hasKey || !hasUid) {
                vita_log::info("[GameStreamClient] probeActiveSession: keyDir='%s' falta client/key/uniqueid -> no reanudar", kd.c_str());
                return false;
            }
        }
    }
    // If the host is not paired, we cannot resume from it
    // Commented to allow resuming active sessions even if PairStatus=0
    // if (!sd.paired) {
    //     vita_log::info("[GameStreamClient] probeActiveSession: host %s not matched according to serverData -> do not resume", address.c_str());
    //     return false;
    // }
    // If there is a UI-initiated resume in progress for this host, avoid
    // probeActiveSession indicates that there is an active session — so you don't
    // will reopen the "Active Session" dialog while the attempt to resume
    // is in progress.
    if (m_resume_in_progress.count(address) > 0) {
        vita_log::info("[GameStreamClient] probeActiveSession: omitiendo notificación de sesión activa para %s porque resume está en progreso (resume_in_progress_count=%d)",
                           address.c_str(), (int)m_resume_in_progress.count(address));
        return false;
    }

    // Check if there was a recent resume attempt (guard with expiration)
    auto itAttempt = m_resume_attempts.find(address);
    if (itAttempt != m_resume_attempts.end()) {
        auto now = std::chrono::steady_clock::now();
        auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - itAttempt->second).count();
        const long long kExpireMs = 10000; // 10s
        if (ageMs >= 0 && ageMs < kExpireMs) {
            vita_log::info("[GameStreamClient] probeActiveSession: omitiendo notificación para %s porque resumeAttempt reciente (ageMs=%lld)", address.c_str(), ageMs);
            return false;
        } else {
            // Guard expired, delete
            vita_log::info("[GameStreamClient] probeActiveSession: resumeAttempt para %s expiró (ageMs=%lld) -> continuar comprobación", address.c_str(), ageMs);
            m_resume_attempts.erase(itAttempt);
        }
    }
    if (sd.currentGame == 0) return false;

    // Fill in ID
    outRunning.id = std::to_string(sd.currentGame);
    outRunning.name = "";

    // Try to resolve name through app list
    std::vector<RemoteAppInfo> apps;
    getAppList(address, [&apps](const std::vector<RemoteAppInfo>& a){ apps = a; });
    for (const auto& a : apps) {
        if (a.id == outRunning.id) { outRunning.name = a.name; break; }
    }

    if (outRunning.name.empty()) {
        // Default name if we couldn't solve it
        outRunning.name = "Running session"; // default text; UI can replace with i18n
    }

    // Record in memory for future local reference
    ActiveStream s; s.appId = sd.currentGame; s.appName = outRunning.name;
    m_active_streams[address] = s;
    vita_log::info("[GameStreamClient] probeActiveSession: registro m_active_streams[%s] = {{ appId=%d, appName='%s' }}", address.c_str(), sd.currentGame, outRunning.name.c_str());
    return true;
}

// Static callback for curl write (Vita-safe, no lambda)
static size_t sunshine_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

bool GameStreamClient::fetchSunshineServerinfo(const std::string& address, std::string& response) {
    response.clear();

    std::string keyDir = getKeyDirFor(address);
    if (keyDir.empty()) {
        vita_log::error("[GameStreamClient] fetchSunshineServerinfo: No keyDir for %s", address.c_str());
        return false;
    }

    std::string certPath = keyDir + "/client.pem";
    std::string keyPath = keyDir + "/key.pem";
    std::string uniqueidPath = keyDir + "/uniqueid.dat";

    std::string uniqueid;
    FILE* f = fopen(uniqueidPath.c_str(), "r");
    if (f) {
        char buf[32] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            uniqueid = buf;
            while (!uniqueid.empty() && (uniqueid.back() == '\n' || uniqueid.back() == '\r')) {
                uniqueid.pop_back();
            }
        }
        fclose(f);
    }

    if (uniqueid.empty()) {
        vita_log::error("[GameStreamClient] fetchSunshineServerinfo: Failed to read uniqueid for %s", address.c_str());
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        vita_log::error("[GameStreamClient] fetchSunshineServerinfo: Failed to init curl");
        return false;
    }

    int port = 47984;
    if (m_server_data.count(address) > 0) {
        port = m_server_data[address].httpsPort;
        if (port == 0) port = 47984;
    }

    char urlBuf[512];
    snprintf(urlBuf, sizeof(urlBuf), "https://%s:%d/serverinfo?uniqueid=%s",
             address.c_str(), port, uniqueid.c_str());

    std::string response_string;

    curl_easy_setopt(curl, CURLOPT_URL, urlBuf);
    curl_easy_setopt(curl, CURLOPT_SSLCERT, certPath.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, keyPath.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sunshine_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        vita_log::warning("[GameStreamClient] Sunshine serverinfo failed (curl error): %s", curl_easy_strerror(res));
        return false;
    }

    if (http_code != 200) {
        vita_log::warning("[GameStreamClient] Sunshine serverinfo HTTP error: %ld", http_code);
        return false;
    }

    response = std::move(response_string);
    return true;
}

bool GameStreamClient::parseSunshineCurrentGame(const std::string& response, int& outCurrentGame) {
    outCurrentGame = 0;

    const std::string tagOpen = "<currentgame>";
    const std::string tagClose = "</currentgame>";

    auto start = response.find(tagOpen);
    auto end = response.find(tagClose);
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return false;
    }

    start += tagOpen.size();
    std::string value = response.substr(start, end - start);
    try {
        outCurrentGame = std::stoi(value);
    } catch (...) {
        return false;
    }
    return true;
}

int GameStreamClient::getSunshinePairStatus(const std::string& address) {
    std::string response;
    if (!fetchSunshineServerinfo(address, response)) {
        return 0;
    }

    if (response.find("<PairStatus>1</PairStatus>") != std::string::npos) {
        vita_log::info("[GameStreamClient] Sunshine validation success (PairStatus=1)");
        return 1;
    }

    vita_log::warning("[GameStreamClient] Sunshine validation returned unpaired/unknown");
    return 0;
}
