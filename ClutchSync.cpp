// ClutchSync
// [i] - information
// [Spotify API] Spotify API info / status
// [W] - warning
// [!] - error
// [CS2] - CS2 info / status

// windows
#define WIN32_LEAN_AND_MEAN

#pragma warning(push,0)

#include <WinSock2.h> // windows sockets
#include <windows.h> // windows API
#include <shellapi.h> // windows shell
#include <wincrypt.h> // encryption for Spotify API
#include <tlhelp32.h>
#include <mmdeviceapi.h> // multimedia devices
#include <audiopolicy.h> // audio

// C++ standard library
#include <fstream> // files (e.g. settings.json and token.json)
#include <iostream> // input/output
#include <string> // 
#include <algorithm>
#include <cwchar> // wide characters for windows API
#include <thread> // multithreading for multiple functions to work simultaneously
#include <chrono> // time
#include <mutex>
#include <atomic> // multi thread safety
#include <cstdio> // FILE, _popen/popen/_pclose/pclose for curl.exe (used in sendSpotifyGet())

// third party library
#include "httplib.h"
#include "json.hpp" // nlohmann json

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib,"Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

#pragma warning(pop)

/* -------------------------------------------------------------------------------- */

using json = nlohmann::json;
std::mutex gsiMutex;

float setSpotifyVolume(float volume);
bool isInLobby(const json& gsiData);
bool sendSpotifyPut(const std::string& endpoint, const std::string& jsonBody = "");
void fadeVolume(float startVol, float endVol, int durationMs, int targetState);
bool checkSpotifyProcess(DWORD processId);
bool spotifyPlayUri(const std::string& uri, int positionMs);
bool spotifyPause();
bool sendSpotifyGet(const std::string& endpoint, std::string& responseOutput);



// settings.json:
std::string spotifyAccessToken;
std::string spotifyRefreshToken;
std::string spotifyClientId;
std::string spotifyClientSecret;

// music duration Spotify API:
std::atomic<int> g_LobbyMusicDuration{ 0 }; // data from ExitCode checkMusicLength()
std::atomic<int> g_MVPMusicDuration{ 0 }; // data from ExitCode checkMusicLength()

// Default settings if settings.json won't work
struct AppSettings { // default

    // volume
    float lobbyVol = 0.6f;
    float mvpVol = 1.0f;
    float freezeTimeVol = 0.3f;
    float roundStartVol = 0.4f;
    
    // fading
    int fadeDurationMs = 1500;

    // lobby
    std::string lobbyUri = "spotify:track:1YTvnpDmqBAgld6hOjNEN1";
    int lobbyStartMilliseconds = 0;

    // mvp
    std::string mvpUri = "spotify:track:6zfT9uWmfX4YVXq3MU93dH";
    int mvpStartMilliseconds = 15500;
    float mvpDurationSeconds = 6.0f;

    // freezeTime
    std::string freezeTimeUri = "spotify:track:4Fv6wNYUixnYkj3Dgfrls8";
    int freezeTimeStartMilliseconds = 15000;

    // roundStart
    std::string roundStartUri = "spotify:track:53AkM8aF3lSu1nShMOj418";
    int roundStartStartMilliseconds = 10200;
    float roundStartDurationSeconds = 5.0f;
};

// CS2 GSI STATE
enum GameState {
    STATE_UNKNOWN,
    STATE_LOBBY,
    STATE_FREEZETIME,
    STATE_LIVE,
    STATE_OVER
};
std::atomic<GameState> currentState{ STATE_UNKNOWN };


float originalSpotifyVolume = 1.0f; // to restore the volume level from before the program was launched
std::atomic<float> currentSpotifyVolume{1.0f};
int myLastMvpCount = -1; // MVP count for handleGameState()
static std::string lastRoundPhase = ""; // for handleGameState()

// return error codes:
enum ExitCode {
    SUCCESS = 0,
    TOKEN_ERROR = -1,
    COM_ERROR = -2,
    PORT_ERROR = -3,
    NO_LOBBY_MUSIC_ID = -4,
    NO_MVP_MUSIC_ID = -5,
    JSON_MUSIC_LENGTH_PARSING_ERROR = -6,

