// Atelier launcher - the suite (see suite.h)
#include "suite.h"
#include "http.h"
#include "json.h"
#include "lang.h"
#include "stb_image.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

#ifndef ATELIER_VERSION
#define ATELIER_VERSION "1.0.0"
#endif

namespace fs = std::filesystem;

namespace Suite {

static const char* CATALOG_URL = "https://raw.githubusercontent.com/CursedPrograms/Atelier/main/atelier.json";
static const char* SELF_REPO = "CursedPrograms/Atelier";

// ---------- the built-in catalog (atelier.json adds to it and can change it) ----------
static std::vector<App> builtIn() {
    std::vector<App> v;
    auto add = [&](const char* name, const char* asset, const char* desc, const char* logo, const char* plugin, bool soon) {
        App a;
        a.name = name; a.repo = std::string("CursedPrograms/") + name; a.asset = asset; a.exe = std::string(name) + ".exe";
        a.description = desc; a.logo = logo; a.plugin = plugin; a.soon = soon;
        v.push_back(a);
    };
    add("LayerTack", "LayerTack-win64.zip", "A layer-based image editor: layers, masks, blend modes and filters.",
        "https://raw.githubusercontent.com/CursedPrograms/LayerTack/main/cpp/assets/logo.png", "", false);
    add("Bathos", "Bathos-win64.zip", "A picture in, its depth map out. Near is bright, far is dark.",
        "https://raw.githubusercontent.com/CursedPrograms/Bathos/main/assets/logo.png", "In LayerTack: Filter > Bathos", false);
    add("Mimic", "Mimic-win64.zip", "One person's face onto a picture or a video.",
        "https://raw.githubusercontent.com/CursedPrograms/Mimic/main/assets/logo.png", "In LayerTack: Filter > Mimic", false);
    // a command-line tool: its own installer (per user) puts it on the PATH and adds Explorer's right-click entries
    add("CRUSH", "CRUSH-*-win64-portable.zip", "A file compressor, written from scratch, for the command line and Explorer.",
        "https://raw.githubusercontent.com/CursedPrograms/CRUSH/main/assets/logo.png", "In Explorer: right-click > Add to CRUSH archive", false);
    App& c = v.back();
    c.exe = "crush.exe"; c.setup = "CRUSH-*-setup.exe"; c.setupArgs = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER";
    c.appId = "{6E3B1F7A-2C1D-4A5B-9C0E-7F3A1B2C3D4E}"; c.launch = "console";
    add("Hymnal", "Hymnal-win64.zip", "Whole songs, with vocals, from a style: lyrics written for you, covers, remixes, stems, tempo and key.",
        "https://raw.githubusercontent.com/CursedPrograms/Hymnal/main/assets/logo.png", "", false);
    // photos to 3D: its own installer (per user, AliceVision inside); the zip works too
    add("Terminus", "Terminus-win64.zip", "Photos to a textured 3D model: AliceVision's photogrammetry in three steps, GLB, OBJ and STL out.",
        "https://raw.githubusercontent.com/CursedPrograms/Terminus/main/assets/logo.png", "", false);
    App& t = v.back();
    t.setup = "Terminus-setup.exe"; t.setupArgs = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    t.appId = "{9B6E2D4C-7A1F-4E3B-8C5D-2F0A6B1E9D73}";
    // people in 3D: MakeHuman remade; its own installer (per user, MakeHuman's assets inside); the zip works too
    add("Pygmalion", "Pygmalion-win64.zip", "People in 3D with sliders: MakeHuman remade. Shape, dress, pose, render, and export with the skeleton.",
        "https://raw.githubusercontent.com/CursedPrograms/Pygmalion/main/assets/logo.png", "", false);
    App& pg = v.back();
    pg.setup = "Pygmalion-setup.exe"; pg.setupArgs = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    pg.appId = "{30C41005-C5F8-461A-878D-A91C1BFA4ACE}";
    add("Firmament", "Firmament-win64.zip", "360° skyboxes from a prompt: look round them, repair the seam, upscale, export cube maps. Needs an NVIDIA card.",
        "https://raw.githubusercontent.com/CursedPrograms/Firmament/main/cpp/assets/logo.png", "", false);
    return v;
}

// ---------- state ----------
static std::mutex mtx;
static std::vector<App> list;
static std::string exeDir;
static std::string checkSay;
static std::atomic<float> checkProg{0};
static std::atomic<bool> checkBusy{false};
static std::string selfLatest;
static std::thread checkThread, workThread;
static std::atomic<bool> stopping{false};
static std::condition_variable wake;
struct Work { std::string name; int kind; std::string zip; };   // kind 0 install/update, 1 uninstall, 2 from a zip
static std::deque<Work> queue;
static bool working = false;
struct Logo { std::vector<uint8_t> px; int w = 0, h = 0; };
static std::map<std::string, Logo> logos;

// ---------- small helpers ----------
static std::wstring W(const std::string& s) { return fs::u8path(s).wstring(); }
static std::string U(const std::wstring& w) { return fs::path(w).u8string(); }

static std::string env(const char* name) {
    wchar_t buf[MAX_PATH * 2] = {0};
    const std::wstring n(name, name + strlen(name));
    const DWORD got = GetEnvironmentVariableW(n.c_str(), buf, MAX_PATH * 2);
    return got ? U(buf) : std::string();
}

std::string programsDir() {
    std::string la = env("LOCALAPPDATA");
    if (la.empty()) la = env("USERPROFILE") + "\\AppData\\Local";
    return (fs::u8path(la) / "Programs").u8string();
}
std::string dataDir() { return (fs::u8path(programsDir()) / "Atelier").u8string(); }
static fs::path dataPath(const std::string& rel) { return fs::u8path(dataDir()) / fs::u8path(rel); }
static std::string appFolder(const std::string& name) { return (fs::u8path(programsDir()) / fs::u8path(name)).u8string(); }

static bool readFile(const fs::path& p, std::string& out) {
    FILE* f = _wfopen(p.c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

static bool writeFile(const fs::path& p, const std::string& data) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    const fs::path tmp = p.wstring() + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (!ok) { fs::remove(tmp, ec); return false; }
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING)) { fs::remove(tmp, ec); return false; }
    return true;
}

