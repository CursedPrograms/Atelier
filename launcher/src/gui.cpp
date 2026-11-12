// Atelier launcher - installs, updates, launches and uninstalls the apps of Atelier, Cursed Entertainment's creative
// suite, from their GitHub releases. Installed together, the apps find each other as plugins (LayerTack's
// Filter > Bathos and Filter > Mimic). SDL2 + OpenGL 3 + Dear ImGui, like LayerTack and Bathos.
//
//   Atelier
//   Atelier --capture out.bmp [--lang de] [--install LayerTack] [--uninstall LayerTack] [--splash]
//       a screenshot from a hidden window once the checks (and the install) are done, then quit;
//       --splash: the splash instead of the window
#include "lang.h"
#include "splash.h"
#include "suite.h"
#include "json.h"
#include "stb_image.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef ATELIER_VERSION
#define ATELIER_VERSION "1.0.0"
#endif

namespace fs = std::filesystem;

// ---------- colours: the Atelier look (Changeling greys, the Cursed pink; solid, no gradients, no shadows) ----------
static const ImU32 C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
                   C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255), C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255),
                   C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255), C_PINK = IM_COL32(0xe0, 0x40, 0x8a, 255),
                   C_DEEP = IM_COL32(0xc2, 0x18, 0x6b, 255), C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2; s.ScrollbarRounding = 2;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0; s.PopupBorderSize = 1; s.ChildBorderSize = 0;
    s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(7, 4); s.ItemSpacing = ImVec2(6, 7);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col4(C_TEXT); c[ImGuiCol_TextDisabled] = col4(C_DIM);
    c[ImGuiCol_WindowBg] = col4(C_PANEL); c[ImGuiCol_ChildBg] = col4(C_PANEL); c[ImGuiCol_PopupBg] = col4(IM_COL32(0x21, 0x21, 0x21, 255));
    c[ImGuiCol_Border] = col4(C_LINE); c[ImGuiCol_Separator] = col4(C_LINE);
    c[ImGuiCol_FrameBg] = col4(C_WELL); c[ImGuiCol_FrameBgHovered] = col4(IM_COL32(0x2e, 0x2e, 0x2e, 255)); c[ImGuiCol_FrameBgActive] = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_MenuBarBg] = col4(C_BG);
    c[ImGuiCol_CheckMark] = col4(C_PINK); c[ImGuiCol_SliderGrab] = col4(C_PINK); c[ImGuiCol_SliderGrabActive] = col4(C_DEEP);
    c[ImGuiCol_Button] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_ButtonHovered] = col4(C_DEEP); c[ImGuiCol_ButtonActive] = col4(C_PINK);
    c[ImGuiCol_Header] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_HeaderHovered] = col4(C_DEEP); c[ImGuiCol_HeaderActive] = col4(C_PINK);
    c[ImGuiCol_PlotHistogram] = col4(C_PINK);
    c[ImGuiCol_ScrollbarBg] = col4(C_BG); c[ImGuiCol_ScrollbarGrab] = col4(C_LINE);
    c[ImGuiCol_ScrollbarGrabHovered] = col4(C_DEEP); c[ImGuiCol_ScrollbarGrabActive] = col4(C_PINK);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
}

static bool fileExists(const char* f) {
    if (FILE* t = fopen(f, "rb")) { fclose(t); return true; }
    return false;
}

// ---------- fonts: Segoe UI (Latin, Cyrillic), the symbols, and a CJK font only for a CJK language ----------
static ImFont *headFont = nullptr, *nameFont = nullptr;

static void mergeInto(const char* file, float size, const ImWchar* ranges) {
    if (!fileExists(file)) return;
    ImFontConfig merge; merge.MergeMode = true; merge.OversampleH = 1;
    ImGui::GetIO().Fonts->AddFontFromFileTTF(file, size, &merge, ranges);
}

