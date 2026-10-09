// The library must stand alone: nothing in it may depend on any consumer's
// headers, sources, fixtures, or repository layout. This test reads every
// source, test, and script file in the library and rejects:
//   * a quoted include that does not resolve inside the library,
//   * an angle include that is neither the C/C++ standard library nor one of
//     the library's own <lts/...> headers,
//   * any mention, outside comments, of a consumer's namespace, types, or
//     folders (layertime, ReconTarget, MonitorEvent, core/, devices/). The
//     library's own name, LayerTime-Sensors, is allowed.
// It then proves the checker works by planting each kind of violation in a
// scratch copy and requiring that every one is caught.
//
// Run from test/, like the other suites.

#include "check.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const std::set<std::string> kStandard = {
    "algorithm", "array", "cctype", "cinttypes", "climits", "cmath", "cstdarg", "cstddef",
    "cstdint", "cstdio", "cstdlib", "cstring", "ctype.h", "filesystem", "float.h", "fstream",
    "functional", "initializer_list", "inttypes.h", "iostream", "limits", "limits.h", "map",
    "math.h", "memory", "new", "set", "sstream", "stdarg.h", "stdbool.h", "stddef.h", "stdint.h",
    "stdio.h", "stdlib.h", "string", "string.h", "type_traits", "utility", "vector",
};

const char *const kForbidden[] = {"layertime", "ReconTarget", "MonitorEvent", "core/", "devices/"};

std::string lower(std::string s)
{
    for (char &c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

bool inside(const fs::path &p, const fs::path &root)
{
    const std::string a = fs::weakly_canonical(p).generic_string();
    const std::string r = fs::weakly_canonical(root).generic_string() + "/";
    return a.compare(0, r.size(), r) == 0;
}

// Returns one line per violation found under `root`.
std::vector<std::string> violations(const fs::path &root)
{
    std::vector<std::string> out;
    const fs::path src = root / "src";
    for (const char *dir : {"src", "test", "tools"}) {
        if (!fs::is_directory(root / dir)) continue;
        for (const auto &e : fs::recursive_directory_iterator(root / dir)) {
            if (!e.is_regular_file()) continue;
            const std::string ext = e.path().extension().string();
            if (ext != ".h" && ext != ".cpp" && ext != ".sh") continue;
            const fs::path file = e.path();
            const std::string name = fs::relative(file, root).generic_string();
            // This file names the forbidden words in order to forbid them.
            if (file.filename() == "test_architecture.cpp") continue;
            std::ifstream in(file);
            std::string line;
            int n = 0;
            while (std::getline(in, line)) {
                ++n;
                std::string code = line;
                const size_t comment = ext == ".sh" ? code.find('#') : code.find("//");
                const bool isInclude = ext != ".sh" && code.find("#include") != std::string::npos;
                if (!isInclude && comment != std::string::npos) code = code.substr(0, comment);
                if (isInclude) {
                    const size_t a = code.find_first_of("<\"");
                    const char close = code[a] == '<' ? '>' : '"';
                    const size_t b = code.find(close, a + 1);
                    const std::string spelled = code.substr(a + 1, b - a - 1);
                    if (code[a] == '"') {
                        // Searched like the build does: the file's folder, then
                        // src/ and test/ (the two -I paths), and nothing else.
                        bool ok = false;
                        for (const fs::path &dir : {file.parent_path(), src, root / "test"})
                            if (fs::exists(dir / spelled) && inside(dir / spelled, root)) ok = true;
                        if (!ok) out.push_back(name + ":" + std::to_string(n) + " quoted include leaves the library: " + spelled);
                    } else if (spelled.rfind("lts/", 0) == 0) {
                        if (!fs::exists(src / spelled))
                            out.push_back(name + ":" + std::to_string(n) + " unknown library header: " + spelled);
                    } else if (kStandard.count(spelled) == 0) {
                        out.push_back(name + ":" + std::to_string(n) + " non-standard include: " + spelled);
                    }
                    code = code.substr(0, code.find_first_of("<\""));
                }
                // The library's own name, LayerTime-Sensors, is not a dependency.
                std::string text = lower(code);
                for (size_t at; (at = text.find("layertime-sensors")) != std::string::npos;)
                    text.erase(at, 17);
                for (const char *word : kForbidden)
                    if (text.find(lower(word)) != std::string::npos)
                        out.push_back(name + ":" + std::to_string(n) + " mentions " + word);
            }
        }
    }
    return out;
}

} // namespace

void the_library_depends_on_nothing_outside_itself()
{
    const fs::path root = fs::weakly_canonical("..");
    CHECK_TRUE(fs::exists(root / "src/lts/Detectors.h"));
    const std::vector<std::string> v = violations(root);
    for (const std::string &s : v) printf("      %s\n", s.c_str());
    CHECK_INT(0, v.size());
}

void the_checker_rejects_each_kind_of_forbidden_dependency()
{
    const fs::path scratch = fs::temp_directory_path() / "lts_architecture_negative";
    fs::remove_all(scratch);
    fs::create_directories(scratch / "src/lts");
    fs::copy_file("../src/lts/Detectors.h", scratch / "src/lts/Detectors.h");
    struct Plant {
        const char *file;
        const char *text;
    } plants[] = {
        {"src/lts/A.h", "#include \"../../../src/core/model/MonitorEvent.h\"\n"},
        {"src/lts/B.h", "#include <core/logic/ReconSelection.h>\n"},
        {"src/lts/C.cpp", "#include \"Detectors.h\"\nint x = static_cast<int>(layertime::ReconTarget::All);\n"},
        {"src/lts/D.h", "#include <lts/NotInTheLibrary.h>\n"},
        {"src/lts/E.h", "#include <Arduino.h>\n"},
        {"tools/F.sh", "cc ../../devices/x/src/main.cpp\n"},
    };
    for (const Plant &p : plants) {
        fs::create_directories((scratch / p.file).parent_path());
        std::ofstream(scratch / p.file) << p.text;
    }
    const std::vector<std::string> v = violations(scratch);
    for (const Plant &p : plants) {
        bool caught = false;
        for (const std::string &s : v)
            if (s.rfind(p.file, 0) == 0) caught = true;
        if (!caught) printf("      not caught: %s\n", p.file);
        CHECK_TRUE(caught);
    }
    // A clean file in the same scratch tree is not flagged.
    for (const std::string &s : v) CHECK_TRUE(s.rfind("src/lts/Detectors.h", 0) != 0);
    fs::remove_all(scratch);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_library_depends_on_nothing_outside_itself);
    CASE(the_checker_rejects_each_kind_of_forbidden_dependency);
    CHECK_SUMMARY();
}
