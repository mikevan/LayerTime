// The generated Monkey C copy of the LayerTime Link vectors,
// devices/garmin-tactix/source/link/LinkVectors.mc, against contracts/vectors/link_frames.json.
//
// Monkey C cannot read the JSON at run time, so tools/gen_link_vectors_mc.py
// compiles the vectors into the Connect IQ app as constants. This suite reads
// that generated file back with its own small parser and checks every
// constant, UUID, byte array and field against the JSON, so a stale or
// hand-edited copy fails here, on the host, before anything reaches a watch.
// It does not run the generator; it needs only g++.
//
// Build command: see test/README.md, "LayerTime Link".

#include "check.h"
#include "link_vectors_json.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

const char *kMcPath = "../devices/garmin-tactix/source/link/LinkVectors.mc";

// One `{:name => "...", ...}` entry of STATUS, REQUESTS or REPLIES.
struct McEntry {
    std::string name;
    std::string tag; // :op or :type when present
    std::vector<uint8_t> bytes;
    std::map<std::string, long long> fields;
};

struct McFile {
    std::string text;
    bool loaded = false;
    std::map<std::string, std::string> consts;               // const NAME = <literal>;
    std::map<std::string, std::vector<McEntry>> arrays;      // STATUS, REQUESTS, REPLIES
};

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string unquote(const std::string &s)
{
    const size_t a = s.find('"'), b = s.rfind('"');
    return (a == std::string::npos || b <= a) ? s : s.substr(a + 1, b - a - 1);
}

// "[0x01, 0xA2]b" -> bytes.
std::vector<uint8_t> parseByteArray(const std::string &s)
{
    std::vector<uint8_t> out;
    size_t i = s.find('[');
    const size_t end = s.find(']', i);
    while (i != std::string::npos && i < end) {
        const size_t x = s.find("0x", i);
        if (x == std::string::npos || x >= end) break;
        out.push_back(static_cast<uint8_t>(std::stoul(s.substr(x + 2, 2), nullptr, 16)));
        i = x + 4;
    }
    return out;
}

// "{:a => 1, :b => 4294967295l, :rssi => -67}" -> fields. A trailing 'l'
// marks a Long; a leading '-' a negative value (EVENT_SUMMARY's rssi).
std::map<std::string, long long> parseFields(const std::string &s)
{
    std::map<std::string, long long> out;
    size_t i = s.find('{');
    const size_t end = s.find('}', i);
    while (i != std::string::npos && i < end) {
        const size_t colon = s.find(':', i);
        if (colon == std::string::npos || colon >= end) break;
        const size_t arrow = s.find("=>", colon);
        const std::string key = trim(s.substr(colon + 1, arrow - colon - 1));
        size_t v = arrow + 2;
        while (v < end && s[v] == ' ') ++v;
        size_t e = v;
        if (e < end && s[e] == '-') ++e;
        while (e < end && isdigit(static_cast<unsigned char>(s[e]))) ++e;
        out[key] = std::stoll(s.substr(v, e - v));
        if (e < end && s[e] == 'l') ++e;
        i = e;
    }
    return out;
}

// Splits ", :key => " on the top level of one entry line: the byte array and
// the field dictionary each contain commas, so the split follows the
// `:key =>` markers instead.
McEntry parseEntry(const std::string &line)
{
    McEntry e;
    const char *keys[] = {":name", ":op", ":type", ":bytes", ":fields"};
    for (const char *k : keys) {
        const size_t at = line.find(std::string(k) + " => ");
        if (at == std::string::npos) continue;
        const size_t v = at + std::string(k).size() + 4;
        const std::string key(k);
        if (key == ":name") e.name = unquote(line.substr(v, line.find('"', line.find('"', v) + 1) - v + 1));
        else if (key == ":op" || key == ":type") e.tag = unquote(line.substr(v, line.find('"', line.find('"', v) + 1) - v + 1));
        else if (key == ":bytes") e.bytes = parseByteArray(line.substr(v));
        else if (key == ":fields") e.fields = parseFields(line.substr(v));
    }
    return e;
}

const McFile &mc()
{
    static McFile f;
    if (f.loaded) return f;
    f.loaded = true;
    std::ifstream in(kMcPath);
    std::stringstream ss; ss << in.rdbuf();
    f.text = ss.str();
    std::string current;
    std::stringstream lines(f.text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string t = trim(line);
        if (t.rfind("const ", 0) == 0) {
            const size_t eq = t.find(" = ");
            const std::string name = t.substr(6, eq - 6);
            std::string value = t.substr(eq + 3);
            if (value == "[") { current = name; continue; }
            if (!value.empty() && value.back() == ';') value.pop_back();
            f.consts[name] = value;
        } else if (!current.empty() && t.rfind("{:name", 0) == 0) {
            f.arrays[current].push_back(parseEntry(t));
        } else if (!current.empty() && t.rfind("]", 0) == 0) {
            current.clear();
        }
    }
    return f;
}