static bool isFile(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }

static App* find(const std::string& name) {
    for (auto& a : list) if (a.name == name) return &a;
    return nullptr;
}

static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

// a file name against a pattern with * (any run of characters), ignoring case
static bool matches(const std::string& name, const std::string& pattern) {
    const std::string n = lower(name), p = lower(pattern);
    size_t i = 0, j = 0, star = std::string::npos, mark = 0;
    while (i < n.size()) {
        if (j < p.size() && p[j] == '*') { star = j++; mark = i; }
        else if (j < p.size() && p[j] == n[i]) { ++i; ++j; }
        else if (star != std::string::npos) { j = star + 1; i = ++mark; }
        else return false;
    }
    while (j < p.size() && p[j] == '*') ++j;
    return j == p.size();
}

// an Inno Setup install of the app: its uninstall entry (this user's first, then everyone's)
struct SetupEntry { std::string folder, version, uninstaller; bool machine = false; };
static bool findSetup(const std::string& appId, SetupEntry& out) {
    if (appId.empty()) return false;
    const std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + W(appId) + L"_is1";
    const struct { HKEY root; REGSAM view; bool machine; } places[] = {
        {HKEY_CURRENT_USER, 0, false}, {HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, true}, {HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, true}};
    for (const auto& pl : places) {
        HKEY k;
        if (RegOpenKeyExW(pl.root, key.c_str(), 0, KEY_READ | pl.view, &k) != ERROR_SUCCESS) continue;
        auto value = [&](const wchar_t* name) {
            wchar_t buf[2048] = {0}; DWORD sz = sizeof(buf) - sizeof(wchar_t), type = 0;
            if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &sz) != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return std::string();
            return U(buf);
        };
        out.folder = value(L"InstallLocation");
        out.version = value(L"DisplayVersion");
        out.uninstaller = value(L"UninstallString");
        out.machine = pl.machine;
        RegCloseKey(k);
        while (!out.folder.empty() && (out.folder.back() == '\\' || out.folder.back() == '/')) out.folder.pop_back();
        if (!out.uninstaller.empty() && out.uninstaller.front() == '"') out.uninstaller = out.uninstaller.substr(1, out.uninstaller.find('"', 1) - 1);
        if (!out.folder.empty()) return true;
    }
    return false;
}

int compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& s) {
        std::vector<long> v; long cur = -1;
        for (const char c : s) {
            if (c >= '0' && c <= '9') cur = (cur < 0 ? 0 : cur) * 10 + (c - '0');
            else if (cur >= 0) { v.push_back(cur); cur = -1; }
        }
        if (cur >= 0) v.push_back(cur);
        return v;
    };
    std::vector<long> x = parts(a), y = parts(b);
    const size_t n = std::max(x.size(), y.size());
    x.resize(n, 0); y.resize(n, 0);
    for (size_t i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

bool App::updatable() const {
    if (!installed || release != R_READY || latest.empty()) return false;
    if (version.empty()) return true;
    return compareVersions(version, latest) < 0;
}

// an exe the launcher didn't install: its version read from the exe itself ("Version 1.2.0" on LayerTack's
// splash, "Bathos 1.0.0" in Bathos's window), so apps installed by their own installers show up properly
static std::string sniffVersion(const fs::path& exe, const std::string& name) {
    std::string data;
    if (!readFile(exe, data)) return "";
    for (const std::string& lead : {std::string("Version "), name + " "}) {
        size_t pos = 0;
        while ((pos = data.find(lead, pos)) != std::string::npos) {
            pos += lead.size();
            size_t e = pos; int dots = 0;
            while (e < data.size() && ((data[e] >= '0' && data[e] <= '9') || (data[e] == '.' && e > pos))) { if (data[e] == '.') ++dots; ++e; }
            if (dots >= 1 && e > pos && data[e - 1] != '.') return "v" + data.substr(pos, e - pos);
        }
    }
    return "";
}

// ---------- installed.json ----------
struct Record { std::string version, folder, when; };

static std::map<std::string, Record> readInstalled() {
    std::map<std::string, Record> r;
    std::string text;
    Json::Value v;
    if (!readFile(dataPath("installed.json"), text) || !Json::parse(text, v) || !v.isObject()) return r;
    for (const auto& m : v.members) r[m.first] = {m.second["version"].str(), m.second["folder"].str(), m.second["installed"].str()};
    return r;
}

static void writeInstalled(const std::map<std::string, Record>& r) {
    std::string o = "{\n";
    size_t i = 0;
    for (const auto& m : r) {
        o += "  " + Json::quote(m.first) + ": {\"version\": " + Json::quote(m.second.version) + ", \"folder\": " +
             Json::quote(m.second.folder) + ", \"installed\": " + Json::quote(m.second.when) + "}";
        o += ++i < r.size() ? ",\n" : "\n";
    }
    o += "}\n";
    writeFile(dataPath("installed.json"), o);
}

static std::string now() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char b[32];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M", &tm);
    return b;
}

// what's on this PC, for one app (call with mtx held)
static void scanInstalled(App& a, const std::map<std::string, Record>& rec) {
    a.folder = appFolder(a.name);
    a.version.clear(); a.guessed = false; a.viaSetup = false; a.machineWide = false; a.uninstaller.clear();
    SetupEntry se;
    if (findSetup(a.appId, se) && isFile(fs::u8path(se.folder) / fs::u8path(a.exe))) {
        // its own installer put it there (maybe for everyone, in Program Files): that install is the one to update
        a.installed = true; a.viaSetup = true; a.machineWide = se.machine;
        a.folder = se.folder; a.uninstaller = se.uninstaller;
        a.version = se.version.empty() ? "" : (se.version[0] == 'v' ? se.version : "v" + se.version);
        return;
    }
    const fs::path exe = fs::u8path(a.folder) / fs::u8path(a.exe);
    a.installed = isFile(exe);
    if (!a.installed) return;
    const auto it = rec.find(a.name);
    if (it != rec.end() && !it->second.version.empty()) a.version = it->second.version;
    else { a.version = sniffVersion(exe, a.name); a.guessed = true; }
}

// ---------- the catalog ----------
static void mergeCatalog(const std::string& text) {
    Json::Value v;
    if (!Json::parse(text, v)) return;
    const Json::Value& arr = v.isArray() ? v : v["apps"];
    if (!arr.isArray()) return;
    for (const auto& e : arr.items) {
        const std::string name = e["name"].str();
        if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos || name == "Atelier") continue;
        App* a = find(name);
        if (!a) {
            App n;
            n.name = name; n.repo = "CursedPrograms/" + name; n.asset = name + "-win64.zip"; n.exe = name + ".exe";
            list.push_back(n);
            a = &list.back();
        }
        auto set = [&](const char* key, std::string& field) { if (e[key].type == Json::Value::String && !e[key].s.empty()) field = e[key].s; };
        set("repo", a->repo); set("asset", a->asset); set("exe", a->exe); set("description", a->description);
        set("logo", a->logo); set("plugin", a->plugin);
        set("setup", a->setup); set("setupArgs", a->setupArgs); set("appId", a->appId); set("launch", a->launch);
        if (e["soon"].type == Json::Value::Bool) a->soon = e["soon"].b;
    }
}

// the releases as last seen, for when GitHub can't be asked
static void loadReleaseCache() {
    std::string text; Json::Value v;
    if (!readFile(dataPath("cache/releases.json"), text) || !Json::parse(text, v)) return;
    for (auto& a : list) {
        const Json::Value& e = v[a.name];
        if (!e.isObject()) continue;
        a.latest = e["tag"].str(); a.url = e["url"].str(); a.size = e["size"].n;
        a.setupUrl = e["setup"].str(); a.setupSize = e["setupSize"].n;
        a.release = a.latest.empty() || (a.url.empty() && a.setupUrl.empty()) ? R_CHECKING : R_READY;
    }
}

static void saveReleaseCache() {
    std::string o = "{\n";
    bool first = true;
    for (const auto& a : list) {
        if (a.release != R_READY) continue;
        if (!first) o += ",\n";
        first = false;
        o += "  " + Json::quote(a.name) + ": {\"tag\": " + Json::quote(a.latest) + ", \"url\": " + Json::quote(a.url) +
             ", \"size\": " + std::to_string((long long)a.size) + ", \"setup\": " + Json::quote(a.setupUrl) +
             ", \"setupSize\": " + std::to_string((long long)a.setupSize) + "}";
    }
    o += "\n}\n";
    writeFile(dataPath("cache/releases.json"), o);
}