static ImFont* addFont(const char* file, float size, bool interfaceText) {
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar base[] = {0x0020, 0x024F, 0x0300, 0x036F, 0x0370, 0x03FF, 0x0400, 0x052F, 0x2000, 0x206F, 0x20A0, 0x20CF, 0x2190, 0x21FF, 0x2600, 0x26FF, 0};
    ImFont* font = io.Fonts->AddFontFromFileTTF(file, size, nullptr, base);
    static const ImWchar sym[] = {0x2190, 0x21FF, 0x2600, 0x26FF, 0};
    mergeInto("C:/Windows/Fonts/seguisym.ttf", size, sym);   // the credit's star, arrows
    if (!interfaceText) return font;
    const Lang::Info& L = Lang::current();
    if (L.font) {
        const ImWchar* r = L.glyphs == 1 ? io.Fonts->GetGlyphRangesJapanese() : L.glyphs == 2 ? io.Fonts->GetGlyphRangesChineseSimplifiedCommon() : io.Fonts->GetGlyphRangesKorean();
        mergeInto(L.font, size, r);
    }
    // the Language menu's own names (日本語, 中文, 한국어): just those few characters
    static const ImWchar ja[] = {0x65E5, 0x65E5, 0x672C, 0x672C, 0x8A9E, 0x8A9E, 0};
    static const ImWchar zh[] = {0x4E2D, 0x4E2D, 0x6587, 0x6587, 0};
    static const ImWchar ko[] = {0xAD6D, 0xAD6D, 0xC5B4, 0xC5B4, 0xD55C, 0xD55C, 0};
    if (L.glyphs != 1) mergeInto("C:/Windows/Fonts/YuGothM.ttc", size, ja);
    if (L.glyphs != 2) mergeInto("C:/Windows/Fonts/msyh.ttc", size, zh);
    if (L.glyphs != 3) mergeInto("C:/Windows/Fonts/malgun.ttf", size, ko);
    return font;
}

static void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    const char* f = "C:/Windows/Fonts/segoeui.ttf";
    if (!fileExists(f)) { io.Fonts->AddFontDefault(); Splash::setFonts(nullptr, nullptr); headFont = nameFont = nullptr; return; }
    addFont(f, 16.0f, true);
    const char* light = fileExists("C:/Windows/Fonts/segoeuil.ttf") ? "C:/Windows/Fonts/segoeuil.ttf" : f;
    const char* semi = fileExists("C:/Windows/Fonts/seguisb.ttf") ? "C:/Windows/Fonts/seguisb.ttf" : f;
    ImFont* title = addFont(light, 50.0f, false);
    ImFont* small = addFont(f, 13.0f, false);
    headFont = addFont(light, 34.0f, false);
    nameFont = addFont(semi, 21.0f, false);
    Splash::setFonts(title, small);
}

static std::string fromW(const wchar_t* w) { return fs::path(w).u8string(); }

static std::string fileDialog(bool save, const char* filter, const char* defExt, const std::string& start) {
    std::wstring f;
    for (const char* p = filter; *p; ++p) f += (*p == '|') ? L'\0' : (wchar_t)*p;
    f += L'\0'; f += L'\0';
    wchar_t buf[4096] = {0};
    if (!start.empty()) { std::wstring s = fs::u8path(start).wstring(); wcsncpy(buf, s.c_str(), 4095); }
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.lpstrFilter = f.c_str(); o.lpstrFile = buf; o.nMaxFile = 4096;
    std::wstring ext = defExt ? std::wstring(defExt, defExt + strlen(defExt)) : L"";
    o.lpstrDefExt = defExt ? ext.c_str() : nullptr;
    o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR) : (OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);
    const BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    return ok ? fromW(buf) : std::string();
}