std::string upper(std::string s)
{
    for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

void checkConst(const char *name, const std::string &expected)
{
    const auto it = mc().consts.find(name);
    if (it == mc().consts.end()) {
        ++check::g_checks;
        check::fail(__FILE__, __LINE__, (std::string("missing const ") + name).c_str());
        return;
    }
    CHECK_STR(expected.c_str(), it->second.c_str());
}

void checkArray(const char *arrayName, const Json &json, const char *tagKey)
{
    const auto it = mc().arrays.find(arrayName);
    CHECK_TRUE(it != mc().arrays.end());
    if (it == mc().arrays.end()) return;
    const std::vector<McEntry> &entries = it->second;
    CHECK_INT(json.items.size(), entries.size());
    for (size_t i = 0; i < json.items.size() && i < entries.size(); ++i) {
        const Json &j = json.items[i];
        const McEntry &e = entries[i];
        CHECK_STR(j["name"].str.c_str(), e.name.c_str());
        if (tagKey) CHECK_STR(j[tagKey].str.c_str(), e.tag.c_str());
        const std::vector<uint8_t> expected = fromHex(j["bytes"].str);
        CHECK_STR(toHex(expected.data(), expected.size()).c_str(), toHex(e.bytes.data(), e.bytes.size()).c_str());
        CHECK_INT(j["fields"].members.size(), e.fields.size());
        for (const auto &m : j["fields"].members) {
            const auto f = e.fields.find(m.first);
            if (f == e.fields.end()) {
                ++check::g_checks;
                check::fail(__FILE__, __LINE__, (e.name + " lacks field " + m.first).c_str());
                continue;
            }
            CHECK_TRUE(static_cast<long long>(m.second.num) == f->second);
        }
    }
}

} // namespace

// --- The file is there and says what it is --------------------------------

void the_generated_file_exists_and_is_marked_generated()
{
    CHECK_FALSE(mc().text.empty());
    CHECK_TRUE(mc().text.find("GENERATED by tools/gen_link_vectors_mc.py") != std::string::npos);
    CHECK_TRUE(mc().text.find("module LinkVectors {") != std::string::npos);
    CHECK_TRUE(mc().text.find("import Toybox.Lang;") != std::string::npos);
}

// --- Scalars and UUIDs ------------------------------------------------------

void versions_and_uuids_match_the_json()
{
    const Json &v = vectors();
    checkConst("CONTRACT_VERSION", "\"" + v["contractVersion"].str + "\"");
    checkConst("LINK_VERSION", "\"" + v["linkVersion"].str + "\"");
    CHECK_INT(6, v["uuids"].members.size());
    for (const auto &u : v["uuids"].members) {
        checkConst(("UUID_" + upper(u.first)).c_str(), "\"" + u.second.str + "\"");
    }
}

void sizes_and_named_values_match_the_json()
{
    const Json &c = vectors()["constants"];
    const char *scalars[] = {"maxFrame", "statusSize", "linkVersionByte", "serverMajor", "serverMinor", "textMaxChunk"};
    for (const char *k : scalars) checkConst(upper(k).c_str(), std::to_string(c[k].asLong()));

    const char *groups[] = {"ops", "frameTypes", "linkStatus", "statusFlags", "schedule", "capabilities",
                            "summaryFlags", "textFields"};
    size_t named = 0;
    for (const char *g : groups) {
        for (const auto &m : c[g].members) {
            checkConst((upper(g) + "_" + upper(m.first)).c_str(), std::to_string(m.second.asLong()));
            ++named;
        }
    }
    CHECK_TRUE(named >= 34);

    std::string sizes = "[";
    for (size_t i = 0; i < c["probeSizes"].items.size(); ++i) {
        if (i) sizes += ", ";
        sizes += std::to_string(c["probeSizes"].items[i].asLong());
    }
    sizes += "] as Array<Number>";
    checkConst("PROBE_SIZES", sizes);
}

// --- Byte-exact vectors -----------------------------------------------------

void status_vectors_match_byte_for_byte()
{
    checkArray("STATUS", vectors()["status"], nullptr);
}

void request_vectors_match_byte_for_byte()
{
    checkArray("REQUESTS", vectors()["requests"], "op");
}

void reply_vectors_match_byte_for_byte()
{
    checkArray("REPLIES", vectors()["replies"], "type");
}

// --- Monkey C needs a Long literal above the signed 32-bit range ----------

void values_above_int32_carry_the_long_suffix()
{
    // 4294967295 (max_values.changeSeq) and 3735928559 (the PING token) do
    // not fit a Monkey C Number; the generator appends 'l'. A copy that lost
    // the suffix would compile and then overflow on the watch.
    CHECK_TRUE(mc().text.find("4294967295l") != std::string::npos);
    CHECK_TRUE(mc().text.find("3735928559l") != std::string::npos);
    CHECK_TRUE(mc().text.find("4294967295,") == std::string::npos);
    CHECK_TRUE(mc().text.find("4294967295}") == std::string::npos);
    CHECK_TRUE(mc().text.find("3735928559,") == std::string::npos);
    CHECK_TRUE(mc().text.find("3735928559}") == std::string::npos);
}

// --- Negative values survive the round trip -------------------------------

void negative_fields_are_generated_and_read_back_signed()
{
    // EVENT_SUMMARY's rssi is the only signed field; the generator writes it
    // as a plain negative literal, which fits a Monkey C Number.
    CHECK_TRUE(mc().text.find(":rssi => -67") != std::string::npos);
    CHECK_TRUE(mc().text.find(":rssi => -100") != std::string::npos);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_generated_file_exists_and_is_marked_generated);
    CASE(versions_and_uuids_match_the_json);
    CASE(sizes_and_named_values_match_the_json);
    CASE(status_vectors_match_byte_for_byte);
    CASE(request_vectors_match_byte_for_byte);
    CASE(reply_vectors_match_byte_for_byte);
    CASE(values_above_int32_carry_the_long_suffix);
    CASE(negative_fields_are_generated_and_read_back_signed);
    CHECK_SUMMARY();
}