    // FOR TOKEN.JSON
    NO_TOKEN_JSON = -7,
    NO_SPOTIFY_ID_TOKEN = -8,
    JSON_TOKEN_PARSING_ERROR = -9,

    // FOR SETTINGS.JSON
    NO_SETTINGS_JSON = -10,
    JSON_SETTINGS_PARSING_ERROR = -11,

    VOLUME_CONFIG_ERROR = -12,
    VOLUME_LOBBY_ERROR = -13,
    VOLUME_MVP_ERROR = -14,
    VOLUME_FREEZETIME_ERROR = -15,
    VOLUME_ROUNDSTART_ERROR = -16,

    FADING_CONFIG_ERROR = -17,
    FADE_DURATION_MS_ERROR = -18,

    TRACKS_CONFIG_ERROR = -19,

    TRACKS_LOBBY_CONFIG_ERROR = -20,
    TRACKS_LOBBY_URI_ERROR = -21,
    TRACKS_LOBBY_START_SECONDS_ERROR = -22,

    TRACKS_MVP_CONFIG_ERROR = -23,
    TRACKS_MVP_URI_ERROR = -24,
    TRACKS_MVP_START_SECONDS_ERROR = -25,
    TRACKS_MVP_DURATION_SECONDS_ERROR = -26,

    TRACKS_FREEZETIME_CONFIG_ERROR = -27,
    TRACKS_FREEZETIME_URI_ERROR = -28,
    TRACKS_FREEZETIME_START_SECONDS_ERROR = -29,

    TRACKS_ROUNDSTART_CONFIG_ERROR = -30,
    TRACKS_ROUNDSTART_URI_ERROR = -31,
    TRACKS_ROUNDSTART_START_SECONDS_ERROR = -32,
    TRACKS_ROUNDSTART_DURATION_SECONDS_ERROR = -33,

};

AppSettings config;

// JSON:

