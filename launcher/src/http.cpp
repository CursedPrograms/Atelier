// Atelier launcher - HTTPS through WinHTTP (see http.h)
#include "http.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace Http {

static std::wstring wide(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string lastError(const char* what) {
    return std::string(what) + " (error " + std::to_string(GetLastError()) + ")";
}

// one request, opened and sent; the caller reads the body and closes the three handles
struct Request {
    HINTERNET session = nullptr, connect = nullptr, request = nullptr;
    ~Request() {
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
    }
    bool open(const std::string& url, int& status, std::string& err) {
        status = 0;
        const std::wstring wurl = wide(url);
        URL_COMPONENTS u{};
        u.dwStructSize = sizeof(u);
        wchar_t host[256] = {0}, path[4096] = {0};
        u.lpszHostName = host; u.dwHostNameLength = 255;
        u.lpszUrlPath = path; u.dwUrlPathLength = 4095;
        wchar_t extra[2048] = {0};
        u.lpszExtraInfo = extra; u.dwExtraInfoLength = 2047;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &u)) { err = lastError("Bad address"); return false; }
        session = WinHttpOpen(L"Atelier-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) session = WinHttpOpen(L"Atelier-Launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) { err = lastError("No internet"); return false; }
        WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);
        connect = WinHttpConnect(session, host, u.nPort, 0);
        if (!connect) { err = lastError("Can't reach the server"); return false; }
        const std::wstring target = std::wstring(path) + extra;
        request = WinHttpOpenRequest(connect, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (!request) { err = lastError("Can't make the request"); return false; }
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
        const wchar_t* headers = url.find("api.github.com") != std::string::npos ? L"Accept: application/vnd.github+json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
        if (!WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
            err = lastError("No answer from the server"); return false;
        }
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        status = (int)code;
        return true;
    }
    double length() {
        wchar_t buf[64] = {0}; DWORD sz = sizeof(buf);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, buf, &sz, WINHTTP_NO_HEADER_INDEX))
            return _wtof(buf);
        return 0;
    }
};

bool get(const std::string& url, std::string& body, int& status, std::string* err) {
    body.clear();
    Request r;
    std::string e;
    if (!r.open(url, status, e)) { if (err) *err = e; return false; }
    std::vector<char> buf(64 * 1024);
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(r.request, buf.data(), (DWORD)buf.size(), &got)) { if (err) *err = lastError("Reading stopped"); return false; }
        if (!got) break;
        body.append(buf.data(), got);
        if (body.size() > 32u * 1024 * 1024) { if (err) *err = "Answer too big"; return false; }
    }
    if (status != 200 && err) *err = "The server said " + std::to_string(status);
    return status == 200;
}

bool download(const std::string& url, const std::string& file, const std::function<void(double, double)>& progress,
              const std::atomic<bool>* cancel, std::string& err) {
    Request r;
    int status = 0;
    if (!r.open(url, status, err)) return false;
    if (status != 200) { err = "The server said " + std::to_string(status); return false; }
    const double total = r.length();
    const fs::path out = fs::u8path(file), part = fs::u8path(file + ".part");
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    FILE* f = _wfopen(part.c_str(), L"wb");
    if (!f) { err = "Can't write " + part.u8string(); return false; }
    std::vector<char> buf(256 * 1024);
    double done = 0;
    bool ok = true;
    for (;;) {
        if (cancel && *cancel) { err = "Stopped"; ok = false; break; }
        DWORD got = 0;
        if (!WinHttpReadData(r.request, buf.data(), (DWORD)buf.size(), &got)) { err = lastError("The download broke off"); ok = false; break; }
        if (!got) break;
        if (fwrite(buf.data(), 1, got, f) != got) { err = "The disk is full?"; ok = false; break; }
        done += got;
        if (progress) progress(done, total);
    }
    fclose(f);
    if (ok && total > 0 && done < total) { err = "The download was cut short"; ok = false; }
    if (!ok) { fs::remove(part, ec); return false; }
    fs::remove(out, ec);
    fs::rename(part, out, ec);
    if (ec) { err = "Can't rename the download: " + ec.message(); return false; }
    return true;
}

}  // namespace Http