static void openUrl(const std::string& url) { ShellExecuteW(nullptr, L"open", fs::u8path(url).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

static std::string fmt(const char* f, const std::string& a) { char b[1024]; snprintf(b, sizeof b, f, a.c_str()); return b; }

// ---------- state ----------
static SDL_Window* window = nullptr;
static bool quit = false, fontsDirty = false, showAbout = false;
static std::string exeDir, status;
static GLuint ownLogo = 0;
static std::map<std::string, GLuint> logos;
static std::string confirmUninstall;          // the app the Uninstall popup asks about
static std::string language = "en";

static GLuint upload(const uint8_t* rgba, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return tex;
}

// ---------- settings: the language ----------
static fs::path settingsFile() { return fs::u8path(Suite::dataDir()) / "settings.json"; }

static void loadSettings() {
    FILE* f = _wfopen(settingsFile().c_str(), L"rb");
    if (!f) return;
    std::string t; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n);
    fclose(f);
    Json::Value v;
    if (Json::parse(t, v) && !v["language"].str().empty()) language = v["language"].str();
}

static void saveSettings() {
    std::error_code ec;
    fs::create_directories(settingsFile().parent_path(), ec);
    FILE* f = _wfopen(settingsFile().c_str(), L"wb");
    if (!f) return;
    const std::string t = "{\n  \"language\": " + Json::quote(language) + "\n}\n";
    fwrite(t.data(), 1, t.size(), f);
    fclose(f);
}

static void setLanguage(const std::string& code) {
    language = code;
    Lang::load(exeDir, code);
    saveSettings();
    fontsDirty = true;
}

// ---------- actions ----------
static void pickZip() {
    const std::string p = fileDialog(false, "Release zips|*.zip|", nullptr, "");
    if (p.empty()) return;
    std::string err;
    if (!Suite::installZip(p, err)) status = err;
}

static void menuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu(T("File"))) {
        if (ImGui::MenuItem(T("Install from a zip..."), "Ctrl+O")) pickZip();
        if (ImGui::MenuItem(T("Check for updates"), "F5", false, !Suite::checking())) Suite::recheck();
        if (ImGui::MenuItem(T("Open the apps folder"))) Suite::openFolder("");
        ImGui::Separator();
        if (ImGui::MenuItem(T("Quit"), "Ctrl+Q")) quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(T("Language"))) {
        for (const auto& l : Lang::all()) {
            const std::string label = std::string(l.native) + (strcmp(l.native, l.english) ? std::string("  (") + l.english + ")" : "");
            if (ImGui::MenuItem(label.c_str(), nullptr, language == l.code || (std::string(Lang::current().code) == l.code && language == l.code))) setLanguage(l.code);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(T("Help"))) {
        if (ImGui::MenuItem(T("Atelier on GitHub"))) openUrl("https://github.com/CursedPrograms/Atelier");
        if (ImGui::MenuItem(T("About Atelier"))) showAbout = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

static void keys() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) pickZip();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) quit = true;
    if (ImGui::IsKeyPressed(ImGuiKey_F5) && !Suite::checking()) Suite::recheck();
}

// a pink button for the main action on a card
static bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, col4(C_PINK));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col4(C_DEEP));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col4(C_DEEP));
    ImGui::PushStyleColor(ImGuiCol_Text, col4(IM_COL32(255, 255, 255, 255)));
    const bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return r;
}

// the drop zone's dashed outline (Bathos's), here round a card for an app that isn't out yet
static void dashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    for (float x = a.x; x < b.x; x += 12) { dl->AddLine(ImVec2(x, a.y), ImVec2(std::min(x + 6, b.x), a.y), col, 1.5f); dl->AddLine(ImVec2(x, b.y), ImVec2(std::min(x + 6, b.x), b.y), col, 1.5f); }
    for (float y = a.y; y < b.y; y += 12) { dl->AddLine(ImVec2(a.x, y), ImVec2(a.x, std::min(y + 6, b.y)), col, 1.5f); dl->AddLine(ImVec2(b.x, y), ImVec2(b.x, std::min(y + 6, b.y)), col, 1.5f); }
}

static std::string megabytes(double b) { char s[32]; snprintf(s, sizeof s, "%.1f", b / (1024.0 * 1024.0)); return s; }

static const char* jobText(int job) {
    switch (job) {
        case Suite::J_QUEUED: return T("Waiting its turn...");
        case Suite::J_DOWNLOAD: return T("Downloading");
        case Suite::J_UNPACK: return T("Unpacking...");
        case Suite::J_COPY: return T("Installing...");
        case Suite::J_REMOVE: return T("Uninstalling...");
    }
    return "";
}