ExitCode loadSettings() {
    std::ifstream file("settings.json");
    if (!file.is_open()) {
        std::cout << "[CONFIG] Config settings.json not found! Loading defaults...\n";
        return NO_SETTINGS_JSON;
    }
    try {
        json j;
        file >> j;
        // volume -> int / lobby / mvp / freezeTime / roundStart
        // fading -> int fade_duration_ms
        // tracks: {
        //  mvp -> uri / start_seconds / duration_seconds
        //  freezeTime -> uri / start_seconds / duration_seconds
        //  roundStart -> uri / start_seconds / duration_seconds
        // }
        if (j.contains("volume") && j["volume"].is_object()) {
            auto vol = j["volume"];
            
            if (vol.contains("lobby") && vol["lobby"].is_number()) {
                config.lobbyVol = vol["lobby"].get<float>() / 100.0f;
            }
            else { return VOLUME_LOBBY_ERROR; }

            if (vol.contains("mvp") && vol["mvp"].is_number()) {
                config.mvpVol = vol["mvp"].get<float>() / 100.0f;
            }
            else { return VOLUME_MVP_ERROR; }

            if (vol.contains("freezeTime") && vol["freezeTime"].is_number()) {
                config.freezeTimeVol = vol["freezeTime"].get<float>() / 100.0f;
            } 
            else { return VOLUME_FREEZETIME_ERROR; }

            if (vol.contains("roundStart") && vol["roundStart"].is_number()) {
                config.roundStartVol = vol["roundStart"].get<float>() / 100.0f;
            } 
            else { return VOLUME_ROUNDSTART_ERROR; }

        } else { return VOLUME_CONFIG_ERROR; }

        // fading
        if (j.contains("fading") && j["fading"].is_object()) {
            auto fading = j["fading"];
            if (fading.contains("fade_duration_ms") && fading["fade_duration_ms"].is_number_integer()) {
                config.fadeDurationMs = fading["fade_duration_ms"].get<int>();
            } 
            else { return FADE_DURATION_MS_ERROR; } 

        } else { return FADING_CONFIG_ERROR; }

        // lobby
        if (j.contains("tracks") && j["tracks"].is_object()) {
            auto tracks = j["tracks"];

            if (tracks.contains("lobby") && tracks["lobby"].is_object()) {
                auto lobbyNode = tracks["lobby"];
                if (lobbyNode.contains("uri") && lobbyNode["uri"].is_string()) {
                    config.lobbyUri = lobbyNode["uri"].get<std::string>();
                }
                else { return TRACKS_LOBBY_URI_ERROR; }

                if (lobbyNode.contains("start_seconds") && lobbyNode["start_seconds"].is_number()) {
                    config.lobbyStartMilliseconds = static_cast<int>(lobbyNode["start_seconds"].get<double>() * 1000.0);
                }
                else { return TRACKS_LOBBY_START_SECONDS_ERROR; }

            }
            else { return TRACKS_LOBBY_CONFIG_ERROR; }

            // mvp
            if (tracks.contains("mvp") && tracks["mvp"].is_object()) {
                auto mvpNode = tracks["mvp"];
                if (mvpNode.contains("uri") && mvpNode["uri"].is_string()) {
                    config.mvpUri = mvpNode["uri"].get<std::string>();
                }
                else { return TRACKS_MVP_URI_ERROR; }

                if (mvpNode.contains("start_seconds") && mvpNode["start_seconds"].is_number()) {
                    config.mvpStartMilliseconds = static_cast<int>(mvpNode["start_seconds"].get<double>() * 1000.0);
                }
                else { return TRACKS_MVP_START_SECONDS_ERROR; }

                if (mvpNode.contains("duration_seconds") && mvpNode["duration_seconds"].is_number()) {
                    config.mvpDurationSeconds = mvpNode["duration_seconds"].get<float>();
                }
                else { return TRACKS_MVP_DURATION_SECONDS_ERROR; }

            } else { return TRACKS_MVP_CONFIG_ERROR; }

            // freezeTime
            if (tracks.contains("freezeTime") && tracks["freezeTime"].is_object()) {
                auto freezeTimeNode = tracks["freezeTime"];
                if (freezeTimeNode.contains("uri") && freezeTimeNode["uri"].is_string()) {
                    config.freezeTimeUri = freezeTimeNode["uri"].get<std::string>();
                }
                else { return TRACKS_FREEZETIME_URI_ERROR; }

                if (freezeTimeNode.contains("start_seconds") && freezeTimeNode["start_seconds"].is_number()) {
                    config.freezeTimeStartMilliseconds = static_cast<int>(freezeTimeNode["start_seconds"].get<double>() * 1000.0);
                }
                else { return TRACKS_FREEZETIME_START_SECONDS_ERROR; }

            }
            else { return TRACKS_FREEZETIME_CONFIG_ERROR; }

            // roundStart
            if (tracks.contains("roundStart") && tracks["roundStart"].is_object()) {
                auto roundStartNode = tracks["roundStart"];
                if (roundStartNode.contains("uri") && roundStartNode["uri"].is_string()) {
                    config.roundStartUri = roundStartNode["uri"].get<std::string>();
                }
                else { return TRACKS_ROUNDSTART_URI_ERROR; }

                if (roundStartNode.contains("start_seconds") && roundStartNode["start_seconds"].is_number()) {
                    config.roundStartStartMilliseconds = static_cast<int>(roundStartNode["start_seconds"].get<double>() * 1000.0);
                }
                else { return TRACKS_ROUNDSTART_START_SECONDS_ERROR; }

                if (roundStartNode.contains("duration_seconds") && roundStartNode["duration_seconds"].is_number()) {
                    config.roundStartDurationSeconds = roundStartNode["duration_seconds"].get<float>();
                }
                else { return TRACKS_ROUNDSTART_DURATION_SECONDS_ERROR; }

            } else { return TRACKS_ROUNDSTART_CONFIG_ERROR; }
        }
        else { return TRACKS_CONFIG_ERROR; }

        std::cout << "[CONFIG] Settings loaded!\n";
        std::cout << "[CONFIG] Loaded values -> "<< "Lobby: " << (config.lobbyVol * 100)
                  << "%, MVP: " << (config.mvpVol * 100) << "%\n";
        return SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "[!] Error parsing settings.json: " << e.what() << "Using defaults.\n";
        return JSON_SETTINGS_PARSING_ERROR;
    }
}

