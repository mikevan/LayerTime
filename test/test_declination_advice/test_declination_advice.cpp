// Unit tests for src/core/logic/DeclinationAdvice, moved out of MappingScreen
// in Phase 0 Step 3b. test_declination_text proves the screen still shows
// the same words; this suite pins the core function on its own.

#include "check.h"

#include <cstdio>
#include <string>

#include "core/logic/DeclinationAdvice.h"

using namespace layertime::declination;

namespace {
std::string fmtValue(const Instruction &i)
{
    char b[64];
    snprintf(b, sizeof(b), i.valueFormat, i.magnitude);
    return b;
}
std::string fmtAdvice(const Instruction &i)
{
    char b[160];
    snprintf(b, sizeof(b), i.adviceFormat, i.magnitude, i.northName);
    return b;
}
} // namespace

void grid_offset_is_declination_minus_convergence()
{
    CHECK_NEAR(14.382039, mapOffsetDegrees(14.875312, 0.493273, MapNorth::Grid), 1e-9);
    CHECK_NEAR(-15.303560, mapOffsetDegrees(-15.142335, 0.161225, MapNorth::Grid), 1e-9);
    CHECK_NEAR(1.568424, mapOffsetDegrees(0.856945, -0.711479, MapNorth::Grid), 1e-9);
}

void true_offset_is_plain_declination()
{
    CHECK_NEAR(14.875312, mapOffsetDegrees(14.875312, 0.493273, MapNorth::True), 0.0);
    CHECK_NEAR(-9.11183, mapOffsetDegrees(-9.11183, 1.147981, MapNorth::True), 0.0);
}

void east_offset_says_add()
{
    const Instruction i = instructionFor(14.382039, MapNorth::Grid);
    CHECK_NEAR(14.382039, i.magnitude, 0.0);
    CHECK_STR("grid", i.northName);
    CHECK_STR("14.4 DEG EAST", fmtValue(i).c_str());
    CHECK_STR("Compass reads low. ADD 14.4 deg to a compass bearing to get a grid bearing.",
              fmtAdvice(i).c_str());
}

void west_offset_says_subtract_with_positive_magnitude()
{
    const Instruction i = instructionFor(-15.14, MapNorth::True);
    CHECK_NEAR(15.14, i.magnitude, 0.0);
    CHECK_STR("true", i.northName);
    CHECK_STR("15.1 DEG WEST", fmtValue(i).c_str());
    CHECK_STR("Compass reads high. SUBTRACT 15.1 deg from a compass bearing to get a true bearing.",
              fmtAdvice(i).c_str());
}

void exactly_zero_reads_east()
{
    const Instruction i = instructionFor(0.0, MapNorth::Grid);
    CHECK_STR("0.0 DEG EAST", fmtValue(i).c_str());
    CHECK_TRUE(std::string(i.adviceFormat).find("ADD") != std::string::npos);
}

void tiny_negative_reads_west_and_subtract_zero()
{
    // Carried over unchanged: a negative offset that rounds to 0.0 still
    // reads WEST and says SUBTRACT 0.0.
    const Instruction i = instructionFor(-0.01, MapNorth::Grid);
    CHECK_STR("0.0 DEG WEST", fmtValue(i).c_str());
    CHECK_TRUE(std::string(i.adviceFormat).find("SUBTRACT") != std::string::npos);
}

void no_location_texts()
{
    CHECK_STR("NO LOCATION", kNoLocationValue);
    CHECK_STR("Waiting for a GPS fix. Tap ELSEWHERE to enter a location instead.", kNoLocationAdvice);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(grid_offset_is_declination_minus_convergence);
    CASE(true_offset_is_plain_declination);
    CASE(east_offset_says_add);
    CASE(west_offset_says_subtract_with_positive_magnitude);
    CASE(exactly_zero_reads_east);
    CASE(tiny_negative_reads_west_and_subtract_zero);
    CASE(no_location_texts);
    CHECK_SUMMARY();
}