// one app: its logo, name and line, the versions, and its buttons
static void card(const Suite::App& a, float width) {
    const float h = 124, pad = 14, logoS = 92;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + width, p0.y + h);
    const bool notYet = a.release == Suite::R_NOT_YET || (a.soon && a.release != Suite::R_READY && !a.installed);
    dl->AddRectFilled(p0, p1, notYet ? C_BG : C_PANEL);
    if (notYet) dashedRect(dl, p0, p1, C_LINE); else dl->AddRect(p0, p1, C_LINE);

    // the logo (or its first letter, until it comes)
    const ImVec2 la(p0.x + pad, p0.y + (h - logoS) / 2), lb(la.x + logoS, la.y + logoS);
    const auto it = logos.find(a.name);
    if (it != logos.end()) dl->AddImage((ImTextureID)(intptr_t)it->second, la, lb, ImVec2(0, 0), ImVec2(1, 1), notYet ? IM_COL32(255, 255, 255, 150) : IM_COL32_WHITE);
    else {
        dl->AddRectFilled(la, lb, C_LINE, 14);
        const std::string ch = a.name.substr(0, 1);
        ImFont* f = headFont ? headFont : ImGui::GetFont();
        const ImVec2 ts = f->CalcTextSizeA(34, FLT_MAX, 0, ch.c_str());
        dl->AddText(f, 34, ImVec2(la.x + (logoS - ts.x) / 2, la.y + (logoS - ts.y) / 2), C_PINK, ch.c_str());
    }

    // the name, and a chip saying where it stands
    const float tx = lb.x + 18;
    float ty = p0.y + 14;
    ImFont* nf = nameFont ? nameFont : ImGui::GetFont();
    dl->AddText(nf, 21, ImVec2(tx, ty), C_TEXT, a.name.c_str());
    const float nameW = nf->CalcTextSizeA(21, FLT_MAX, 0, a.name.c_str()).x;
    const char* chip; ImU32 chipCol = C_LINE, chipText = C_TEXT;
    if (a.job != Suite::J_NONE) { chip = jobText(a.job); chipCol = C_DEEP; }
    else if (a.running) { chip = T("Running"); chipCol = C_DEEP; }
    else if (a.installed && a.updatable()) { chip = T("Update available"); chipCol = C_PINK; chipText = IM_COL32_WHITE; }
    else if (a.installed && a.release == Suite::R_READY) chip = T("Up to date");
    else if (a.installed) chip = T("Installed");
    else if (notYet) { chip = T("Coming soon"); chipText = C_PINK; }
    else chip = T("Not installed");
    const ImVec2 cs = ImGui::CalcTextSize(chip);
    const ImVec2 ca(tx + nameW + 12, ty + 4), cb(ca.x + cs.x + 14, ca.y + cs.y + 4);
    dl->AddRectFilled(ca, cb, chipCol, 3);
    dl->AddText(ImVec2(ca.x + 7, ca.y + 2), chipText, chip);
    ty += 30;

    // the buttons, on the right
    // the buttons fit the longest label (some languages run long)
    float bw = 112;
    for (const char* l : {"Install", "Update", "Reinstall", "Launch", "Console", "Uninstall", "Folder", "On GitHub"}) bw = std::max(bw, ImGui::CalcTextSize(T(l)).x + 24);
    const float bh = 30, gap = 6;
    std::vector<int> buttons;   // 0 install, 1 update, 2 reinstall, 3 launch, 4 uninstall, 5 folder, 6 GitHub
    if (a.installed) {
        if (a.launch != "none") buttons.push_back(3);
        buttons.push_back(a.updatable() ? 1 : 2);
        buttons.push_back(4);
        buttons.push_back(5);
    } else if (notYet) buttons.push_back(6);
    else buttons.push_back(0);
    const int cols = (int)std::min<size_t>(buttons.size(), 2);
    const float bx0 = p1.x - pad - cols * bw - (cols - 1) * gap;
    const int rows = (int)(buttons.size() + 1) / 2;
    const float by0 = p0.y + (h - rows * bh - (rows - 1) * gap) / 2;
    const bool idle = a.job == Suite::J_NONE;
    for (size_t i = 0; i < buttons.size(); ++i) {
        ImGui::SetCursorScreenPos(ImVec2(bx0 + (i % 2) * (bw + gap), by0 + (i / 2) * (bh + gap)));
        const int b = buttons[i];
        const ImVec2 sz(bw, bh);
        const bool blockedByRun = a.running && (b == 1 || b == 2 || b == 4);
        ImGui::BeginDisabled(!idle || blockedByRun || (b <= 2 && a.release != Suite::R_READY));
        bool hit = false;
        switch (b) {
            case 0: hit = primaryButton(T("Install"), sz); break;
            case 1: hit = primaryButton(T("Update"), sz); break;
            case 2: hit = ImGui::Button(T("Reinstall"), sz); break;
            case 3: {
                const char* l = a.launch == "console" ? T("Console") : T("Launch");
                hit = a.updatable() ? ImGui::Button(l, sz) : primaryButton(l, sz);
                if (a.launch == "console" && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", fmt(T("%s has no window: this opens a console with it ready."), a.name).c_str());
                break;
            }
            case 4: hit = ImGui::Button(T("Uninstall"), sz); break;
            case 5: hit = ImGui::Button(T("Folder"), sz); break;
            case 6: hit = ImGui::Button(T("On GitHub"), sz); break;
        }
        ImGui::EndDisabled();
        if (blockedByRun && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", fmt(T("Close %s first, then try again."), a.name).c_str());
        if (b == 5 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", a.folder.c_str());
        if (!hit) continue;
        std::string err;
        switch (b) {
            case 0: case 1: case 2: Suite::install(a.name); break;
            case 3: if (!Suite::launch(a.name, err)) status = err; break;
            case 4: confirmUninstall = a.name; break;
            case 5: Suite::openFolder(a.name); break;
            case 6: openUrl("https://github.com/" + a.repo); break;
        }
    }

    // the line, the versions, the plugin note, the job or its outcome
    const float textW = bx0 - tx - 16;
    ImGui::PushClipRect(ImVec2(tx, p0.y), ImVec2(tx + textW, p1.y), true);
    dl->AddText(ImVec2(tx, ty), C_DIM, T(a.description.c_str()));
    ty += 22;
    std::string ver;
    if (a.installed) ver = std::string(T("Installed:")) + " " + (a.version.empty() ? T("version unknown") : a.version);
    std::string latest;
    switch (a.release) {
        case Suite::R_READY: {
            const bool viaSetup = !a.setupUrl.empty() && (!a.installed || a.viaSetup);
            const double sz = viaSetup ? a.setupSize : a.size;
            latest = std::string(T("Latest:")) + " " + a.latest + (sz > 0 ? "  (" + megabytes(sz) + " MB)" : "");
            if (a.installed && a.viaSetup && a.machineWide) latest += std::string("  \xc2\xb7 ") + T("installed for everyone");
            break;
        }
        case Suite::R_CHECKING: latest = T("Asking GitHub..."); break;
        case Suite::R_NOT_YET: latest = T("Not released yet"); break;
        case Suite::R_OFFLINE: latest = std::string(T("Couldn't check:")) + " " + a.releaseError; break;
    }
    if (!ver.empty()) ver += "     ";
    ver += latest;
    dl->AddText(ImVec2(tx, ty), C_TEXT, ver.c_str());
    ty += 22;
    if (a.job == Suite::J_DOWNLOAD) {
        const float frac = a.total > 0 ? (float)(a.done / a.total) : 0.0f;
        dl->AddRectFilled(ImVec2(tx, ty + 7), ImVec2(tx + textW, ty + 11), C_WELL);
        dl->AddRectFilled(ImVec2(tx, ty + 7), ImVec2(tx + textW * frac, ty + 11), C_PINK);
        const std::string t = std::string(T("Downloading")) + " " + megabytes(a.done) + " / " + (a.total > 0 ? megabytes(a.total) : "?") + " MB";
        dl->AddText(ImVec2(tx, ty + 14), C_DIM, t.c_str());
    } else if (a.job != Suite::J_NONE) {
        // a busy bar: a pink block sliding along
        const float t = (float)fmod(ImGui::GetTime() * 0.6, 1.0), bwid = textW * 0.2f, x = tx + (textW + bwid) * t - bwid;
        dl->AddRectFilled(ImVec2(tx, ty + 7), ImVec2(tx + textW, ty + 11), C_WELL);
        dl->AddRectFilled(ImVec2(std::max(tx, x), ty + 7), ImVec2(std::min(tx + textW, x + bwid), ty + 11), C_PINK);
        dl->AddText(ImVec2(tx, ty + 14), C_DIM, jobText(a.job));
    } else if (!a.message.empty()) dl->AddText(ImVec2(tx, ty), a.error ? C_PINK : C_DIM, a.message.c_str());
    else if (!a.plugin.empty()) dl->AddText(ImVec2(tx, ty), C_DIM, T(a.plugin.c_str()));
    ImGui::PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + 10));
    ImGui::Dummy(ImVec2(width, 0));
}

