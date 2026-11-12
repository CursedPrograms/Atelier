// Atelier launcher - language packs. English is the main language, written straight into the code; each other language
// is lang/<code>.txt next to the exe, one "English text = translation" per line. T("text") gives the translation, or
// the English when the pack doesn't have it. CJK packs say which Windows font to merge in (only for that language).
#pragma once
#include <string>
#include <vector>

namespace Lang {

struct Info {
    const char* code;      // file name: lang/<code>.txt ("en": none)
    const char* english;   // "Japanese"
    const char* native;    // "日本語" (UTF-8)
    const char* font;      // a CJK font to merge in, or nullptr
    int glyphs;            // which glyph range that font needs: 0 none, 1 Japanese, 2 Chinese, 3 Korean
};

const std::vector<Info>& all();
const Info& current();

// loads lang/<code>.txt from folder (an unknown code or a missing file: English)
void load(const std::string& folder, const std::string& code);

// the translation of an English text (the text itself when there's none)
const char* T(const char* english);

}  // namespace Lang

using Lang::T;
