// Test-only stand-in for the ESP32 Arduino Preferences (NVS) library.
// Never seen by the firmware build.
//
// Keeps every namespace in memory so a test can read back exactly which keys
// were written, with which types and values. A namespace opened read-only
// refuses writes, as the real library does. Setting fake_nvs::g_available to
// false makes begin() fail, as it does when NVS cannot be opened.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

// Arduino's String, as far as the code under test uses it.
class String {
public:
    String(const char *s = "") : _s(s ? s : "") {}
    const char *c_str() const { return _s.c_str(); }
    size_t length() const { return _s.size(); }

private:
    std::string _s;
};

namespace fake_nvs {
enum class Type { UChar, Bool, String };
struct Value {
    Type type = Type::UChar;
    long number = 0;
    std::string text;
};
inline std::map<std::string, std::map<std::string, Value>> g_store;
inline bool g_available = true;
inline void reset()
{
    g_store.clear();
    g_available = true;
}
} // namespace fake_nvs

class Preferences {
public:
    bool begin(const char *name, bool readOnly = false)
    {
        if (!fake_nvs::g_available) return false;
        _ns = name;
        _readOnly = readOnly;
        _open = true;
        return true;
    }
    void end() { _open = false; }

    uint8_t getUChar(const char *key, uint8_t def = 0) const
    {
        const fake_nvs::Value *v = find(key, fake_nvs::Type::UChar);
        return v ? static_cast<uint8_t>(v->number) : def;
    }
    size_t putUChar(const char *key, uint8_t value)
    {
        return put(key, fake_nvs::Type::UChar, value, "") ? 1 : 0;
    }
    bool getBool(const char *key, bool def = false) const
    {
        const fake_nvs::Value *v = find(key, fake_nvs::Type::Bool);
        return v ? v->number != 0 : def;
    }
    size_t putBool(const char *key, bool value)
    {
        return put(key, fake_nvs::Type::Bool, value ? 1 : 0, "") ? 1 : 0;
    }
    String getString(const char *key, const String &def = String()) const
    {
        const fake_nvs::Value *v = find(key, fake_nvs::Type::String);
        return v ? String(v->text.c_str()) : def;
    }
    size_t putString(const char *key, const char *value)
    {
        const std::string s = value ? value : "";
        return put(key, fake_nvs::Type::String, 0, s) ? s.size() : 0;
    }

private:
    const fake_nvs::Value *find(const char *key, fake_nvs::Type type) const
    {
        if (!_open) return nullptr;
        auto ns = fake_nvs::g_store.find(_ns);
        if (ns == fake_nvs::g_store.end()) return nullptr;
        auto it = ns->second.find(key);
        if (it == ns->second.end() || it->second.type != type) return nullptr;
        return &it->second;
    }
    bool put(const char *key, fake_nvs::Type type, long number, const std::string &text)
    {
        if (!_open || _readOnly) return false;
        fake_nvs::Value &v = fake_nvs::g_store[_ns][key];
        v.type = type;
        v.number = number;
        v.text = text;
        return true;
    }

    std::string _ns;
    bool _readOnly = false;
    bool _open = false;
};
