// Atelier launcher - language packs (see lang.h)
#include "lang.h"

#include <cstdio>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Lang {

static const std::vector<Info> langs = {
    {"en", "English", "English", nullptr, 0},
    {"ja", "Japanese", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "C:/Windows/Fonts/YuGothM.ttc", 1},
    {"zh", "Chinese", "\xE4\xB8\xAD\xE6\x96\x87", "C:/Windows/Fonts/msyh.ttc", 2},
    {"es", "Spanish", "Espa\xC3\xB1ol", nullptr, 0},
    {"ko", "Korean", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", "C:/Windows/Fonts/malgun.ttf", 3},
    {"fr", "French", "Fran\xC3\xA7" "ais", nullptr, 0},
    {"de", "German", "Deutsch", nullptr, 0},
    {"ru", "Russian", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9", nullptr, 0},
    {"sr", "Serbian", "\xD0\xA1\xD1\x80\xD0\xBF\xD1\x81\xD0\xBA\xD0\xB8", nullptr, 0},
    {"af", "Afrikaans", "Afrikaans", nullptr, 0},
    {"yua", "Yucatec Maya", "Maaya t'aan", nullptr, 0},
    {"pdc", "Pennsylvania Dutch", "Deitsch", nullptr, 0},
};

static size_t cur = 0;
static std::unordered_map<std::string, std::string> table;

const std::vector<Info>& all() { return langs; }
const Info& current() { return langs[cur]; }

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// "\n" in a pack is a new line
static std::string unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { o += '\n'; ++i; }
        else o += s[i];
    }
    return o;
}

void load(const std::string& folder, const std::string& code) {
    table.clear();
    cur = 0;
    for (size_t i = 0; i < langs.size(); ++i) if (code == langs[i].code) cur = i;
    if (cur == 0) return;
    const fs::path file = fs::u8path(folder) / "lang" / (std::string(langs[cur].code) + ".txt");
    FILE* f = _wfopen(file.c_str(), L"rb");
    if (!f) { cur = 0; return; }
    std::string text;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    fclose(f);
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text.erase(0, 3);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        const size_t eq = t.find(" = ");
        if (eq == std::string::npos) continue;
        const std::string key = unescape(trim(t.substr(0, eq))), value = unescape(trim(t.substr(eq + 3)));
        if (!key.empty() && !value.empty()) table[key] = value;
    }
}

const char* T(const char* english) {
    if (table.empty()) return english;
    const auto it = table.find(english);
    return it == table.end() ? english : it->second.c_str();
}

}  // namespace Lang