static bool loadLogo(const fs::path& file, const std::string& name) {
    std::string data;
    if (!readFile(file, data)) return false;
    int w, h, n;
    uint8_t* px = stbi_load_from_memory((const stbi_uc*)data.data(), (int)data.size(), &w, &h, &n, 4);
    if (!px) return false;
    Logo l; l.w = w; l.h = h; l.px.assign(px, px + (size_t)w * h * 4);
    stbi_image_free(px);
    std::lock_guard<std::mutex> g(mtx);
    logos[name] = std::move(l);
    return true;
}

bool takeLogo(const std::string& name, std::vector<uint8_t>& rgba, int& w, int& h) {
    std::lock_guard<std::mutex> g(mtx);
    const auto it = logos.find(name);
    if (it == logos.end()) return false;
    rgba = std::move(it->second.px); w = it->second.w; h = it->second.h;
    logos.erase(it);
    return true;
}

static void say(const std::string& s, float p) { { std::lock_guard<std::mutex> g(mtx); checkSay = s; } checkProg = p; }

static std::string fmt(const char* f, const std::string& a) {
    char b[512];
    snprintf(b, sizeof b, f, a.c_str());
    return b;
}

// the background check: the catalog, each app's latest release, the logos, a newer launcher
static void check() {
    say(T("Reading the catalog..."), 0.05f);
    std::string text; int status = 0;
    if (Http::get(CATALOG_URL, text, status)) {
        std::lock_guard<std::mutex> g(mtx);
        mergeCatalog(text);
        const auto rec = readInstalled();
        for (auto& a : list) if (a.folder.empty()) scanInstalled(a, rec);
        writeFile(dataPath("cache/atelier.json"), text);
    }
    std::vector<App> snap;
    { std::lock_guard<std::mutex> g(mtx); snap = list; }
    const size_t n = snap.size();
    for (size_t i = 0; i < n && !stopping; ++i) {
        const App& a = snap[i];
        say(fmt(T("Asking GitHub about %s..."), a.name), 0.1f + 0.6f * i / n);
        std::string body, err;
        const bool ok = Http::get("https://api.github.com/repos/" + a.repo + "/releases/latest", body, status, &err);
        std::lock_guard<std::mutex> g(mtx);
        App* t = find(a.name);
        if (!t) continue;
        Json::Value v;
        if (ok && Json::parse(body, v)) {
            std::string url, setupUrl; double size = 0, setupSize = 0;
            for (const auto& as : v["assets"].items) {
                const std::string an = as["name"].str();
                if (url.empty() && matches(an, t->asset)) { url = as["browser_download_url"].str(); size = as["size"].n; }
                if (setupUrl.empty() && !t->setup.empty() && matches(an, t->setup)) { setupUrl = as["browser_download_url"].str(); setupSize = as["size"].n; }
            }
            t->latest = v["tag_name"].str(); t->url = url; t->size = size; t->setupUrl = setupUrl; t->setupSize = setupSize;
            t->release = url.empty() && setupUrl.empty() ? R_NOT_YET : R_READY;
            t->releaseError.clear();
        } else if (status == 404) {
            t->release = R_NOT_YET; t->latest.clear(); t->url.clear(); t->setupUrl.clear(); t->releaseError.clear();
        } else {
            // offline, or GitHub's limit (60 questions an hour): keep what was cached
            t->releaseError = status == 403 || status == 429 ? T("GitHub is busy: try again in a while") : (err.empty() ? T("Offline") : err);
            if (t->release != R_READY) t->release = R_OFFLINE;
        }
    }
    { std::lock_guard<std::mutex> g(mtx); saveReleaseCache(); }

    // the logos: cached, or from the installed app, or from its repo (refreshed once a week)
    for (size_t i = 0; i < n && !stopping; ++i) {
        const App& a = snap[i];
        say(T("Fetching the logos..."), 0.75f + 0.2f * i / n);
        const fs::path cached = dataPath("cache/logos/" + a.name + ".png");
        std::error_code ec;
        // installed: the app's own logo, the one on this PC (newer than its repo's until it's pushed)
        if (!a.folder.empty()) {
            const fs::path own = fs::u8path(a.folder) / "assets" / "logo.png";
            if (isFile(own)) { fs::copy_file(own, cached, fs::copy_options::overwrite_existing, ec); loadLogo(isFile(cached) ? cached : own, a.name); continue; }
        }
        bool fresh = false;
        if (isFile(cached)) {
            const auto age = fs::file_time_type::clock::now() - fs::last_write_time(cached, ec);
            fresh = !ec && age < std::chrono::hours(24 * 7);
        }
        if (!fresh && !a.logo.empty()) {
            std::string png;
            if (Http::get(a.logo, png, status) && png.size() > 8 && png.compare(1, 3, "PNG") == 0) {
                writeFile(cached, png);
                loadLogo(cached, a.name);
                continue;
            }
        }
        if (isFile(cached)) loadLogo(cached, a.name);
    }

    // a newer launcher?
    std::string body;
    if (!stopping && Http::get(std::string("https://api.github.com/repos/") + SELF_REPO + "/releases/latest", body, status)) {
        Json::Value v;
        if (Json::parse(body, v)) {
            const std::string tag = v["tag_name"].str();
            std::lock_guard<std::mutex> g(mtx);
            selfLatest = !tag.empty() && compareVersions(ATELIER_VERSION, tag) < 0 ? tag : "";
        }
    }
    say(T("Ready"), 1.0f);
    checkBusy = false;
}