ExitCode loadToken() {
    std::ifstream f("token.json");
    if (!f.is_open()) {
        std::cerr << "[!] No token.json file found!\n";
        return NO_TOKEN_JSON;
    }
    try {
        json data = json::parse(f);
        spotifyClientId = data.value("client_id", "");
        spotifyClientSecret = data.value("client_secret", "");
        spotifyAccessToken = data.value("spotify_token", "");
        spotifyRefreshToken = data.value("refresh_token", "");
        if (spotifyClientId == "PUT_HERE_CLIENT_ID" || spotifyClientSecret == "PUT_HERE_CLIENT_SECRET" || spotifyRefreshToken == "PUT_HERE_REFRESH_TOKEN" || spotifyAccessToken == "PUT_HERE_SPOTIFY_TOKEN") {
            std::cout << "You didn't paste all your Spotify Token ID's. Read README.md. ClutchSync won't work without it.\nYou must have Spotify Premium.\n";
            return NO_SPOTIFY_ID_TOKEN;
        }
        return SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "[!] JSON parse error: " << e.what() << "\n";
        return JSON_TOKEN_PARSING_ERROR;
    }
}

/*
std::string getJsonStringSafe(const json& parent, const std::string& key) {
    if (!parent.contains(key) || parent[key].is_null()) return "";
    try {
        if (parent[key].is_string()) return parent[key].get<std::string>();
        if (parent[key].is_number_integer() || parent[key].is_number_unsigned()) {
            return std::to_string(parent[key].get<uint64_t>());
        }
        if (parent[key].is_number_float()) {
            return std::to_string(parent[key].get<double>());
        }
    }
    catch (...) {
        return "";
    }
    return "";
}
*/

std::string base64Encode(const std::string& in) {
    DWORD outLen = 0;

    CryptBinaryToStringA(reinterpret_cast<const BYTE*>(in.data()), static_cast<DWORD>(in.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);

    std::string out(outLen, '\0');
    CryptBinaryToStringA(reinterpret_cast<const BYTE*>(in.data()), static_cast<DWORD>(in.size()),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);

    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

bool refreshSpotifyToken() {
    if (spotifyRefreshToken.empty() || spotifyClientId.empty() || spotifyClientSecret.empty()) {
        std::cerr << "[!] No data to enable token refresh in token.json!\n";
        return false;
    }

    std::string authStr = spotifyClientId + ":" + spotifyClientSecret;
    std::string base64Auth = base64Encode(authStr);

    std::string cmd = "curl.exe -s -X POST \"https://accounts.spotify.com/api/token\" "
        "-H \"Authorization: Basic " + base64Auth + "\" "
        "-H \"Content-Type: application/x-www-form-urlencoded\" "
        "-d \"grant_type=refresh_token&refresh_token=" + spotifyRefreshToken + "\"";

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[256];
    std::string response = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        response += buffer;
    }
    _pclose(pipe);

    try {
        auto j = json::parse(response);
        if (j.contains("access_token")) {
            spotifyAccessToken = j["access_token"].get<std::string>();
            std::cout << "[Spotify API] Token successfully refreshed!\n";
        }
        if (j.contains("refresh_token")) {
            spotifyRefreshToken = j["refresh_token"].get<std::string>();
        }

        json data; // saving new token to token.json
        data["spotify_token"] = spotifyAccessToken;
        data["refresh_token"] = spotifyRefreshToken;
        data["client_id"] = spotifyClientId;
        data["client_secret"] = spotifyClientSecret;

        std::ofstream out("token.json");
        out << data.dump(4);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[!] Parsing error of token response: " << e.what() << "\n";
    }
    return false;
}


// SPOTIFY API:

bool sendSpotifyGet(const std::string& endpoint, std::string& responseOutput) {
    std::string url = "https://api.spotify.com" + endpoint;

    std::string command = "curl.exe -s -H \"Authorization: Bearer " + spotifyAccessToken + "\" \"" + url + "\"";

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        std::cout << "[!] Couldn't open curl.exe\n";
        return false;
    }
    char buffer[128];
    responseOutput.clear();
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        responseOutput += buffer;
    }
