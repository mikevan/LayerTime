// The core/platform boundary, checked on the source tree itself. Added in
// Phase 0 Step 7.
//
// Since layout step 4 (2026-10-07) src/ holds only core, and the T-Watch
// Ultra's platform code is its own folder, devices/lilygo-tultra/src/. The
// rules are the same seven; "platform code" now means that folder, and
// main.cpp is the one inside it. The LayerWand and the S3 Plus check their
// own boundaries in their own test folders.
//
// Every #include under src/ and devices/lilygo-tultra/src/ is resolved the
// way the compiler resolves it, and classified by the file it actually
// reaches, not by how its path is spelled:
//   * "quoted": the including file's own directory first, then the src/
//     include root (the firmware build and these tests both put src/ on the
//     include path).
//   * <angled>: the src/ include root, then the sensor library's include root
//     (sensors/src, LayerTime-Sensors). Anything not found there is a system
//     or library header.
//
// Layout step 5 (2026-10-09) added the sensor library. Core may include it,
// and only through its public <lts/...> headers; device code may not include
// it at all, so every device reaches the sensors through core.
// So a core file writing "../model/Mesh.h" and one writing
// "core/model/Mesh.h" are the same include, and both are core.
//
// Run from test/, like every other suite; the trees are read from ../src and
// ../devices/lilygo-tultra/src.

#include "check.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const fs::path kRepo = fs::weakly_canonical("..");
const fs::path kSrc = fs::weakly_canonical("../src");
const fs::path kUltraSrc = fs::weakly_canonical("../devices/lilygo-tultra/src");
const fs::path kSensorsSrc = fs::weakly_canonical("../sensors/src");

// Headers a core file may take from outside the project: the C and C++
// standard library, and nothing else.
const std::set<std::string> kStandard = {
    "algorithm", "array", "cctype", "cinttypes", "climits", "cmath", "cstddef", "cstdint",
    "cstdio", "cstdlib", "cstring", "ctype.h", "float.h", "initializer_list", "inttypes.h",
    "limits", "limits.h", "math.h", "new", "stdbool.h", "stddef.h", "stdint.h", "stdio.h",
    "stdlib.h", "string", "string.h", "type_traits", "utility",
};

// Quoted includes that name a library header rather than a project file.
// Any other quoted include must resolve inside the project.
const std::set<std::string> kQuotedLibrary = {"esp_mac.h"};

struct Include {
    fs::path from;       // the including file
    std::string spelled; // as written
    bool quoted = false;
    fs::path target;     // resolved project file; empty when outside the project
};

std::string rel(const fs::path &p) { return fs::relative(p, kRepo).generic_string(); }

bool under(const fs::path &p, const fs::path &dir)
{
    const std::string a = p.generic_string(), b = dir.generic_string() + "/";
    return a.compare(0, b.size(), b) == 0;
}

fs::path resolve(const fs::path &from, const std::string &name, bool quoted)
{
    std::vector<fs::path> tries;
    if (quoted) tries.push_back(from.parent_path() / name);
    tries.push_back(kSrc / name);
    if (!quoted) tries.push_back(kSensorsSrc / name);
    for (const fs::path &t : tries) {
        std::error_code ec;
        if (fs::is_regular_file(t, ec)) return fs::weakly_canonical(t);
    }
    return {};
}

std::vector<Include> scan()
{
    std::vector<Include> out;
    std::vector<fs::path> files;
    for (const fs::path &root : {kSrc, kUltraSrc}) {
        if (!fs::is_directory(root)) continue;
        for (const auto &e : fs::recursive_directory_iterator(root))
            if (e.is_regular_file()) files.push_back(fs::weakly_canonical(e.path()));
    }
    std::sort(files.begin(), files.end());
    for (const fs::path &f : files) {
        std::ifstream in(f);
        std::string line;
        while (std::getline(in, line)) {
            size_t i = line.find_first_not_of(" \t");
            if (i == std::string::npos || line[i] != '#') continue;
            i = line.find_first_not_of(" \t", i + 1);
            if (i == std::string::npos || line.compare(i, 7, "include") != 0) continue;
            i = line.find_first_of("<\"", i + 7);
            if (i == std::string::npos) continue;
            const char close = line[i] == '<' ? '>' : '"';
            const size_t j = line.find(close, i + 1);
            if (j == std::string::npos) continue;
            Include inc;
            inc.from = f;
            inc.quoted = line[i] == '"';
            inc.spelled = line.substr(i + 1, j - i - 1);
            inc.target = resolve(f, inc.spelled, inc.quoted);
            out.push_back(inc);
        }
    }
    return out;
}

