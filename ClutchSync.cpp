// CS2 music
// [i] - information
// [Spotify API] Spotify API info / status
// [W] - warning
// [!] - error
// [CS2] - CS2 info / status

// windows
#define WIN32_LEAN_AND_MEAN

#include <WinSock2.h>
#include <windows.h>
#include <shellapi.h>
#include <wincrypt.h>

#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

// C++ standard library
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <cwchar>
#include <thread>
#include <chrono>
#include <mutex>

// third party library
#include "httplib.h"
#include "json.hpp" // nlohmann json

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib,"Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

using json = nlohmann::json;

std::mutex gsiMutex;

float setSpotifyVolume(float volume);
bool isInLobby(const json& gsiData);
bool sendSpotifyPut(const std::string& endpoint, const std::string& jsonBody = "");
void fadeVolume(float startVol, float endVol, int durationMs);
bool checkSpotifyProcess(DWORD processId);
bool spotifyPlayUri(const std::string& uri, int positionMs);
bool spotifyPause();

std::string spotifyAccessToken;
std::string spotifyRefreshToken;
std::string spotifyClientId;
std::string spotifyClientSecret;

struct AppSettings { // default
    float masterVol = 1.0f;
    float lobbyVol = 0.6f;
    float mvpVol = 1.0f;
    int fadeDurationMs = 1500;

    std::string lobbyUri = "spotify:track:1YTvnpDmqBAgld6hOjNEN1";
    int lobbyStartMs = 0;

    std::string mvpUri = "spotify:track:6zfT9uWmfX4YVXq3MU93dH";
    int mvpStartMs = 15500;
    float mvpDurationSec = 6.0f;
};

enum GameState { STATE_UNKNOWN, STATE_LOBBY, STATE_FREEZETIME, STATE_LIVE, STATE_OVER };
GameState currentState = STATE_UNKNOWN;

float originalSpotifyVolume = 1.0f;
float currentSpotifyVolume = 1.0f;
int myLastMvpCount = -1;

/*
enum ExitCode {
    SUCCESS = 0,
    TOKEN_ERROR = -1,
    COM_ERROR = -2,
    PORT_ERROR = -3,
};
*/
AppSettings config;

bool loadSettings() {
    std::ifstream file("settings.json");
    if (!file.is_open()) {
        std::cout << "[CONFIG] Config settings.json not found! Loading defaults...\n";
        return false;
    }
    try {
        json j;
        file >> j;

        if (j.contains("volume") && j["volume"].is_object()) {
            auto vol = j["volume"];
            if (vol.contains("master") && vol["master"].is_number()) {
                config.masterVol = vol["master"].get<float>() / 100.0f;
            }
            if (vol.contains("lobby") && vol["lobby"].is_number()) {
                config.lobbyVol = vol["lobby"].get<float>() / 100.0f;
            }
            if (vol.contains("mvp") && vol["mvp"].is_number()) {
                config.mvpVol = vol["mvp"].get<float>() / 100.0f;
            }
        }

        // fading
        if (j.contains("fading") && j["fading"].is_object()) {
            auto fading = j["fading"];
            if (fading.contains("fade_duration_ms") && fading["fade_duration_ms"].is_number_integer()) {
                config.fadeDurationMs = fading["fade_duration_ms"].get<int>();
            }
        }
        // lobby
        if (j.contains("tracks") && j["tracks"].is_object()) {
            auto tracks = j["tracks"];

            if (tracks.contains("lobby") && tracks["lobby"].is_object()) {
                auto lobbyNode = tracks["lobby"];
                if (lobbyNode.contains("uri") && lobbyNode["uri"].is_string()) {
                    config.lobbyUri = lobbyNode["uri"].get<std::string>();
                }
                if (lobbyNode.contains("start_seconds") && lobbyNode["start_seconds"].is_number()) {
                    config.lobbyStartMs = static_cast<int>(lobbyNode["start_seconds"].get<double>() * 1000.0);
                }
            }

        // mvp
        if (tracks.contains("mvp") && tracks["mvp"].is_object()) {
            auto mvpNode = tracks["mvp"];
            if (mvpNode.contains("uri") && mvpNode["uri"].is_string()) {
                config.mvpUri = mvpNode["uri"].get<std::string>();
            }
            if (mvpNode.contains("start_seconds") && mvpNode["start_seconds"].is_number()) {
                config.mvpStartMs = static_cast<int>(mvpNode["start_seconds"].get<double>() * 1000.0);
            }
            if (mvpNode.contains("duration_seconds") && mvpNode["duration_seconds"].is_number()) {
                config.mvpDurationSec = mvpNode["duration_seconds"].get<float>();
            }
        }
    }

        std::cout << "[CONFIG] Settings loaded!\n";
        std::cout << "[CONFIG] Loaded values -> Master: " << (config.masterVol * 100)
            << "%, Lobby: " << (config.lobbyVol * 100)
            << "%, MVP: " << (config.mvpVol * 100) << "%\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[!] Error parsing settings.json: " << e.what() << "Using defaults.\n";
        return false;
    }
}