#ifdef _WIN32
    int returnCode = _pclose(pipe);
#else
    int returnCode = pclose(pipe);
#endif
    if (returnCode != 0) {
        std::cout << "[!] curl.exe returned an error: " << returnCode << "\n";
        return false;
    }
    if (responseOutput.empty()) {
        std::cout << "[!] curl.exe got an empty response\n";
        return false;
    }
    return true;
}

ExitCode checkMusicLength(const std::string& lobbyTrack, const std::string& MVPTrack, std::atomic<int>& g_LobbyMusicDuration, std::atomic<int>& g_MVPMusicDuration /* later const std::string& TenSecondRoundStart */) {
    // check if track ID are empty
    if (lobbyTrack.empty()) return NO_LOBBY_MUSIC_ID;
    if (MVPTrack.empty()) return NO_MVP_MUSIC_ID;

    // checking length of lobbyTrack
    std::string LOBBYendpoint = "/v1/tracks/" + lobbyTrack;
    std::string LobbyLengthResponse;

    if (sendSpotifyGet(LOBBYendpoint, LobbyLengthResponse)) {
        try {
            json jsonLOBBYlength = json::parse(LobbyLengthResponse);

            // checking length of MVPTrack
            std::string MVPendpoint = "/v1/tracks/" + MVPTrack;
            std::string MVPLengthResponse;

            g_LobbyMusicDuration.store(jsonLOBBYlength.value("duration_ms", 90000)); // if music keeps repeating after 1min 30s, that means fallback data is used (90000ms)
            if (sendSpotifyGet(MVPendpoint, MVPLengthResponse)) {
                try {
                    json MVPlength = json::parse(MVPLengthResponse);
                    // everything okay
                    g_MVPMusicDuration.store(MVPlength.value("duration_ms", 10000));
                    return SUCCESS;
                }
                // MVP music error
                catch (const json::parse_error& errorMVP) {
                    std::cout << "[!] Parsing error with MVP Music Length. Error code: " << errorMVP.what() << "\n";
                }
            }
        }
        // Lobby music error
        catch (const json::parse_error& errorLobby) {
            std::cout << "[!] Parsing error with Lobby Music Length. Error code: " << errorLobby.what() << "\n";
        }
    }
    // any error
    return JSON_MUSIC_LENGTH_PARSING_ERROR;
}

bool sendSpotifyPut(const std::string& endpoint, const std::string& jsonBody) {
    std::string url = "https://api.spotify.com" + endpoint;
    std::string cmd = "curl.exe -s -o NUL -w \"%{http_code}\" -X PUT \"" + url + "\" "
        "-H \"Authorization: Bearer " + spotifyAccessToken + "\" "
        "-H \"Content-Type: application/json\" ";
    if (!jsonBody.empty()) {
        std::string escapedBody = jsonBody;
        size_t pos = 0;
        while ((pos = escapedBody.find("\"", pos)) != std::string::npos) {
            escapedBody.replace(pos, 1, "\\\"");
            pos += 2;
        }
        cmd += "-d \"" + escapedBody + "\"";
    }
    else {
        cmd += "-d \"\"";
    }

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;

    }
    _pclose(pipe);

    int statusCode = 0;
    try { statusCode = std::stoi(result); }
    catch (...) {}
    if (statusCode == 401) {
        std::cout << "Token expired. Refreshing...\n";
        if (refreshSpotifyToken()) {
            return sendSpotifyPut(endpoint, jsonBody);
        }
    }
    if (statusCode == 204 || statusCode == 200) { // error 204 - No Content = success
        std::cout << "[Spotify API] Request success (" << endpoint << ") HTTP: " << statusCode << "\n";
        return true;
    }
    else {
        std::cerr << "[Spotify API] Error on endpoint: " << endpoint << " | status: " << statusCode << "\n";
        if (statusCode) std::cerr << "[Spotify API] Responce body: " << statusCode << "\n";
        return false;
    }
}