// ---------- running apps: any process whose exe is inside the app's folder ----------
static std::vector<std::wstring> runningExes() {
    std::vector<std::wstring> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        wchar_t path[MAX_PATH * 2]; DWORD sz = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(h, 0, path, &sz)) {
            std::wstring p(path, sz);
            for (auto& c : p) c = (wchar_t)towlower(c);
            out.push_back(p);
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
    return out;
}

static bool runningIn(const std::string& folder, const std::vector<std::wstring>& exes) {
    std::wstring f = W(folder);
    for (auto& c : f) c = (wchar_t)towlower(c);
    if (f.empty()) return false;
    if (f.back() != L'\\') f += L'\\';
    for (const auto& e : exes) if (e.compare(0, f.size(), f) == 0) return true;
    return false;
}

void poll() {
    const auto exes = runningExes();
    std::lock_guard<std::mutex> g(mtx);
    for (auto& a : list) a.running = a.installed && runningIn(a.folder, exes);
}

// ---------- shortcuts (what install.bat makes with WScript.Shell, through the same COM object underneath) ----------
static fs::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    fs::path r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) r = p;
    CoTaskMemFree(p);
    return r;
}

static std::wstring cmdExe() {
    wchar_t sys[MAX_PATH] = {0};
    GetSystemDirectoryW(sys, MAX_PATH);
    return std::wstring(sys) + L"\\cmd.exe";
}

static bool makeShortcut(const fs::path& lnk, const fs::path& target, const fs::path& dir, const std::string& desc,
                         const std::wstring& args = L"", const fs::path& iconFile = {}) {
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) return false;
    sl->SetPath(target.c_str());
    if (!args.empty()) sl->SetArguments(args.c_str());
    sl->SetWorkingDirectory(dir.c_str());
    sl->SetIconLocation(iconFile.empty() ? target.c_str() : iconFile.c_str(), 0);
    sl->SetDescription(W(desc).c_str());
    IPersistFile* pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) {
        ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

// an app with a window: <App>.lnk on the Start menu and the Desktop. A command-line tool: "<App> command prompt",
// a console with it ready (as CRUSH's own installer makes). An app its own installer put in gets none from here.
static void shortcuts(const App& a, bool make) {
    if (a.launch == "none") return;
    const fs::path exe = fs::u8path(a.folder) / fs::u8path(a.exe);
    const bool console = a.launch == "console";
    for (const auto& id : {FOLDERID_Programs, FOLDERID_Desktop}) {
        const fs::path d = knownFolder(id);
        if (d.empty()) continue;
        const fs::path lnk = d / fs::u8path(a.name + (console ? " command prompt.lnk" : ".lnk"));
        std::error_code ec;
        if (!make) { fs::remove(lnk, ec); continue; }
        if (console) makeShortcut(lnk, cmdExe(), knownFolder(FOLDERID_Documents), a.name + ": " + a.description,
                                  L"/k \"\"" + exe.wstring() + L"\" --help\"", exe);
        else makeShortcut(lnk, exe, fs::u8path(a.folder), a.name + ": " + a.description);
    }
}

// runs a program and waits for it (an installer may ask Windows for the rights it needs: ShellExecute lets it)
static bool runAndWait(const std::wstring& file, const std::wstring& args, DWORD& code, std::string& err) {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = file.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        const DWORD e = GetLastError();
        err = e == ERROR_CANCELLED ? T("Windows asked for permission and it was refused.") : std::string(T("It wouldn't start")) + " (" + std::to_string(e) + ")";
        return false;
    }
    WaitForSingleObject(sei.hProcess, INFINITE);
    code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return true;
}

// the switches for an installer: an install for everyone is updated for everyone (Windows asks first)
static std::string setupSwitches(const App& a) {
    std::string s = a.setupArgs;
    if (a.installed && a.viaSetup && a.machineWide) {
        const size_t p = s.find("/CURRENTUSER");
        if (p != std::string::npos) s.replace(p, 12, "/ALLUSERS");
    }
    return s;
}

