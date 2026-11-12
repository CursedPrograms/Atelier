// Atelier launcher - HTTPS through WinHTTP: small answers into memory (GitHub's API, atelier.json, logos), and
// downloads to a file with progress. Redirects (GitHub's release assets) are followed. User-Agent: Atelier-Launcher.
#pragma once
#include <atomic>
#include <functional>
#include <string>

namespace Http {

// status: the HTTP status (0: no connection). true on 200.
bool get(const std::string& url, std::string& body, int& status, std::string* err = nullptr);

// to a file, through <file>.part; progress(done, total) (total 0 when unknown). cancel stops it.
bool download(const std::string& url, const std::string& file, const std::function<void(double, double)>& progress,
              const std::atomic<bool>* cancel, std::string& err);

}