bool spotifyPlayUri(const std::string& uri, int positionMs) {
    if (uri.empty()) return false;

    json body = {
        {"uris", json::array({uri})},
        {"position_ms", positionMs}
    };
    std::string endpoint = "/v1/me/player/play";
    if (sendSpotifyPut(endpoint, body.dump())) {
        std::cout << "[Spotify API] Playing track: " << uri << " at " << positionMs << " ms.\n";
        return true;
    }
    return false;
}

void setSpotifyRepeat(const std::string& state) {
    std::string endpoint = "/v1/me/player/repeat?state=" + state;
    sendSpotifyPut(endpoint);
}

bool spotifyPause() {
    if (sendSpotifyPut("/v1/me/player/pause", "{}")) {
        std::cout << "[Spotify API] Playback paused.\n";
        return true;
    }
    return false;
}

void fadeVolume(float startVol, float endVol, int durationMs, int targetState) {
    std::thread([=]() {
        bool comInitialized = false;
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr)) {
            comInitialized = true;
        }
        int steps = 20;
        int delay = durationMs / steps;
        float stepSize = (endVol - startVol) / static_cast<float>(steps);

        for (int i = 0; i <= steps; ++i) {
            if (currentState.load() != targetState) {
                if (comInitialized) CoUninitialize();
                return;
            }

            float calculatedVol = startVol + (stepSize * i);
            setSpotifyVolume(calculatedVol);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        setSpotifyVolume(endVol);
        if (comInitialized) CoUninitialize(); // API clear
        }).detach();
}


// CS2 GSI:

bool isInLobby(const json& gsiData) {
    if (!gsiData.contains("map") || gsiData.value("player", json::object()).value("activity", "") == "menu") {
        return true;
    }
    return false;
}