// ---------- the jobs ----------
// what each job ended with, in launcher.log (kept short: the last 200 lines)
static void logLine(const std::string& line) {
    const fs::path f = dataPath("launcher.log");
    std::string old;
    readFile(f, old);
    std::vector<std::string> lines;
    size_t p = 0;
    while (p < old.size()) { size_t e = old.find('\n', p); if (e == std::string::npos) e = old.size(); lines.push_back(old.substr(p, e - p)); p = e + 1; }
    if (lines.size() > 199) lines.erase(lines.begin(), lines.end() - 199);
    std::string out;
    for (const auto& l : lines) out += l + "\n";
    writeFile(f, out + now() + "  " + line + "\n");
}

static void setJob(const std::string& name, int job, const std::string& msg = "", bool error = false) {
    if (job == J_NONE && !msg.empty()) logLine(name + ": " + (error ? "ERROR " : "") + msg);
    std::lock_guard<std::mutex> g(mtx);
    if (App* a = find(name)) { a->job = job; a->message = msg; a->error = error; if (job != J_DOWNLOAD) { a->done = 0; a->total = 0; } }
}

static bool runTar(const fs::path& zip, const fs::path& into, std::string& err) {
    wchar_t sys[MAX_PATH] = {0};
    GetSystemDirectoryW(sys, MAX_PATH);
    const std::wstring tar = std::wstring(sys) + L"\\tar.exe";
    std::wstring cmd = L"\"" + tar + L"\" -xf \"" + zip.wstring() + L"\" -C \"" + into.wstring() + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(tar.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        err = T("Windows' tar.exe is missing"); return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (code != 0) { err = T("The zip couldn't be unpacked"); return false; }
    return true;
}

// where the exe is in an unpacked zip: at its root (LayerTack) or in a top folder (Bathos/)
static fs::path findRoot(const fs::path& dir, const std::string& exe) {
    if (isFile(dir / fs::u8path(exe))) return dir;
    std::error_code ec;
    fs::path best;
    size_t bestDepth = 99;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file(ec) && lower(it->path().filename().u8string()) == lower(exe) && (size_t)it.depth() < bestDepth) {
            best = it->path().parent_path(); bestDepth = it.depth();
        }
    }
    return best;
}

// the unpacked files over the installed ones (a locked file is tried again a few times)
static bool copyOver(const fs::path& from, const fs::path& to, std::string& err) {
    std::error_code ec;
    fs::create_directories(to, ec);
    for (auto it = fs::recursive_directory_iterator(from, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) { err = ec.message(); return false; }
        const fs::path rel = fs::relative(it->path(), from, ec), dst = to / rel;
        if (it->is_directory(ec)) { fs::create_directories(dst, ec); continue; }
        bool ok = false;
        for (int tries = 0; tries < 5 && !ok; ++tries) {
            ok = CopyFileW(it->path().c_str(), dst.c_str(), FALSE) != 0;
            if (!ok) Sleep(300);
        }
        if (!ok) { err = std::string(T("Can't write")) + " " + dst.u8string() + " (" + std::to_string(GetLastError()) + ")"; return false; }
    }
    return true;
}