static void header(ImVec2 pos, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = 72;
    if (ownLogo) dl->AddImage((ImTextureID)(intptr_t)ownLogo, pos, ImVec2(pos.x + s, pos.y + s));
    ImFont* hf = headFont ? headFont : ImGui::GetFont();
    dl->AddText(hf, 34, ImVec2(pos.x + s + 16, pos.y - 2), C_TEXT, "Atelier");
    dl->AddText(ImVec2(pos.x + s + 18, pos.y + 40), C_PINK, T("Creative & Studio Suite"));
    // the two buttons on the right
    const auto list = Suite::apps();
    int todo = 0;
    for (const auto& a : list) if (a.release == Suite::R_READY && a.job == Suite::J_NONE && !a.running && (!a.installed || a.updatable())) ++todo;
    const float bw = 150, bh = 32;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + width - bw * 2 - 8, pos.y + (s - bh) / 2));
    ImGui::BeginDisabled(Suite::checking());
    if (ImGui::Button(Suite::checking() ? T("Checking...") : T("Check for updates"), ImVec2(bw, bh))) Suite::recheck();
    ImGui::EndDisabled();
    ImGui::SameLine(0, 8);
    ImGui::BeginDisabled(todo == 0);
    if (primaryButton(T("Install all"), ImVec2(bw, bh))) Suite::installAll();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", todo ? T("Installs or updates every app that's out of date") : T("Everything is installed and up to date"));
}