void report(const Include &inc, const char *why)
{
    fprintf(stderr, "      %s: %s includes %c%s%c%s%s\n", why, rel(inc.from).c_str(),
            inc.quoted ? '"' : '<', inc.spelled.c_str(), inc.quoted ? '"' : '>',
            inc.target.empty() ? "" : " -> ", inc.target.empty() ? "" : rel(inc.target).c_str());
}

const fs::path kCore = kSrc / "core";
const fs::path kPlatform = kUltraSrc;
const fs::path kMain = kUltraSrc / "main.cpp";

// The platform target a file belongs to ("twatch_ultra"), or "".
std::string targetOf(const fs::path &p)
{
    return under(p, kUltraSrc) ? "twatch_ultra" : "";
}

} // namespace

void the_source_tree_is_where_the_tests_expect()
{
    CHECK_TRUE(fs::is_directory(kCore));
    CHECK_TRUE(fs::is_directory(kPlatform));
    CHECK_TRUE(scan().size() > 100);
}

bool isPublicSensorInclude(const Include &inc)
{
    return !inc.quoted && inc.spelled.rfind("lts/", 0) == 0 && !inc.target.empty() &&
           under(inc.target, kSensorsSrc);
}

void core_includes_only_core_the_standard_library_and_the_sensor_library()
{
    int bad = 0;
    for (const Include &inc : scan()) {
        if (!under(inc.from, kCore)) continue;
        const bool ok = inc.target.empty() ? (!inc.quoted && kStandard.count(inc.spelled) == 1)
                                           : (under(inc.target, kCore) || isPublicSensorInclude(inc));
        if (!ok) {
            report(inc, "core reaches outside core");
            ++bad;
        }
    }
    CHECK_INT(0, bad);
}

void every_quoted_project_include_resolves()
{
    int bad = 0;
    for (const Include &inc : scan()) {
        if (!inc.quoted || !inc.target.empty() || kQuotedLibrary.count(inc.spelled) == 1) continue;
        report(inc, "does not resolve");
        ++bad;
    }
    CHECK_INT(0, bad);
}

void every_include_that_resolves_stays_inside_src()
{
    int bad = 0;
    for (const Include &inc : scan()) {
        if (inc.target.empty() || under(inc.target, kSrc) || under(inc.target, kUltraSrc)) continue;
        if (under(inc.from, kCore) && isPublicSensorInclude(inc)) continue;
        report(inc, "leaves src and the Ultra's folder");
        ++bad;
    }
    CHECK_INT(0, bad);
}

// Added with the Phase 0 Step 7 move, once the layout was final.

void every_source_file_is_core_a_platform_target_or_main()
{
    int bad = 0;
    for (const auto &e : fs::recursive_directory_iterator(kSrc)) {
        if (!e.is_regular_file()) continue;
        const fs::path f = fs::weakly_canonical(e.path());
        // Since layout step 4, src/ holds only core; every file in the
        // Ultra's own folder (main.cpp included) is that target's.
        if (under(f, kCore)) continue;
        fprintf(stderr, "      outside the layout: %s\n", rel(f).c_str());
        ++bad;
    }
    CHECK_INT(0, bad);
}

void only_platform_code_and_main_reach_platform_code()
{
    int bad = 0;
    for (const Include &inc : scan()) {
        if (inc.target.empty() || !under(inc.target, kPlatform)) continue;
        if (inc.from == kMain || under(inc.from, kPlatform)) continue;
        report(inc, "reaches platform code");
        ++bad;
    }
    CHECK_INT(0, bad);
}

