// The LayerTime Link vectors, contracts/vectors/link_frames.json, read by a
// small JSON reader that needs nothing outside the standard library. Shared
// by test_link_codec (the C++ codec) and test_link_vectors_mc (the generated
// Monkey C copy). Include from a suite run in test/, which is where the
// relative path resolves.
#pragma once

#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// --- A small JSON reader, enough for link_frames.json --------------------

struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> items;
    std::map<std::string, Json> members;

    const Json &operator[](const char *key) const
    {
        static const Json none;
        auto it = members.find(key);
        return it == members.end() ? none : it->second;
    }
    long asLong() const { return static_cast<long>(num); }
    unsigned long asULong() const { return static_cast<unsigned long>(num); }
};

struct Parser {
    const std::string &s;
    size_t i = 0;
    explicit Parser(const std::string &src) : s(src) {}

    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    Json parse()
    {
        ws();
        Json j;
        if (i >= s.size()) return j;
        const char c = s[i];
        if (c == '{') {
            j.kind = Json::Object; ++i; ws();
            if (s[i] == '}') { ++i; return j; }
            for (;;) {
                ws(); std::string key = parseString(); ws(); ++i; // ':'
                j.members[key] = parse(); ws();
                if (s[i] == ',') { ++i; continue; }
                ++i; break; // '}'
            }
        } else if (c == '[') {
            j.kind = Json::Array; ++i; ws();
            if (s[i] == ']') { ++i; return j; }
            for (;;) {
                j.items.push_back(parse()); ws();
                if (s[i] == ',') { ++i; continue; }
                ++i; break; // ']'
            }
        } else if (c == '"') {
            j.kind = Json::String; j.str = parseString();
        } else if (c == 't' || c == 'f') {
            j.kind = Json::Bool; j.b = c == 't'; i += j.b ? 4 : 5;
        } else if (c == 'n') {
            i += 4;
        } else {
            j.kind = Json::Number;
            size_t start = i;
            while (i < s.size() && (isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+')) ++i;
            j.num = std::stod(s.substr(start, i - start));
        }
        return j;
    }
    std::string parseString()
    {
        std::string out; ++i; // opening quote
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') { ++i; out += s[i]; } else out += s[i];
            ++i;
        }
        ++i;
        return out;
    }
};

std::vector<uint8_t> fromHex(const std::string &h)
{
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(static_cast<uint8_t>(std::stoul(h.substr(i, 2), nullptr, 16)));
    return out;
}

std::string toHex(const uint8_t *p, size_t n)
{
    static const char *kHex = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < n; ++i) { out += kHex[p[i] >> 4]; out += kHex[p[i] & 15]; }
    return out;
}

const Json &vectors()
{
    static std::unique_ptr<Json> loaded;
    if (!loaded) {
        std::ifstream f("../contracts/vectors/link_frames.json");
        std::stringstream ss; ss << f.rdbuf();
        const std::string text = ss.str();
        Parser p(text);
        loaded.reset(new Json(p.parse()));
    }
    return *loaded;
}