static void doInstall(const Work& w) {
    App a;
    {
        std::lock_guard<std::mutex> g(mtx);
        const App* p = find(w.name);
        if (!p) return;
        a = *p;
    }
    if (runningIn(a.folder, runningExes())) { setJob(a.name, J_NONE, fmt(T("Close %s first, then try again."), a.name), true); return; }
    const fs::path work = dataPath("downloads");
    std::error_code ec;
    fs::create_directories(work, ec);
    // the app's own installer, when it has one (a copy unpacked from a zip by hand stays a zip install)
    if (w.kind == 0 && !a.setupUrl.empty() && (!a.installed || a.viaSetup)) {
        const fs::path exe = work / fs::u8path(a.setupUrl.substr(a.setupUrl.find_last_of('/') + 1));
        setJob(a.name, J_DOWNLOAD);
        std::string err;
        const bool ok = Http::download(a.setupUrl, exe.u8string(), [&](double done, double total) {
            std::lock_guard<std::mutex> g(mtx);
            if (App* p = find(a.name)) { p->done = done; p->total = total > 0 ? total : a.setupSize; }
        }, &stopping, err);
        if (!ok) { setJob(a.name, J_NONE, std::string(T("The download failed:")) + " " + err, true); return; }
        if (runningIn(a.folder, runningExes())) { fs::remove(exe, ec); setJob(a.name, J_NONE, fmt(T("Close %s first, then try again."), a.name), true); return; }
        setJob(a.name, J_COPY);
        DWORD code = 1;
        const bool ran = runAndWait(exe.wstring(), W(setupSwitches(a)), code, err);
        fs::remove(exe, ec);
        if (!ran || code != 0) { setJob(a.name, J_NONE, ran ? std::string(T("Its installer stopped with an error")) + " (" + std::to_string(code) + ")" : err, true); return; }
        std::string version;
        {
            std::lock_guard<std::mutex> g(mtx);
            auto rec = readInstalled();
            App* p = find(a.name);
            if (p) scanInstalled(*p, rec);
            version = p && !p->version.empty() ? p->version : a.latest;
            rec[a.name] = {version, p ? p->folder : a.folder, now()};
            writeInstalled(rec);
        }
        setJob(a.name, J_NONE, fmt(T("Installed %s."), version));
        return;
    }
    fs::path zip;
    if (w.kind == 2) zip = fs::u8path(w.zip);
    else {
        if (a.url.empty()) { setJob(a.name, J_NONE, T("There's no release to download yet."), true); return; }
        zip = work / fs::u8path(a.asset);
        setJob(a.name, J_DOWNLOAD);
        std::string err;
        const bool ok = Http::download(a.url, zip.u8string(), [&](double done, double total) {
            std::lock_guard<std::mutex> g(mtx);
            if (App* p = find(a.name)) { p->done = done; p->total = total > 0 ? total : a.size; }
        }, &stopping, err);
        if (!ok) { setJob(a.name, J_NONE, std::string(T("The download failed:")) + " " + err, true); return; }
    }
    setJob(a.name, J_UNPACK);
    const fs::path unpack = work / fs::u8path(a.name + "-unpacked");
    fs::remove_all(unpack, ec);
    fs::create_directories(unpack, ec);
    std::string err;
    const auto cleanup = [&] { std::error_code e; fs::remove_all(unpack, e); if (w.kind != 2) fs::remove(zip, e); };
    if (!runTar(zip, unpack, err)) { cleanup(); setJob(a.name, J_NONE, err, true); return; }
    const fs::path root = findRoot(unpack, a.exe);
    if (root.empty()) { cleanup(); setJob(a.name, J_NONE, fmt(T("%s isn't in that zip."), a.exe), true); return; }
    // it may have been started while the download ran
    if (runningIn(a.folder, runningExes())) { cleanup(); setJob(a.name, J_NONE, fmt(T("Close %s first, then try again."), a.name), true); return; }
    setJob(a.name, J_COPY);
    if (!copyOver(root, fs::u8path(a.folder), err)) { cleanup(); setJob(a.name, J_NONE, err, true); return; }
    cleanup();
    shortcuts(a, true);
    std::string version = w.kind == 2 ? sniffVersion(fs::u8path(a.folder) / fs::u8path(a.exe), a.name) : a.latest;
    {
        std::lock_guard<std::mutex> g(mtx);
        auto rec = readInstalled();
        rec[a.name] = {version, a.folder, now()};
        writeInstalled(rec);
        if (App* p = find(a.name)) scanInstalled(*p, rec);
    }
    // its logo, if none came from GitHub
    const fs::path cached = dataPath("cache/logos/" + a.name + ".png"), own = fs::u8path(a.folder) / "assets" / "logo.png";
    if (!isFile(cached) && isFile(own)) { fs::create_directories(cached.parent_path(), ec); fs::copy_file(own, cached, ec); loadLogo(cached, a.name); }
    setJob(a.name, J_NONE, version.empty() ? T("Installed.") : fmt(T("Installed %s."), version));
}