void handleGameState(const json& j) { // "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo\cfg\gamestate_integration_ClutchSync.cfg"
    try {
        
        // music duration
        /*
        std::thread(checkMusicLength, config.lobbyUri, config.mvpUri, std::ref(g_LobbyMusicDuration), std::ref(g_MVPMusicDuration)).detach();
        */

        if (isInLobby(j)) {
            if (currentState != STATE_LOBBY) {
                currentState.store(STATE_LOBBY);
                std::cout << "[CS2] Lobby / menu\n";
                setSpotifyRepeat("track");
                spotifyPlayUri(config.lobbyUri, config.lobbyStartMilliseconds);
                fadeVolume(currentSpotifyVolume.load(), config.lobbyVol, config.fadeDurationMs, STATE_LOBBY);
            }
            return;
        }

        if (j.contains("round") && j["round"].is_object() && j["round"].contains("phase")) {
            std::string roundPhase = j["round"].value("phase", "");

            if (roundPhase == "freezetime") {
                if (currentState.load() != STATE_FREEZETIME) {
                    currentState.store(STATE_FREEZETIME);
                    std::cout << "[CS2][STATE_FREEZETIME] Shopping time.\n";
                    spotifyPlayUri(config.freezeTimeUri, config.freezeTimeStartMilliseconds);
                    setSpotifyRepeat("track");
                    fadeVolume(currentSpotifyVolume.load(), config.lobbyVol * 0.4f, config.fadeDurationMs, STATE_FREEZETIME);
                }
            }
            else if (roundPhase == "live") {
                if (currentState.load() != STATE_LIVE) {
                    bool cameFromFreeze = (lastRoundPhase == "freezetime");
                    currentState.store(STATE_LIVE);

                    if (cameFromFreeze) {
                        std::cout << "[CS2] Round start -> Playing roundStart music/\n";
                        std::thread([=]() {  // roundStart music when round starts
                            spotifyPlayUri(config.roundStartUri, config.roundStartStartMilliseconds);
                            setSpotifyVolume(config.roundStartVol);

                            int roundStartMusicDuration = static_cast<int>(config.roundStartDurationSeconds * 1000.0f);
                            std::this_thread::sleep_for(std::chrono::milliseconds(roundStartMusicDuration));

                            fadeVolume(config.roundStartVol, 0.0f, config.fadeDurationMs, STATE_LIVE);
                            std::this_thread::sleep_for(std::chrono::milliseconds(config.fadeDurationMs));
                            if (currentState.load() == STATE_LIVE) {
                                spotifyPause();
                            }
                        }).detach();
                    }
                    else {
                        std::cout << "[CS2] Round start (mid-game join) -> music muted & paused\n";
                        std::thread([=]() {
                            fadeVolume(currentSpotifyVolume.load(), 0.0f, config.fadeDurationMs, STATE_LIVE);
                            std::this_thread::sleep_for(std::chrono::milliseconds(config.fadeDurationMs));
                            if (currentState == STATE_LIVE) {
                                spotifyPause();
                            }
                        }).detach();
                    }
                }
            }
            else if (roundPhase == "over") {
                if (currentState != STATE_OVER) {
                    currentState = STATE_OVER;
                    std::cout << "[CS2] End of the round -> Checking MVP...\n";

                    bool gotMvp = false;

                    std::string mySteamID = "";
                    if (j.contains("provider") && j["provider"].contains("steamid")) {
                        mySteamID = j["provider"].value("steamid", "");
                    }
                    if (j.contains("player") && j["player"].is_object()) {
                        std::string playerSteamID = j["player"].value("steamid", "");

                        if (playerSteamID == mySteamID && j["player"].contains("match_stats")) {
                            int currentMvps = j["player"]["match_stats"].value("mvps", 0);

                            if (myLastMvpCount != -1 && currentMvps > myLastMvpCount) {
                                gotMvp = true;
                            }
                            myLastMvpCount = currentMvps;
                        }
                    }
                    if (gotMvp) {
                        std::cout << "[CS2] You got MVP! Playing: " << config.mvpUri << ".\n";
                        setSpotifyVolume(0.0f);
                        spotifyPlayUri(config.mvpUri, config.mvpStartMilliseconds);
                        setSpotifyVolume(config.mvpVol);

                        std::thread([=]() {
                            int duration = static_cast<int>(config.mvpDurationSeconds * 1000);

                            if (duration > config.fadeDurationMs) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(duration - config.fadeDurationMs));
                            }

                            if (currentState.load() == STATE_OVER) {
                                fadeVolume(config.mvpVol, 0.0f, config.fadeDurationMs, STATE_OVER);
                                if (currentState.load() == STATE_OVER) {
                                    spotifyPause();
                                }
                            }
                                
                        }).detach();
                    }
                }
                else {
                    std::cout << "[CS2] Someone else got MVP...\n";
                }
            }
            if (!roundPhase.empty()) {
                lastRoundPhase = roundPhase;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] GSI JSON exception caught: " << e.what() << "\n";
    }
}


// WINDOWS & SPOTIFY APP:

bool checkSpotifyProcess(DWORD processId = 0) { // program start check
    bool exists = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"Spotify.exe") == 0) {
                    if (processId == 0 || pe.th32ProcessID == processId) {
                        exists = true;
                        break;
                    }
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return exists;
}

