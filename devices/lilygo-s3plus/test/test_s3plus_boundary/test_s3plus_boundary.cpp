// The S3 Plus target's boundary, checked on the source tree itself.
//
// devices/lilygo-s3plus/ is its own project beside src/, the way devices/garmin-tactix/ is. Its code may
// reach the shared, hardware-independent core (src/core/) and its own files,
// and nothing else in the repository: never another target's platform code
// (src/platform/...), never src/main.cpp. In the other direction, nothing
// under src/ may reach devices/lilygo-s3plus/, so the T-Watch Ultra build (which compiles
// all of src/) can never pull S3 Plus code in.
//
// Includes are resolved the way devices/lilygo-s3plus/platformio.ini resolves them: a
// quoted include first against the including file's folder, then the -I
// paths in order (../../src, variants/lilygo_twatch_s3, and . = devices/lilygo-s3plus/).
// An include that resolves to no project file is a system or library header.
//
// Run from devices/lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -o tests_s3plus_boundary test_s3plus_boundary/test_s3plus_boundary.cpp
//   ./tests_s3plus_boundary

#include "check.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const fs::path kRepo = fs::weakly_canonical(fs::path("../../.."));
const fs::path kS3 = kRepo / "devices" / "lilygo-s3plus";
const fs::path kSource = kS3 / "src";
const fs::path kSrc = kRepo / "src";
const fs::path kCore = kSrc / "core";

struct Include {
    fs::path from;
    std::string spelled;
    bool quoted;
    fs::path target; // empty when it is not a project file
};

bool under(const fs::path &p, const fs::path &dir)
{
    const std::string a = fs::weakly_canonical(p).generic_string();
    const std::string b = fs::weakly_canonical(dir).generic_string() + "/";
    return a.compare(0, b.size(), b) == 0;
}

bool isSource(const fs::path &p)
{
    const std::string e = p.extension().string();
    return e == ".h" || e == ".hpp" || e == ".c" || e == ".cpp";
}

std::vector<fs::path> sourcesUnder(const fs::path &dir)
{
    std::vector<fs::path> out;
    if (!fs::exists(dir)) return out;
    for (const auto &e : fs::recursive_directory_iterator(dir))
        if (e.is_regular_file() && isSource(e.path())) out.push_back(e.path());
    return out;
}

std::vector<Include> includesOf(const fs::path &file, const std::vector<fs::path> &searchPath)
{
    static const std::regex re(R"(^\s*#\s*include\s*([<"])([^>"]+)[>"])");
    std::vector<Include> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        std::smatch m;
        if (!std::regex_search(line, m, re)) continue;
        Include inc{file, m[2].str(), m[1].str() == "\"", {}};
        std::vector<fs::path> candidates;
        if (inc.quoted) candidates.push_back(file.parent_path() / inc.spelled);
        for (const auto &dir : searchPath) candidates.push_back(dir / inc.spelled);
        for (const auto &c : candidates) {
            if (fs::is_regular_file(c)) { inc.target = fs::weakly_canonical(c); break; }
        }
        out.push_back(inc);
    }
    return out;
}

void report(const Include &inc, const char *why)
{
    char msg[512];
    std::snprintf(msg, sizeof(msg), "%s: %s includes \"%s\"%s%s", why,
                  fs::relative(inc.from, kRepo).generic_string().c_str(), inc.spelled.c_str(),
                  inc.target.empty() ? "" : " -> ",
                  inc.target.empty() ? "" : fs::relative(inc.target, kRepo).generic_string().c_str());
    check::fail(__FILE__, __LINE__, msg);
}

const std::vector<fs::path> kS3SearchPath = {kSrc, kS3 / "variants" / "lilygo_twatch_s3", kS3};

} // namespace

void the_s3plus_source_tree_exists()
{
    CHECK_TRUE(fs::is_directory(kSource));
    CHECK_TRUE(!sourcesUnder(kSource).empty());
}

void s3plus_reaches_only_its_own_files_and_core()
{
    for (const auto &f : sourcesUnder(kSource)) {
        for (const auto &inc : includesOf(f, kS3SearchPath)) {
            ++check::g_checks;
            if (inc.target.empty()) continue; // system or library header
            const bool ok = under(inc.target, kS3) || under(inc.target, kCore) ||
                            under(inc.target, kS3 / "variants" / "lilygo_twatch_s3");
            if (!ok) report(inc, "reaches outside devices/lilygo-s3plus/ and src/core/");
        }
    }
}

void s3plus_never_reaches_another_targets_platform_code()
{
    for (const auto &f : sourcesUnder(kSource)) {
        for (const auto &inc : includesOf(f, kS3SearchPath)) {
            ++check::g_checks;
            if (!inc.target.empty() && under(inc.target, kSrc / "platform"))
                report(inc, "reaches platform code of another target");
        }
    }
}

void nothing_under_src_reaches_s3plus()
{
    // The Ultra and C5 builds compile src/. None of it may include S3 Plus code.
    for (const auto &f : sourcesUnder(kSrc)) {
        for (const auto &inc : includesOf(f, {kSrc})) {
            ++check::g_checks;
            if (!inc.target.empty() && under(inc.target, kS3))
                report(inc, "src/ reaches devices/lilygo-s3plus/");
            if (inc.spelled.find("s3plus") != std::string::npos)
                report(inc, "src/ names s3plus in an include");
        }
    }
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_s3plus_source_tree_exists);
    CASE(s3plus_reaches_only_its_own_files_and_core);
    CASE(s3plus_never_reaches_another_targets_platform_code);
    CASE(nothing_under_src_reaches_s3plus);
    CHECK_SUMMARY();
}
