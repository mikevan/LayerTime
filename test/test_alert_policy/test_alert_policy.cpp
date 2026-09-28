// Unit tests for src/core/logic/AlertPolicy, moved out of
// ReconService::addDetection in Phase 0 Step 3g. The whole truth table is
// small, so every combination is checked.

#include "check.h"

#include "core/logic/AlertPolicy.h"

using namespace layertime;
using namespace layertime::alert;

void new_record_truth_table()
{
    CHECK_TRUE(raisesOnNewRecord(Confidence::High, false));
    CHECK_TRUE(raisesOnNewRecord(Confidence::Medium, false));
    CHECK_FALSE(raisesOnNewRecord(Confidence::Low, false));
    CHECK_FALSE(raisesOnNewRecord(Confidence::High, true));
    CHECK_FALSE(raisesOnNewRecord(Confidence::Medium, true));
    CHECK_FALSE(raisesOnNewRecord(Confidence::Low, true));
}

void repeat_keeps_the_strongest_grade()
{
    const Confidence all[] = {Confidence::Low, Confidence::Medium, Confidence::High};
    for (Confidence recorded : all) {
        for (Confidence seen : all) {
            const RepeatOutcome o = onRepeatSighting(recorded, seen);
            const Confidence expected = seen > recorded ? seen : recorded;
            CHECK_INT(static_cast<int>(expected), static_cast<int>(o.confidence));
        }
    }
}

void repeat_never_alerts_even_on_upgrade_KNOWN_DEFECT()
{
    const Confidence all[] = {Confidence::Low, Confidence::Medium, Confidence::High};
    for (Confidence recorded : all)
        for (Confidence seen : all) CHECK_FALSE(onRepeatSighting(recorded, seen).raiseAlert);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(new_record_truth_table);
    CASE(repeat_keeps_the_strongest_grade);
    CASE(repeat_never_alerts_even_on_upgrade_KNOWN_DEFECT);
    CHECK_SUMMARY();
}
