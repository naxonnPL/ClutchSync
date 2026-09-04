#ifndef DEBUG_INFO_HPP
#define DEBUG_INFO_HPP

#include "pch.hpp"

#include <TlHelp32.h>
#include <Psapi.h>

inline std::atomic<bool> keepDebugging(true);
inline std::string lastStats;

inline std::string getSystemStats() {
	DWORD processID = GetCurrentProcessId();
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	if (hProcess == NULL) {
		return "Open process error";
	}

	// RAM // 

	PROCESS_MEMORY_COUNTERS_EX pmc;
	size_t ramBytes = 0;
	if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
		ramBytes = pmc.WorkingSetSize;
	}

	// THREADS //

	DWORD processThreadCount = 0;
	DWORD totalSystemThreads = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		THREADENTRY32 te;
		te.dwSize = sizeof(THREADENTRY32);
		if (Thread32First(hSnapshot, &te)) {
			do {
				totalSystemThreads++;
				if (te.th32OwnerProcessID == processID) {
					processThreadCount++;
				}
			} while (Thread32Next(hSnapshot, &te));
		}
		CloseHandle(hSnapshot);
	}

	// CPU //
	
	static ULARGE_INTEGER lastNow, lastSysCPU, lastUserCPU;
	static int numProcessors = []() {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		return sysInfo.dwNumberOfProcessors;
		}();
	static bool firstCall = true;
	
	FILETIME ftime, fsys, fuser;
	GetSystemTimeAsFileTime(&ftime);
	GetProcessTimes(hProcess, &ftime, &ftime, &fsys, &fuser);

	ULARGE_INTEGER now, sys, user;
	now.LowPart = ftime.dwLowDateTime; now.HighPart = ftime.dwHighDateTime;
	sys.LowPart = fsys.dwLowDateTime; sys.HighPart = fsys.dwHighDateTime;
	user.LowPart = fuser.dwLowDateTime; user.HighPart = fuser.dwHighDateTime;

	double cpuPercent = 0.0;
	if (!firstCall) {
		ULONGLONG systemDiff = sys.QuadPart - lastSysCPU.QuadPart;
		ULONGLONG userDiff = user.QuadPart - lastUserCPU.QuadPart;
		ULONGLONG totalProcessTime = systemDiff + userDiff;
		
		ULONGLONG timeElapsed = now.QuadPart - lastNow.QuadPart;
		
		if (timeElapsed > 0) {
			cpuPercent = (static_cast<double>(totalProcessTime) * 100.0) / timeElapsed;
			cpuPercent /= numProcessors;
		}
		else {
			firstCall = false;
		}
	}
	lastNow = now;
	lastSysCPU = sys;
	lastUserCPU = user;

	CloseHandle(hProcess);

	double ramMB = static_cast<double>(ramBytes) / (1024.0 * 1024.0);

	return std::format("Threads: {} / {} | RAM: {}MB | CPU: {}%", // C++ 20
		processThreadCount,
		totalSystemThreads,
		static_cast<int>(ramMB),
		static_cast<int>(cpuPercent));
}

inline void includeDebugInformation() {
	static std::atomic<bool> isRunning{ false };
	if (isRunning.exchange(true)) return; // only one
	std::thread([]() {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		while (keepDebugging) {
			std::string stats = getSystemStats();
			if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
				COORD originalPos = csbi.dwCursorPosition;
				SHORT lastRow = csbi.srWindow.Bottom;

				SetConsoleCursorPosition(hConsole, { 0, lastRow });
				std::cout << INFO << "\033[s"		 // save cursor position
					<< "\r[DEBUG] " << STATS << stats
					// << std::string((std::max)(0, (int)lastStats.length() - (int)stats.length()), ' ') // fill with spaces
					<< "\033[K"						 // overwrite and clear the line
					<< "\033[u"						 // back to the program
					<< RESET << std::flush;

				SetConsoleCursorPosition(hConsole, originalPos);
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		isRunning = false;
	}).detach();
}

#endif