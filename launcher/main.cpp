// RLBounceLauncher.exe
//
// One-click launcher for the Bounce Checker overlay:
//   1. Checks that BakkesMod is installed.
//   2. Copies BounceChecker.dll (shipped next to this exe) into BakkesMod's plugins folder.
//   3. Adds "plugin load bouncechecker" to BakkesMod's plugins.cfg so it auto-loads.
//   4. Starts BakkesMod (if it isn't already running).
//   5. Launches Rocket League through Steam or Epic.
//
// Usage:  RLBounceLauncher.exe            (auto-detect Steam / Epic)
//         RLBounceLauncher.exe --steam
//         RLBounceLauncher.exe --epic
//         RLBounceLauncher.exe --install-only

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const wchar_t* kPluginDll = L"BounceChecker.dll";
const char* kLoadLine = "plugin load bouncechecker";
const wchar_t* kSteamUri = L"steam://rungameid/252950";
const wchar_t* kEpicUri = L"com.epicgames.launcher://apps/9773aa1aa54f4f7b80e44bef04986cea%3A530145df28a24424923f5828cc9031a1%3ASugar?action=launch&silent=true";

void Info(const std::wstring& s) { std::wcout << L"[+] " << s << std::endl; }
void Warn(const std::wstring& s) { std::wcout << L"[!] " << s << std::endl; }

fs::path ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

fs::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

std::wstring RegString(HKEY root, const wchar_t* key, const wchar_t* value) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) return buf;
    return L"";
}

bool IsProcessRunning(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = true; break; }
    }
    CloseHandle(snap);
    return found;
}

// %APPDATA%\bakkesmod\bakkesmod  - where BakkesMod keeps plugins and cfg files.
fs::path BakkesDataDir() { return KnownFolder(FOLDERID_RoamingAppData) / L"bakkesmod" / L"bakkesmod"; }

fs::path FindBakkesExe() {
    std::vector<fs::path> candidates;
    // The BakkesMod installer records its folder here.
    std::wstring reg = RegString(HKEY_CURRENT_USER, L"Software\\BakkesMod\\AppPath", L"BakkesModPath");
    if (!reg.empty()) candidates.push_back(fs::path(reg) / L"BakkesMod.exe");
    candidates.push_back(KnownFolder(FOLDERID_ProgramFiles) / L"BakkesMod" / L"BakkesMod.exe");
    candidates.push_back(KnownFolder(FOLDERID_ProgramFilesX86) / L"BakkesMod" / L"BakkesMod.exe");
    candidates.push_back(BakkesDataDir().parent_path() / L"BakkesMod.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (!c.empty() && fs::exists(c, ec)) return c;
    }
    return {};
}

bool InstallPlugin(const fs::path& dataDir) {
    const fs::path src = ExeDir() / kPluginDll;
    if (!fs::exists(src)) {
        Warn(L"BounceChecker.dll not found next to the launcher: " + src.wstring());
        return false;
    }
    std::error_code ec;
    fs::create_directories(dataDir / L"plugins", ec);
    fs::create_directories(dataDir / L"cfg", ec);
    const fs::path dst = dataDir / L"plugins" / kPluginDll;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        // Most likely the DLL is loaded by a running game. That's fine if it's already the same file.
        Warn(L"Could not overwrite plugin (is Rocket League running?). Keeping the existing copy.");
    } else {
        Info(L"Plugin installed to " + dst.wstring());
    }

    // Make BakkesMod load the plugin automatically.
    const fs::path cfg = dataDir / L"cfg" / L"plugins.cfg";
    bool present = false;
    {
        std::ifstream in(cfg);
        std::string line;
        while (std::getline(in, line))
            if (line.find(kLoadLine) != std::string::npos) present = true;
    }
    if (!present) {
        std::ofstream out(cfg, std::ios::app);
        out << "\n" << kLoadLine << "\n";
        Info(L"Added auto-load entry to plugins.cfg");
    }
    return true;
}

bool LaunchUri(const wchar_t* uri) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool HasProtocolHandler(const wchar_t* scheme) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, scheme, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

void PauseIfDoubleClicked() {
    // When launched from Explorer the console closes instantly - keep messages visible.
    DWORD pids[2];
    if (GetConsoleProcessList(pids, 2) <= 1) {
        std::wcout << L"\nPress Enter to close..." << std::endl;
        std::wstring dummy;
        std::getline(std::wcin, dummy);
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleTitleW(L"Rocket League Bounce Checker Launcher");
    std::wcout << L"=== Rocket League Bounce Checker ===\n"
               << L"Red line = predicted ball path, red boxes = where it bounces.\n"
               << L"Works in Freeplay, Custom Training and offline matches.\n\n";

    enum class Platform { Auto, Steam, Epic, None } platform = Platform::Auto;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--steam") platform = Platform::Steam;
        else if (a == L"--epic") platform = Platform::Epic;
        else if (a == L"--install-only") platform = Platform::None;
    }

    const fs::path dataDir = BakkesDataDir();
    const fs::path bakkesExe = FindBakkesExe();
    if (bakkesExe.empty() && !fs::exists(dataDir)) {
        Warn(L"BakkesMod is not installed. Get it from https://bakkesplugins.com/ or https://bakkesmod.com/ and run this again.");
        LaunchUri(L"https://bakkesmod.com/");
        PauseIfDoubleClicked();
        return 1;
    }

    if (!InstallPlugin(dataDir)) {
        PauseIfDoubleClicked();
        return 1;
    }

    if (platform == Platform::None) {
        Info(L"Install complete.");
        PauseIfDoubleClicked();
        return 0;
    }

    if (IsProcessRunning(L"BakkesMod.exe")) {
        Info(L"BakkesMod already running.");
    } else if (!bakkesExe.empty()) {
        ShellExecuteW(nullptr, L"open", bakkesExe.c_str(), nullptr, bakkesExe.parent_path().c_str(), SW_SHOWMINNOACTIVE);
        Info(L"Started BakkesMod.");
    } else {
        Warn(L"Couldn't find BakkesMod.exe - please start BakkesMod yourself.");
    }

    if (IsProcessRunning(L"RocketLeague.exe")) {
        Info(L"Rocket League is already running. In the game press F6 and type:  plugin load bouncechecker");
    } else {
        if (platform == Platform::Auto)
            platform = HasProtocolHandler(L"steam") ? Platform::Steam : Platform::Epic;
        const bool ok = LaunchUri(platform == Platform::Steam ? kSteamUri : kEpicUri);
        if (ok) Info(platform == Platform::Steam ? L"Launching Rocket League via Steam..." : L"Launching Rocket League via Epic Games...");
        else Warn(L"Couldn't launch Rocket League automatically - start it yourself.");
    }

    std::wcout << L"\nIn game: open Freeplay or Custom Training. Toggle the overlay with F6 -> bounce_toggle\n"
               << L"(or bind a key:  bind F3 bounce_toggle). Tweak with F6 -> bounce_seconds, bounce_max_bounces ...\n";
    PauseIfDoubleClicked();
    return 0;
}