void checkAndStartSpotify() {
    if (checkSpotifyProcess()) return;
    std::cout << "[!] Spotify is not running. Do you want to launch it? (Y/n): ";
    std::string input;
    std::getline(std::cin, input);

    size_t first = input.find_first_not_of(" \t\n\r");
    if (first != std::string::npos) {
        size_t last = input.find_last_not_of(" \t\n\r");
        input = input.substr(first, (last - first + 1));
    }
    else {
        input.clear();
    }

    if (input.empty() || std::tolower(static_cast<unsigned char>(input[0])) == 'y') {
        std::cout << "[i] Launching spotify...\n";
        ShellExecuteW(NULL, L"open", L"spotify:", NULL, NULL, SW_SHOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
    else {
        std::cout << "[i] Skipping...\n";
    }
}

float setSpotifyVolume(float newVolume) {
    newVolume = std::clamp(newVolume, 0.0f, 1.0f);
    float previousVolume = currentSpotifyVolume;


    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager2* pSessionManager = NULL;
    IAudioSessionEnumerator* pSessionEnum = NULL;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (SUCCEEDED(hr)) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (SUCCEEDED(hr)) {
            hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&pSessionManager)); // session manager
            if (SUCCEEDED(hr)) {
                hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
                if (SUCCEEDED(hr)) {

                    int sessionCount = 0;
                    pSessionEnum->GetCount(&sessionCount);

                    for (int i = 0; i < sessionCount; i++) {
                        IAudioSessionControl* pSessionControl = NULL;
                        IAudioSessionControl2* pSessionControl2 = NULL;
                        if (SUCCEEDED(pSessionEnum->GetSession(i, &pSessionControl))) {
                            if (SUCCEEDED(pSessionControl->QueryInterface(IID_PPV_ARGS(&pSessionControl2)))) {
                                DWORD processId = 0;
                                pSessionControl2->GetProcessId(&processId);

                                if (checkSpotifyProcess(processId)) {
                                    ISimpleAudioVolume* pSimpleAudioVolume = NULL;
                                    if (SUCCEEDED(pSessionControl2->QueryInterface(IID_PPV_ARGS(&pSimpleAudioVolume)))) {
                                        float volFetch = 1.0f;
                                        if (SUCCEEDED(pSimpleAudioVolume->GetMasterVolume(&volFetch))) {
                                            previousVolume = volFetch;
                                        }

                                        pSimpleAudioVolume->SetMasterVolume(newVolume, NULL);
                                        pSimpleAudioVolume->Release();
                                        currentSpotifyVolume = newVolume;
                                        pSessionControl2->Release();
                                        pSessionControl->Release();
                                        break;
                                    }
                                }
                                pSessionControl2->Release();
                            }
                            pSessionControl->Release();
                        }
                    }
                }
            }
        }
    }

    if (pSessionEnum) pSessionEnum->Release();
    if (pSessionManager) pSessionManager->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();

    return previousVolume;
}


// SERVER & CONSOLE HANDLER & INT MAIN():

bool startServer() { // GSI server
    httplib::Server svr;

    svr.Post("/", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::lock_guard<std::mutex> lock(gsiMutex);
            handleGameState(j);
        }
        catch (const std::exception& e) { std::cout << "[!] JSON Parsing error code: " << e.what() << " \n"; }
        res.status = 200;
        res.set_content("OK", "text/plain");
        });
    std::cout << "[i] Server online on port 6767...\n";

    if (!svr.listen("127.0.0.1", 6767)) {
        std::cout << "[W] If the application doesn't work, use \"netstat - ano | findstr 127.0.0.1\" and write in \"void startServer()\": svr.listen(\"127.0.0.1\", ENTER_FREE_PORT_HERE);\", which doesn't appear in console.\nAlso don't forget to change port in \"gamestate_integration_ClutchSync.cfg\".\nBoth ports must be the same.\n";
        return false;
    }
    return true;
}

BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
        std::cout << "\n[!] Cleanup: Restoring original Spotify volume (" << (originalSpotifyVolume * 100.0f) << "%)...\n";
        (void)CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
        setSpotifyVolume(originalSpotifyVolume);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        CoUninitialize();
        return TRUE;
    }
    return FALSE;
}

int main()
{
    std::cout << "=== ClutchSync by naxonn ===\n";
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "[!] Error: Couldn't initialize COM library.\n";
        system("pause");
        return COM_ERROR;
    }
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    ExitCode loadSettingsStatus = loadSettings();
    if (loadSettingsStatus != SUCCESS) {
        std::cerr << "Error code: " << loadSettingsStatus << ".\n";
        config = AppSettings();
    }

    std::cout << "settings.json loaded.\n";
    ExitCode loadTokenStatus = loadToken();
    if (loadTokenStatus != SUCCESS) {
        std::cerr << "[!] Error with token read.\nError code: " << loadTokenStatus << ".\n";
        CoUninitialize();
        system("pause");
        return loadTokenStatus;
    }
    std::cout << "token.json loaded.\n";

    checkAndStartSpotify();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    originalSpotifyVolume = setSpotifyVolume(config.lobbyVol);
    std::cout << "[i] Saved original Spotify volume: " << (originalSpotifyVolume * 100.0f) << "%\n";

    if (!startServer()) {
        std::cerr << "[!] Port error! Read warning.\n";
        CoUninitialize();
        return PORT_ERROR;
    }

    CoUninitialize();
    return 0;
}

