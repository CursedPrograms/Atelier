// Atelier launcher - the JSON reader (see json.h)
#include "json.h"
#include <cstdio>
#include <cstdlib>

namespace Json {

static const Value nothing;

const Value& Value::operator[](const std::string& key) const {
    if (type == Object)
        for (const auto& m : members) if (m.first == key) return m.second;
    return nothing;
}

const Value& Value::operator[](size_t i) const { return type == Array && i < items.size() ? items[i] : nothing; }

namespace {
struct Reader {
    const std::string& t;
    size_t i = 0;
    std::string err;
    explicit Reader(const std::string& text) : t(text) {}
    void space() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i; }
    bool fail(const char* why) { if (err.empty()) err = std::string(why) + " at " + std::to_string(i); return false; }
    static void utf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    bool hex4(unsigned& c) {
        if (i + 4 > t.size()) return fail("short \\u");
        c = (unsigned)strtoul(t.substr(i, 4).c_str(), nullptr, 16); i += 4; return true;
    }
    bool string(std::string& o) {
        if (t[i] != '"') return fail("expected a string");
        ++i;
        while (i < t.size() && t[i] != '"') {
            if (t[i] == '\\') {
                if (++i >= t.size()) break;
                const char e = t[i++];
                switch (e) {
                    case 'n': o += '\n'; break; case 't': o += '\t'; break; case 'r': o += '\r'; break;
                    case 'b': o += '\b'; break; case 'f': o += '\f'; break;
                    case 'u': {
                        unsigned c; if (!hex4(c)) return false;
                        if (c >= 0xD800 && c < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                            i += 2; unsigned lo; if (!hex4(lo)) return false;
                            c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(o, c); break;
                    }
                    default: o += e;
                }
            } else o += t[i++];
        }
        if (i >= t.size()) return fail("unterminated string");
        ++i; return true;
    }
    bool value(Value& v, int depth) {
        if (depth > 64) return fail("too deep");
        space();
        if (i >= t.size()) return fail("unexpected end");
        const char c = t[i];
        if (c == '{') {
            v.type = Value::Object; ++i; space();
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            for (;;) {
                space(); std::string k; if (!string(k)) return false;
                space(); if (i >= t.size() || t[i] != ':') return fail("expected :"); ++i;
                Value m; if (!value(m, depth + 1)) return false;
                v.members.emplace_back(std::move(k), std::move(m));
                space(); if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; return true; }
                return fail("expected , or }");
            }
        }
        if (c == '[') {
            v.type = Value::Array; ++i; space();
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            for (;;) {
                Value m; if (!value(m, depth + 1)) return false;
                v.items.push_back(std::move(m));
                space(); if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; return true; }
                return fail("expected , or ]");
            }
        }
        if (c == '"') { v.type = Value::String; return string(v.s); }
        if (t.compare(i, 4, "true") == 0) { v.type = Value::Bool; v.b = true; i += 4; return true; }
        if (t.compare(i, 5, "false") == 0) { v.type = Value::Bool; i += 5; return true; }
        if (t.compare(i, 4, "null") == 0) { i += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char* end = nullptr; v.type = Value::Number; v.n = strtod(t.c_str() + i, &end);
            i = end - t.c_str(); return true;
        }
        return fail("unexpected character");
    }
};
}  // namespace

bool parse(const std::string& text, Value& out, std::string* err) {
    Reader r(text);
    out = Value();
    size_t start = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) start = 3;   // a BOM
    r.i = start;
    const bool ok = r.value(out, 0);
    if (!ok && err) *err = r.err;
    return ok;
}

std::string quote(const std::string& s) {
    std::string o = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += c;
        }
    }
    return o + "\"";
}

}  // namespace Json