static void doUninstall(const Work& w) {
    App a;
    {
        std::lock_guard<std::mutex> g(mtx);
        const App* p = find(w.name);
        if (!p) return;
        a = *p;
    }
    if (runningIn(a.folder, runningExes())) { setJob(a.name, J_NONE, fmt(T("Close %s first, then try again."), a.name), true); return; }
    setJob(a.name, J_REMOVE);
    std::error_code ec;
    if (a.viaSetup && isFile(fs::u8path(a.uninstaller))) {
        // its own uninstaller: it takes back what its installer did (PATH, right-click entries)
        DWORD code = 1; std::string err;
        if (!runAndWait(W(a.uninstaller), L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART", code, err)) { setJob(a.name, J_NONE, err, true); return; }
        for (int i = 0; i < 40 && isFile(fs::u8path(a.folder) / fs::u8path(a.exe)); ++i) Sleep(250);   // it finishes from a copy of itself
    } else {
        fs::remove_all(fs::u8path(a.folder), ec);
        shortcuts(a, false);
    }
    std::lock_guard<std::mutex> g(mtx);
    auto rec = readInstalled();
    rec.erase(a.name);
    writeInstalled(rec);
    App* p = find(a.name);
    if (!p) return;
    scanInstalled(*p, rec);
    p->job = J_NONE;
    p->error = !!ec || p->installed;
    p->message = p->error ? std::string(T("Some files couldn't be removed:")) + " " + a.folder : T("Uninstalled.");
    logLine(a.name + ": " + (p->error ? "ERROR " : "") + p->message);
}

static void worker() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    for (;;) {
        Work w;
        {
            std::unique_lock<std::mutex> g(mtx);
            wake.wait(g, [] { return stopping || !queue.empty(); });
            if (stopping) break;
            w = queue.front(); queue.pop_front();
            working = true;
        }
        if (w.kind == 1) doUninstall(w); else doInstall(w);
        std::lock_guard<std::mutex> g(mtx);
        working = false;
    }
    CoUninitialize();
}

static void enqueue(const Work& w) {
    {
        std::lock_guard<std::mutex> g(mtx);
        App* a = find(w.name);
        if (!a || a->job != J_NONE) return;
        a->job = J_QUEUED; a->message.clear(); a->error = false;
        queue.push_back(w);
    }
    wake.notify_one();
}

// ---------- the API ----------
void start(const std::string& folder) {
    exeDir = folder;
    {
        std::lock_guard<std::mutex> g(mtx);
        list = builtIn();
        std::string cached;
        if (readFile(dataPath("cache/atelier.json"), cached)) mergeCatalog(cached);
        loadReleaseCache();
        const auto rec = readInstalled();
        for (auto& a : list) scanInstalled(a, rec);
    }
    // logos already here show at once
    for (const auto& a : apps()) {
        const fs::path cached = dataPath("cache/logos/" + a.name + ".png"), own = fs::u8path(a.folder) / "assets" / "logo.png";
        if (!loadLogo(cached, a.name)) loadLogo(own, a.name);
    }
    poll();
    workThread = std::thread(worker);
    recheck();
}

void recheck() {
    if (checkBusy) return;
    if (checkThread.joinable()) checkThread.join();
    checkBusy = true;
    {
        std::lock_guard<std::mutex> g(mtx);
        for (auto& a : list) if (a.release != R_READY) a.release = R_CHECKING;
    }
    checkThread = std::thread(check);
}

bool checking() { return checkBusy; }
std::string checkStatus() { std::lock_guard<std::mutex> g(mtx); return checkSay; }
float checkProgress() { return checkProg; }
std::vector<App> apps() { std::lock_guard<std::mutex> g(mtx); return list; }
bool busy() { std::lock_guard<std::mutex> g(mtx); return working || !queue.empty(); }
std::string launcherLatest() { std::lock_guard<std::mutex> g(mtx); return selfLatest; }

void install(const std::string& name) {
    {
        std::lock_guard<std::mutex> g(mtx);
        App* a = find(name);
        if (!a) return;
        if (a->running) { a->message = fmt(T("Close %s first, then try again."), name); a->error = true; return; }
    }
    enqueue({name, 0, ""});
}

void installAll() {
    for (const auto& a : apps())
        if (a.release == R_READY && a.job == J_NONE && !a.running && (!a.installed || a.updatable())) enqueue({a.name, 0, ""});
}

void uninstall(const std::string& name) {
    {
        std::lock_guard<std::mutex> g(mtx);
        App* a = find(name);
        if (!a) return;
        if (a->running) { a->message = fmt(T("Close %s first, then try again."), name); a->error = true; return; }
    }
    enqueue({name, 1, ""});
}

bool installZip(const std::string& zipPath, std::string& err) {
    const std::string file = lower(fs::u8path(zipPath).filename().u8string());
    std::string match;
    for (const auto& a : apps()) {
        if (matches(file, a.asset) || file.rfind(lower(a.name), 0) == 0) { match = a.name; break; }
    }
    if (match.empty()) { err = T("That isn't a release zip of an Atelier app."); return false; }
    enqueue({match, 2, zipPath});
    return true;
}

bool launch(const std::string& name, std::string& err) {
    App a;
    {
        std::lock_guard<std::mutex> g(mtx);
        const App* p = find(name);
        if (!p) return false;
        a = *p;
    }
    const fs::path exe = fs::u8path(a.folder) / fs::u8path(a.exe);
    if (a.launch == "none") return false;
    if (a.launch == "console") {
        // a command-line tool: a console in the user's Documents with its help showing
        const std::wstring args = L"/k \"\"" + exe.wstring() + L"\" --help\"";
        const HINSTANCE r = ShellExecuteW(nullptr, L"open", cmdExe().c_str(), args.c_str(), knownFolder(FOLDERID_Documents).c_str(), SW_SHOWNORMAL);
        if ((INT_PTR)r <= 32) { err = fmt(T("%s wouldn't start."), a.name); return false; }
        return true;
    }
    const HINSTANCE r = ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, W(a.folder).c_str(), SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) { err = fmt(T("%s wouldn't start."), a.name); return false; }
    return true;
}

void openFolder(const std::string& name) {
    std::string f = programsDir();
    if (!name.empty()) {
        std::lock_guard<std::mutex> g(mtx);
        const App* a = find(name);
        f = a ? a->folder : appFolder(name);
    }
    ShellExecuteW(nullptr, L"open", W(f).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void stop() {
    stopping = true;
    wake.notify_all();
    // a check waiting on a slow server isn't waited for: it only reads, and the process is ending
    if (checkThread.joinable()) { if (checkBusy) checkThread.detach(); else checkThread.join(); }
    if (workThread.joinable()) workThread.join();
}

}  // namespace Suite
