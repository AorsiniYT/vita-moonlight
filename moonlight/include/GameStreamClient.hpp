#ifndef GAMESTREAM_CLIENT_HPP
#define GAMESTREAM_CLIENT_HPP

// Include Borealis first to avoid conflicts with BUTTON_* constants
#include "borealis.hpp"

#include <map>
#include <string>
#include <functional>
#include <set>
#include <vector>
#include <chrono>

// Limelight headers after Borealis
#include "client.h"
#include "errors.h"
#include "Limelight.h"

// RemoteAppInfo: lightweight structure used by the UI to list remote apps
struct RemoteAppInfo {
    std::string id;
    std::string name;
    std::string iconUrl;
};

struct HostInfo; // forward

typedef std::function<void(const std::vector<RemoteAppInfo>&)> AppListCallback;
typedef std::function<void(bool)> BoolCallback;

class GameStreamClient {
public:
    static GameStreamClient& instance();

    // Server initialization
    bool connect(const std::string& address);
    bool connect(const HostInfo& host); // use consistent safeId
    bool isConnected(const std::string& address);

    // Get data from server
    SERVER_DATA& serverData(const std::string& address);

    // Start application (returns true if started successfully)
    bool startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId);
    bool startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId, int displayWidth, int displayHeight);
    
    // Enum to control startApp behavior
    enum class StartMode {
        AUTO = 0,        // Allow automatic resume if there is an active session
        RESUME_ONLY = 1, // Just resume, fail if no session active
        NEW_ONLY = 2     // Always fresh launch, ignore active session
    };
    bool startApp(const std::string& address, STREAM_CONFIGURATION& config, int appId, StartMode mode, int displayWidth = 0, int displayHeight = 0);
    // Retrieve last used configuration (includes remoteInputAesKey/IV if already generated)
    bool lastStreamConfig(const std::string& address, STREAM_CONFIGURATION& out) const;

    // Pairing
    bool pair(const std::string& address, const std::string& pin);
    bool unpair(const std::string& address);
    bool isPaired(const std::string& address);
    // Complete pairing flow with popup (auto-generated PIN) replicating original behavior
    bool beginPairing(const HostInfo& host, std::function<void(bool)> onFinished, std::function<void(const std::string&)> onPinReady = nullptr);

    // Get list of apps
    void getAppList(const std::string& address, AppListCallback callback);

    // Get app cover boxart
    bool getAppBoxart(const std::string& address, int appId, Data& outData);

    // Finish application
    bool quitApp(const std::string& address);

    // Active session management
    void setActiveStream(const std::string& address, int appId, const std::string& appName);
    void clearActiveStream(const std::string& address);
    bool hasActiveStream(const std::string& address) const;
    RemoteAppInfo activeAppInfo(const std::string& address) const; // returns generic appName/icon if it exists

    // Test if an active session exists for a host (local or remote). If there is a
    // active session, fills outRunning with id/name and returns true.
    // This function encapsulates the logic of connect()+query to SERVER_DATA.currentGame
    // and app name resolution using getAppList.
    bool probeActiveSession(const HostInfo& host, RemoteAppInfo& outRunning);

    // Returns the calculated/saved keyDir for an address if it exists
    std::string getKeyDirFor(const std::string& address) const;

    // Get Sunshine's PairStatus via HTTPS request to /serverinfo
    // Returns 1 if paired, 0 if not or error
    int getSunshinePairStatus(const std::string& address);

private:
    GameStreamClient();
    ~GameStreamClient();

    bool fetchSunshineServerinfo(const std::string& address, std::string& response);
    bool parseSunshineCurrentGame(const std::string& response, int& outCurrentGame);

    std::map<std::string, SERVER_DATA> m_server_data;
    std::map<std::string, STREAM_CONFIGURATION> m_last_stream_cfg; // address -> last released config
    std::map<std::string, std::vector<RemoteAppInfo>> m_app_lists;
    std::map<std::string, std::string> m_key_dirs;
    struct ActiveStream {
        int appId;
        std::string appName;
    };
    std::map<std::string, ActiveStream> m_active_streams; // address -> active stream
    // Mark resumes in progress to prevent probeActiveSession from reopening
    // the "Active Session" dialog during the resume attempt initiated
    // from the UI. The key is the address (ip) of the host.
    std::set<std::string> m_resume_in_progress;
    // Resume attempt flags with expiration to prevent dialog reappearance
    // when the UI is recreated. Maps address -> start time_point.
    std::map<std::string, std::chrono::steady_clock::time_point> m_resume_attempts;
};

#endif // GAMESTREAM_CLIENT_HPP