static void about() {
    if (showAbout) { ImGui::OpenPopup("##about"); showAbout = false; }
    ImGui::SetNextWindowSize(ImVec2(Splash::W, Splash::H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::BeginPopupModal("##about", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        const ImVec2 p = ImGui::GetWindowPos();
        Splash::draw(ImGui::GetWindowDrawList(), p.x, p.y, T("Click to close"), -1);
        ImGui::InvisibleButton("##close", ImVec2(Splash::W, Splash::H));
        if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

static void uninstallPopup() {
    if (!confirmUninstall.empty() && !ImGui::IsPopupOpen("##uninstall")) ImGui::OpenPopup("##uninstall");
    ImGui::SetNextWindowSize(ImVec2(440, 0));
    if (ImGui::BeginPopupModal("##uninstall", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ImGui::PushFont(nameFont);
        ImGui::TextUnformatted(fmt(T("Uninstall %s?"), confirmUninstall).c_str());
        ImGui::PopFont();
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", T("Its folder and its shortcuts are removed. Your pictures and settings stay where they are."));
        ImGui::TextDisabled("%s", (fs::u8path(Suite::programsDir()) / fs::u8path(confirmUninstall)).u8string().c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        const float w = (ImGui::GetContentRegionAvail().x - 6) / 2;
        if (primaryButton(T("Uninstall"), ImVec2(w, 30))) { Suite::uninstall(confirmUninstall); confirmUninstall.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), ImVec2(w, 30)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { confirmUninstall.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

static void frame() {
    // logos that arrived
    for (const auto& a : Suite::apps()) {
        std::vector<uint8_t> px; int w, h;
        if (Suite::takeLogo(a.name, px, w, h)) {
            auto it = logos.find(a.name);
            if (it != logos.end()) glDeleteTextures(1, &it->second);
            logos[a.name] = upload(px.data(), w, h);
        }
    }
    static Uint32 lastPoll = 0;
    if (SDL_GetTicks() - lastPoll > 1500) { Suite::poll(); lastPoll = SDL_GetTicks(); }
    keys();
    menuBar();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = ImGui::GetFrameHeight(), foot = 34;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - top - foot));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    const float w = ImGui::GetContentRegionAvail().x;
    const float headY = ImGui::GetCursorPosY();
    header(ImGui::GetCursorScreenPos(), w);
    ImGui::SetCursorPosY(headY + 86);
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", T("Install the apps, keep them up to date, start them from here. Installed together, they find each other as plugins."));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##cards", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const float cw = ImGui::GetContentRegionAvail().x - (ImGui::GetScrollMaxY() > 0 ? 4 : 0);
    for (const auto& a : Suite::apps()) { ImGui::PushID(a.name.c_str()); card(a, cw); ImGui::PopID(); }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // the foot: what's happening, where the apps go, the credit
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - foot));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, foot));
    ImGui::Begin("##foot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 fp = ImGui::GetWindowPos();
    dl->AddLine(fp, ImVec2(fp.x + vp->Size.x, fp.y), C_LINE);
    std::string left;
    const std::string upd = Suite::launcherLatest();
    if (Suite::checking()) left = Suite::checkStatus();
    else if (!status.empty()) left = status;
    else left = fmt(T("Apps go in %s"), Suite::programsDir());
    ImGui::SetCursorPosY(8);
    ImGui::TextDisabled("%s", left.c_str());
    if (!upd.empty()) {
        ImGui::SameLine(0, 18);
        ImGui::PushStyleColor(ImGuiCol_Text, col4(C_PINK));
        if (ImGui::SmallButton(fmt(T("Atelier %s is out: get it"), upd).c_str())) openUrl("https://github.com/CursedPrograms/Atelier/releases/latest");
        ImGui::PopStyleColor();
    }
    const char* credit = "Atelier " ATELIER_VERSION "   \xc2\xa9 Cursed Entertainment";
    ImGui::SameLine(vp->Size.x - ImGui::CalcTextSize(credit).x - 14);
    ImGui::TextDisabled("%s", credit);
    ImGui::End();

    uninstallPopup();
    about();
}

// ---------- the splash window: shown while the launcher starts (LayerTack's) ----------
static SDL_Window* splashWin = nullptr;
static bool splashSkip = false, quitEarly = false;   // skip: clicked or Escape; quitEarly: closed (Alt+F4)

static void render(SDL_Window* win) {
    ImGui::Render();
    int w, h;
    SDL_GL_GetDrawableSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void splashSay(const char* text, float progress) {
    if (!splashWin) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) quitEarly = true;
        if (e.type == SDL_MOUSEBUTTONDOWN || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) splashSkip = true;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, text, progress);
    render(splashWin);
    SDL_GL_SwapWindow(splashWin);
}

static void saveCapture(int w, int h, const std::string& file) {
    std::vector<unsigned char> px((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    std::vector<unsigned char> flip(px.size());
    for (int y = 0; y < h; ++y) memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(flip.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
    SDL_SaveBMP(s, file.c_str());
    SDL_FreeSurface(s);
}

int main(int, char**) {
    std::string capture, forceLang, installName, uninstallName;
    bool splashShot = false;
    {   // the arguments as UTF-8 (argv is the ANSI code page)
        int n; LPWSTR* wa = CommandLineToArgvW(GetCommandLineW(), &n);
        for (int i = 1; wa && i < n; ++i) {
            const std::string a = fromW(wa[i]);
            if (a == "--capture" && i + 1 < n) capture = fromW(wa[++i]);
            else if (a == "--lang" && i + 1 < n) forceLang = fromW(wa[++i]);
            else if (a == "--install" && i + 1 < n) installName = fromW(wa[++i]);
            else if (a == "--uninstall" && i + 1 < n) uninstallName = fromW(wa[++i]);
            else if (a == "--splash") splashShot = true;
        }
        if (wa) LocalFree(wa);
    }
    char* bp = SDL_GetBasePath();
    exeDir = bp ? bp : "";
    SDL_free(bp);
    loadSettings();
    if (!forceLang.empty()) language = forceLang;
    Lang::load(exeDir, language);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // hidden until it's ready: the splash shows meanwhile (--capture never shows it)
    const int ww = splashShot ? (int)Splash::W : 1040, wh = splashShot ? (int)Splash::H : 820;
    window = SDL_CreateWindow("Atelier", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    if (!splashShot) SDL_SetWindowMinimumSize(window, 820, 560);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    SDL_Surface* icon = SDL_LoadBMP((exeDir + "assets/icon.bmp").c_str());
    if (icon) SDL_SetWindowIcon(window, icon);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    loadFont();
    applyStyle();

    // the splash: a small borderless window sharing the main window's OpenGL context
    if (capture.empty()) {
        splashWin = SDL_CreateWindow("Atelier", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)Splash::W, (int)Splash::H,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (splashWin && SDL_GL_MakeCurrent(splashWin, gl) != 0) { SDL_DestroyWindow(splashWin); splashWin = nullptr; SDL_GL_MakeCurrent(window, gl); }
        if (splashWin && icon) SDL_SetWindowIcon(splashWin, icon);
    }
    if (icon) SDL_FreeSurface(icon);
    ImGui_ImplSDL2_InitForOpenGL(splashWin ? splashWin : window, gl);
    ImGui_ImplOpenGL3_Init("#version 330");
    const Uint32 splashStart = SDL_GetTicks();
    splashSay(T("Starting..."), 0.02f);

    {   // its own logo, for the header
        int w, h, n;
        if (uint8_t* px = stbi_load((exeDir + "assets/logo.png").c_str(), &w, &h, &n, 4)) { ownLogo = upload(px, w, h); stbi_image_free(px); }
    }
    Suite::start(exeDir);
    if (!installName.empty()) Suite::install(installName);
    if (!uninstallName.empty()) Suite::uninstall(uninstallName);

    if (splashWin) {
        // the checks with GitHub run meanwhile: the splash stays until they're done (at least 1.5 s, at most 5 s)
        for (;;) {
            const Uint32 t = SDL_GetTicks() - splashStart;
            const bool busy = Suite::checking();
            if (splashSkip || quitEarly || (t >= 1500 && (!busy || t >= 5000))) break;
            const std::string s = busy ? Suite::checkStatus() : std::string(T("Ready"));
            splashSay(s.empty() ? T("Starting...") : s.c_str(), busy ? std::max(0.05f, Suite::checkProgress()) : 1.0f);
        }
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyWindow(splashWin);
        splashWin = nullptr;
        SDL_GL_MakeCurrent(window, gl);
        ImGui_ImplSDL2_InitForOpenGL(window, gl);
    }
    if (capture.empty()) {
        SDL_ShowWindow(window);
        SDL_RaiseWindow(window);
    }
    if (quitEarly) quit = true;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    int frames = 0, quiet = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_DROPFILE) {
                std::string err;
                if (!Suite::installZip(e.drop.file, err)) status = err;
                SDL_free(e.drop.file);
            }
        }
        if (fontsDirty) {   // a new language: the fonts again (a CJK one only for a CJK language)
            fontsDirty = false;
            ImGui::GetIO().Fonts->Clear();
            loadFont();
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (splashShot) Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, T("Asking GitHub about LayerTack..."), 0.45f);
        else frame();
        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        render(window);
        ++frames;
        if (!capture.empty()) {
            quiet = Suite::checking() || Suite::busy() ? 0 : quiet + 1;
            if (frames >= 8 && quiet >= 6) { saveCapture(w, h, capture); break; }
        }
        SDL_GL_SwapWindow(window);
    }

    Suite::stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