void one_target_never_reaches_into_another()
{
    int bad = 0;
    for (const Include &inc : scan()) {
        if (inc.target.empty()) continue;
        const std::string from = targetOf(inc.from), to = targetOf(inc.target);
        if (from.empty() || to.empty() || from == to) continue;
        report(inc, "crosses targets");
        ++bad;
    }
    CHECK_INT(0, bad);
}

// Device folders whose code must reach the sensors only through core.
const char *const kDeviceSrcs[] = {"../devices/lilygo-tultra/src", "../devices/lilygo-s3plus/src",
                                   "../devices/lilygo-layerwand/src"};

// Every include in `root` that names the sensor library, however spelled:
// <lts/...>, or any path through sensors/src.
std::vector<std::string> sensorIncludesUnder(const fs::path &root)
{
    std::vector<std::string> out;
    if (!fs::is_directory(root)) return out;
    for (const auto &e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file()) continue;
        std::ifstream in(e.path());
        std::string line;
        int n = 0;
        while (std::getline(in, line)) {
            ++n;
            const size_t h = line.find("#include");
            if (h == std::string::npos) continue;
            const std::string rest = line.substr(h);
            if (rest.find("<lts/") != std::string::npos || rest.find("\"lts/") != std::string::npos ||
                rest.find("sensors/src") != std::string::npos || rest.find("/lts/") != std::string::npos)
                out.push_back(e.path().generic_string() + ":" + std::to_string(n) + "  " + rest);
        }
    }
    return out;
}

void devices_reach_the_sensors_only_through_core()
{
    int found = 0;
    for (const char *dir : kDeviceSrcs) {
        CHECK_TRUE(fs::is_directory(dir));
        for (const std::string &s : sensorIncludesUnder(dir)) {
            fprintf(stderr, "      device includes the sensor library directly: %s\n", s.c_str());
            ++found;
        }
    }
    CHECK_INT(0, found);
}

void the_device_rule_rejects_a_planted_direct_include()
{
    const fs::path scratch = fs::temp_directory_path() / "layertime_boundary_negative";
    fs::remove_all(scratch);
    fs::create_directories(scratch);
    std::ofstream(scratch / "A.cpp") << "#include <lts/Signatures.h>\n";
    std::ofstream(scratch / "B.h") << "#include \"../../../sensors/src/lts/Detectors.h\"\n";
    std::ofstream(scratch / "C.cpp") << "#include \"core/logic/ReconClassification.h\"\n";
    CHECK_INT(2, sensorIncludesUnder(scratch).size());
    fs::remove_all(scratch);
}

void the_core_rule_rejects_a_quoted_path_into_the_sensors()
{
    // Core must use the public <lts/...> spelling. A quoted path that
    // happens to reach the same file is still a layout dependency.
    Include quoted;
    quoted.from = kCore / "logic" / "X.cpp";
    quoted.spelled = "../../../sensors/src/lts/Detectors.h";
    quoted.quoted = true;
    quoted.target = kSensorsSrc / "lts" / "Detectors.h";
    CHECK_FALSE(isPublicSensorInclude(quoted));
    Include angled = quoted;
    angled.spelled = "lts/Detectors.h";
    angled.quoted = false;
    CHECK_TRUE(isPublicSensorInclude(angled));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_source_tree_is_where_the_tests_expect);
    CASE(core_includes_only_core_the_standard_library_and_the_sensor_library);
    CASE(every_quoted_project_include_resolves);
    CASE(every_include_that_resolves_stays_inside_src);
    CASE(every_source_file_is_core_a_platform_target_or_main);
    CASE(only_platform_code_and_main_reach_platform_code);
    CASE(one_target_never_reaches_into_another);
    CASE(devices_reach_the_sensors_only_through_core);
    CASE(the_device_rule_rejects_a_planted_direct_include);
    CASE(the_core_rule_rejects_a_quoted_path_into_the_sensors);
    CHECK_SUMMARY();
}
