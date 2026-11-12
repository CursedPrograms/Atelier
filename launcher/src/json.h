// Atelier launcher - a small JSON reader: enough for GitHub's release answers, atelier.json and installed.json.
// Parse once into a tree of Values; ask for members by name. Writing is done by hand where it's needed.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> items;                   // Array
    std::vector<std::pair<std::string, Value>> members;   // Object, in order

    const Value& operator[](const std::string& key) const;   // Null when missing
    const Value& operator[](size_t i) const;
    std::string str(const std::string& def = "") const { return type == String ? s : def; }
    bool isObject() const { return type == Object; }
    bool isArray() const { return type == Array; }
};

// false (and err) when the text isn't JSON
bool parse(const std::string& text, Value& out, std::string* err = nullptr);

// a string as a JSON literal, quotes included
std::string quote(const std::string& s);

}  // namespace Json
