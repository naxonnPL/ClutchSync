#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib,"Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

// windows
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

#define RESET "\033[38;5;255m" // default one
#define RED "\033[31m" // error
#define GREEN "\033[32m" // spotify info
#define YELLOW "\033[33m" // CS R info / warning
#define BLUE "\033[34m" // CS L info
#define INFO "\033[38;5;245m" // for debug.hpp
#define PINK "\033[38;5;200m"
#define CYAN "\033[38;5;87m" // config
#define STATS "\033[38;5;240m" // for debug.hpp