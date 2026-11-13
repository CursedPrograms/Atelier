// Atelier launcher - the suite: the catalog of apps, their latest releases on GitHub, what's installed on this PC,
// and the jobs that install, update and uninstall them (one at a time, on a worker thread).
//
// The catalog is a built-in list plus atelier.json from the Atelier repo (fetched each start, cached), so new apps
// can be added without a new launcher. Apps go in %LOCALAPPDATA%\Programs\<App>, where their own installers put
// them and where Chimera looks for bathos-cli / mimic-cli; what the launcher installed is remembered in
// %LOCALAPPDATA%\Programs\Atelier\installed.json.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Suite {

enum Release { R_CHECKING, R_READY, R_NOT_YET, R_OFFLINE };    // not yet: no release (or no repo) on GitHub
enum Job { J_NONE, J_QUEUED, J_DOWNLOAD, J_UNPACK, J_COPY, J_REMOVE };

struct App {
    // from the catalog
    // asset and setup are names or patterns with * ("CRUSH-*-win64-portable.zip"), so new versions match
    std::string name, repo, asset, exe, description, logo, plugin;
    std::string setup, setupArgs, appId;   // an installer (Inno Setup) to prefer over the zip, its silent switches, its AppId
    std::string launch = "window";         // "window", "console" (a command-line tool: a console in its folder), "none"
    bool soon = false;                 // the catalog says it isn't out yet
    std::vector<std::string> categories;   // "Images", "3D", "Games", "Audio", "Video", "AI", "Tools": the launcher's filters
    int order = 1000;                  // where it stands in the list: the big apps first
    // from GitHub
    int release = R_CHECKING;
    std::string latest, url, setupUrl, releaseError;
    double size = 0, setupSize = 0;
    // on this PC
    bool installed = false, guessed = false, running = false;   // guessed: the version was read from the exe
    bool viaSetup = false, machineWide = false;                  // installed by its own installer (for everyone)
    std::string version, folder, uninstaller;
    // the job on it
    int job = J_NONE;
    double done = 0, total = 0;
    std::string message;
    bool error = false;

    bool updatable() const;            // installed, and the release is newer (or the installed version unknown)
};

void start(const std::string& exeFolder);   // reads what's installed and the cached catalog; checks GitHub in the background
void recheck();                             // asks GitHub again
bool checking();                            // the background check is still going
std::string checkStatus();                  // what it's doing ("Asking GitHub about Chimera...")
float checkProgress();
std::vector<App> apps();                    // a copy, for drawing
void poll();                                // which apps are running (call now and then)
bool busy();                                // a job is going or waiting
std::string launcherLatest();               // a newer launcher on GitHub ("" when this one is the newest)

void install(const std::string& name);      // or update, or reinstall
void installAll();                          // everything that's out and not up to date
void uninstall(const std::string& name);
bool installZip(const std::string& zipPath, std::string& err);   // a release zip picked or dropped by hand
bool launch(const std::string& name, std::string& err);
void openFolder(const std::string& name);   // "" : the Programs folder
void stop();                                // stops a download and waits for the threads

// a logo that arrived (RGBA): taken once
bool takeLogo(const std::string& name, std::vector<uint8_t>& rgba, int& w, int& h);

std::string programsDir();                  // %LOCALAPPDATA%\Programs
std::string dataDir();                      // %LOCALAPPDATA%\Programs\Atelier
int compareVersions(const std::string& a, const std::string& b);   // "v1.2.0" vs "1.10": -1, 0, 1

}  // namespace Suite