bool loadToken() {
    std::ifstream f("token.json");
    if (!f.is_open()) {
        std::cerr << "[!] No token.json file found!" << std::endl;
        return false;
    }

    try {
        json data = json::parse(f);
        spotifyClientId = data.value("client_id", "");
        spotifyClientSecret = data.value("client_secret", "");
        spotifyAccessToken = data.value("spotify_token", "");
        spotifyRefreshToken = data.value("refresh_token", "");
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[!] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

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

bool spotifyPause() {
    if (sendSpotifyPut("/v1/me/player/pause", "{}")) {
        std::cout << "[Spotify API] Playback paused.\n";
        return true;
    }
    return false;
}

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

void handleGameState(const json& j) { // "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo\cfg\gamestate_integration_ClutchSync.cfg"
    try {
        if (isInLobby(j)) {
            if (currentState != STATE_LOBBY) {
                currentState = STATE_LOBBY;
                std::cout << "[CS2] Lobby / menu\n";
                spotifyPlayUri(config.lobbyUri, config.lobbyStartMs);
                fadeVolume(currentSpotifyVolume, config.lobbyVol, config.fadeDurationMs);
            }
            return;
        }

        if (j.contains("round") && j["round"].is_object() && j["round"].contains("phase")) {
            std::string roundPhase = j["round"].value("phase", "");

            if (roundPhase == "freezetime") {
                if (currentState != STATE_FREEZETIME) {
                    currentState = STATE_FREEZETIME;
                    std::cout << "[CS2] Shopping time -> music turned down\n";
                    spotifyPlayUri(config.lobbyUri, config.lobbyStartMs);
                    fadeVolume(currentSpotifyVolume, config.lobbyVol * 0.4f, 1000);
                }
            }

            else if (roundPhase == "live") {
                if (currentState != STATE_LIVE) {
                    currentState = STATE_LIVE;
                    std::cout << "[CS2] Round start -> music muted & paused\n";
                    std::thread([=]() {
                        fadeVolume(currentSpotifyVolume, 0.0f, 800);
                        std::this_thread::sleep_for(std::chrono::milliseconds(800));
                        if (currentState == STATE_LIVE) {
                            spotifyPause();
                        }
                        }).detach();
                }
            }
            else if (roundPhase == "over") {
                if (currentState != STATE_OVER) {
                    currentState = STATE_OVER;
                    std::cout << "[CS2] End of the round -> Checking MVP / music ON\n";

                    bool gotMvp = false;
                    if (j.contains("player") && j["player"].contains("match_stats") && j["player"]["match_stats"].contains("mvps")) {
                        int currentMvps = j["player"]["match_stats"].value("mvps", 0);

                        if (j.contains("previously") && j["previously"].contains("player") &&
                            j["previously"]["player"].contains("match_stats") &&
                            j["previously"]["player"]["match_stats"].contains("mvps")) {
                            int prevMvps = j["previously"]["player"]["match_stats"].value("mvps", 0);
                            if (currentMvps > prevMvps) gotMvp = true;
                        }
                        else if (myLastMvpCount != -1 && currentMvps > myLastMvpCount) {
                            gotMvp = true;
                        }
                        myLastMvpCount = currentMvps;

                        if (gotMvp) {
                            std::cout << "You got MVP! Playing: " << config.mvpUri << " at " << config.mvpStartMs << ".\n";
                            setSpotifyVolume(0.0f);

                            spotifyPlayUri(config.mvpUri, config.mvpStartMs);
                            setSpotifyVolume(config.mvpVol);

                            std::thread([=]() {
                                int duration = static_cast<int>(config.mvpDurationSec * 1000);
                                if (duration > 1000) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(duration - 1000));
                                }
                                fadeVolume(config.mvpVol, 0.0f, 1000);
                                spotifyPause();
                                }).detach();
                        }
                        else {
                            std::cout << "[CS2] Someone else got MVP...\n";
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[!] GSI JSON exception caught: " << e.what() << "\n";
    }
}

bool isInLobby(const json& gsiData) {
    if (!gsiData.contains("map") || gsiData.value("player", json::object()).value("activity", "") == "menu") {
        return true;
    }
    return false;
}

void fadeVolume(float startVol, float endVol, int durationMs) {
    std::thread([=]() {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            std::cerr << "[!] [FadeVolume()] CoInitializeEx failed: 0x" << std::hex << hr << std::dec << "\n";
            int steps = 20;
            int delay = durationMs / steps;
            float stepSize = (endVol - startVol) / static_cast<float>(steps);
            for (int i = 0; i <= steps; ++i) {
                float calculatedVol = startVol + (stepSize * i);
                setSpotifyVolume(calculatedVol);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
            setSpotifyVolume(endVol);
            return;
        }
        int steps = 20;
        int delay = durationMs / steps;
        float stepSize = (endVol - startVol) / static_cast<float>(steps);

        for (int i = 0; i <= steps; ++i) {
            float calculatedVol = startVol + (stepSize * i);
            setSpotifyVolume(calculatedVol);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        setSpotifyVolume(endVol);
        CoUninitialize(); // API clear
        }).detach();
}

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

float setSpotifyVolume(float newVolume) {
    newVolume = std::clamp(newVolume, 0.0f, 1.0f);
    float previousVolume = currentSpotifyVolume;
    bool success = false;

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

bool startServer() { // GSI server
    httplib::Server svr;

    svr.Post("/", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::lock_guard<std::mutex> lock(gsiMutex);
            handleGameState(j);
        }
        catch (const std::exception& e) {}
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
        CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
        setSpotifyVolume(originalSpotifyVolume);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        CoUninitialize();
        return TRUE;
    }
    return FALSE;
}

int main()
{
    std::cout << "=== Music for CS2 ===\n";
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "[!] Error: Couldn't initialize COM library.\n";
        system("pause");
        return -2;
    }

    SetConsoleCtrlHandler(consoleHandler, TRUE);

    loadSettings();
    if (!loadToken()) {
        std::cerr << "[!] Error with token read.\n";
        CoUninitialize();
        system("pause");
        return -1;
    }
    checkAndStartSpotify();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    originalSpotifyVolume = setSpotifyVolume(config.lobbyVol);
    std::cout << "[i] Saved original Spotify volume: " << (originalSpotifyVolume * 100.0f) << "%\n";

    if (!startServer()) {
        std::cerr << "[!] Port error! Read warning.\n";
        CoUninitialize();
        return -3;
    }
    CoUninitialize();
    return 0;
}

