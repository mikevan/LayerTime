// Unit tests for src/core/logic/ReconScheduler, the Recon schedule expressed
// in core in Slice 1 Increment 2A from the T-Watch Ultra reference
// (test_recon characterizes that original through its own ReconService).
// This suite pins the core scheduler on its own, against a recording radio
// port; the T-Dongle-C5 is its consumer.
//
// Each case advances a fake clock exactly the way the loop would and checks
// the sequence of port calls, because that sequence is what the radios see.

#include "check.h"

#include "core/logic/ReconScheduler.h"

#include <string>
#include <vector>

using namespace layertime;
using namespace layertime::recon;

namespace {

uint32_t g_now = 1000;
uint32_t clock() { return g_now; }

// Records every port call as text, and models the one piece of state the
// scheduler reads back: whether a scan is still running. A scan started for
// `durationMs` finishes when the clock passes its end.
struct FakeRadio : ReconRadio {
    std::vector<std::string> calls;
    bool wifiOn = false;
    uint8_t channel = 0;
    bool scanRunning = false;
    uint32_t scanEndsMs = 0;
    ReconTarget scanFor = ReconTarget::None;

    uint32_t settleMs = 0; // clock time the platform spends inside a call
    void startWifiMonitoring() override
    {
        calls.push_back("wifi-start");
        wifiOn = true;
        channel = 1;
        g_now += settleMs;
    }
    void stopWifiMonitoring() override { calls.push_back("wifi-stop"); wifiOn = false; }
    void setWifiChannel(uint8_t c) override { calls.push_back("hop " + std::to_string(c)); channel = c; }
    void startBleScan(ReconTarget d, uint32_t ms) override
    {
        calls.push_back("ble-start " + std::to_string(static_cast<int>(d)) + " " + std::to_string(ms));
        scanRunning = true;
        scanEndsMs = g_now + ms;
        scanFor = d;
    }
    void stopBleScan() override { calls.push_back("ble-stop"); scanRunning = false; }
    bool bleScanning() const override { return scanRunning && g_now < scanEndsMs; }

    void tick() { if (scanRunning && g_now >= scanEndsMs) scanRunning = false; }
};

struct Rig {
    FakeRadio radio;
    ReconScheduler s{radio, clock};
    Rig() { g_now = 1000; }
    // One loop pass `ms` later.
    void after(uint32_t ms)
    {
        g_now += ms;
        radio.tick();
        s.poll(g_now);
    }
    std::string last() const { return radio.calls.empty() ? "" : radio.calls.back(); }
    size_t hops() const
    {
        size_t n = 0;
        for (const std::string &c : radio.calls) if (c.rfind("hop ", 0) == 0) ++n;
        return n;
    }
};

} // namespace

// ------------------------------------------------------------ constants

void the_timing_constants_are_the_ones_reconservice_used()
{
    CHECK_INT(650, ReconScheduler::kChannelHopMs);
    CHECK_INT(12000, ReconScheduler::kBleCycleMs);
    CHECK_INT(1800, ReconScheduler::kBleScanMs);
    CHECK_INT(5000, ReconScheduler::kBleOnlyRescanMs);
    CHECK_INT(10000, ReconScheduler::kEarlyWarningActiveMs);
    CHECK_INT(60000, ReconScheduler::kEarlyWarningRestMs);
    CHECK_INT(1, ReconScheduler::kFirstChannel);
    CHECK_INT(11, ReconScheduler::kLastChannel);
}

void idle_scheduler_touches_no_radio()
{
    Rig r;
    r.after(650);
    r.after(60000);
    CHECK_INT(0, r.radio.calls.size());
    CHECK_FALSE(r.s.status().monitoring);
    CHECK_INT(static_cast<int>(ReconTarget::None), static_cast<int>(r.s.status().selected));
}

// ------------------------------------------------------------ Wi-Fi only

void wifi_only_detector_starts_on_channel_1_and_hops_every_650_ms()
{
    Rig r;
    r.s.start(ReconTarget::Deauth);
    // start() tears down first, then brings Wi-Fi up.
    CHECK_INT(3, r.radio.calls.size());
    CHECK_STR("wifi-stop", r.radio.calls[0].c_str());
    CHECK_STR("ble-stop", r.radio.calls[1].c_str());
    CHECK_STR("wifi-start", r.radio.calls[2].c_str());
    CHECK_INT(1, r.s.wifiChannel());
    CHECK_TRUE(r.s.status().monitoring);
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), static_cast<int>(r.s.status().active));

    r.after(649);
    CHECK_INT(0, r.hops());
    r.after(1);
    CHECK_INT(1, r.hops());
    CHECK_STR("hop 2", r.last().c_str());
    CHECK_INT(2, r.s.wifiChannel());
}

void the_hop_cursor_wraps_from_11_to_1()
{
    Rig r;
    r.s.start(ReconTarget::Pwnagotchi);
    for (int i = 0; i < 10; ++i) r.after(650);
    CHECK_INT(11, r.s.wifiChannel());
    r.after(650);
    CHECK_INT(1, r.s.wifiChannel());
    CHECK_STR("hop 1", r.last().c_str());
}

void a_late_poll_hops_once_not_several_times()
{
    Rig r;
    r.s.start(ReconTarget::Deauth);
    r.after(5000); // the loop stalled for several hop periods
    CHECK_INT(1, r.hops());
    CHECK_INT(2, r.s.wifiChannel());
}

// ------------------------------------------------------------ BLE only

void ble_only_detector_scans_at_once_and_rescans_5_s_after_the_last_start()
{
    Rig r;
    r.s.start(ReconTarget::AirTag);
    CHECK_STR("ble-start 10 1800", r.last().c_str());
    CHECK_INT(0, r.hops());
    r.after(1000); // still scanning
    r.after(1000); // scan over at +1800, but 5 s not passed
    CHECK_INT(3, r.radio.calls.size());
    r.after(2999); // +4999
    CHECK_INT(3, r.radio.calls.size());
    r.after(1); // +5000
    CHECK_INT(4, r.radio.calls.size());
    CHECK_STR("ble-start 10 1800", r.last().c_str());
    // No Wi-Fi at any point.
    for (const std::string &c : r.radio.calls) CHECK_TRUE(c != "wifi-start");
}

void ble_only_never_starts_a_scan_over_one_still_running()
{
    Rig r;
    r.s.start(ReconTarget::Flipper);
    r.radio.scanEndsMs = g_now + 7000; // the stack ran long
    r.after(5000);
    r.after(1000);
    CHECK_INT(3, r.radio.calls.size());
    r.after(1000); // now finished and past 5 s
    CHECK_INT(4, r.radio.calls.size());
}

// ------------------------------------------------------------ mixed (ALL)

void all_starts_on_wifi_showing_deauth_and_bursts_ble_every_12_s()
{
    Rig r;
    r.s.start(ReconTarget::All);
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), static_cast<int>(r.s.status().active));
    CHECK_INT(static_cast<int>(ReconTarget::All), static_cast<int>(r.s.status().selected));

    for (int i = 0; i < 18; ++i) r.after(650); // 11700 ms: still sweeping
    CHECK_INT(18, r.hops());
    r.after(300); // 12000 ms
    // Wi-Fi off, one BLE burst for ALL, active shows Flock.
    const size_t n = r.radio.calls.size();
    CHECK_STR("ble-start 1 1800", r.radio.calls[n - 1].c_str());
    CHECK_STR("wifi-stop", r.radio.calls[n - 2].c_str());
    CHECK_INT(static_cast<int>(ReconTarget::Flock), static_cast<int>(r.s.status().active));

    r.after(1000); // burst still running: nothing happens
    CHECK_INT(n, r.radio.calls.size());
    r.after(800); // burst over: Wi-Fi resumes on channel 1, Deauth again
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_INT(1, r.s.wifiChannel());
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), static_cast<int>(r.s.status().active));
    // The next burst is 12 s after the resume, not after the previous burst.
    const size_t m = r.radio.calls.size();
    for (int i = 0; i < 18; ++i) r.after(650);
    CHECK_TRUE(r.last().rfind("hop ", 0) == 0);
    r.after(300);
    CHECK_STR("ble-start 1 1800", r.last().c_str());
    CHECK_TRUE(r.radio.calls.size() == m + 18 + 2);
}

void a_mixed_group_alternates_but_keeps_its_own_active_detector()
{
    Rig r;
    r.s.start(ReconTarget::CounterIntrusion); // four Wi-Fi detectors plus Flipper
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_INT(static_cast<int>(ReconTarget::CounterIntrusion), static_cast<int>(r.s.status().active));
    r.after(12000);
    CHECK_STR("ble-start 4 1800", r.last().c_str());
    CHECK_INT(static_cast<int>(ReconTarget::CounterIntrusion), static_cast<int>(r.s.status().active));
    r.after(1800);
    CHECK_STR("wifi-start", r.last().c_str());
}

// ------------------------------------------------------------ early warning

void early_warning_sweeps_10_s_bursts_ble_then_rests_60_s()
{
    Rig r;
    r.s.setEarlyWarningEnabled(true);
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_TRUE(r.s.sweepingForBackground());
    CHECK_TRUE(r.s.status().earlyWarningEnabled);
    CHECK_FALSE(r.s.status().earlyWarningResting);
    CHECK_FALSE(r.s.status().monitoring);

    for (int i = 0; i < 15; ++i) r.after(650); // 9750 ms
    CHECK_INT(15, r.hops());
    r.after(250); // 10000 ms: sweep over, BLE burst
    const size_t n = r.radio.calls.size();
    CHECK_STR("ble-start 17 1800", r.radio.calls[n - 1].c_str());
    CHECK_STR("wifi-stop", r.radio.calls[n - 2].c_str());
    CHECK_FALSE(r.s.sweepingForBackground());
    CHECK_FALSE(r.s.status().earlyWarningResting);

    r.after(1000); // burst running
    CHECK_FALSE(r.s.status().earlyWarningResting);
    r.after(800); // burst over: rest begins, both radios idle, no call
    CHECK_TRUE(r.s.status().earlyWarningResting);
    CHECK_INT(n, r.radio.calls.size());
    r.after(59999);
    CHECK_INT(n, r.radio.calls.size());
    r.after(1); // 60 s after the rest began: sweep again from channel 1
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_INT(1, r.s.wifiChannel());
    CHECK_TRUE(r.s.sweepingForBackground());
    CHECK_FALSE(r.s.status().earlyWarningResting);
}

void disabling_early_warning_idles_both_radios_and_clears_resting()
{
    Rig r;
    r.s.setEarlyWarningEnabled(true);
    r.after(10000);
    r.after(1800);
    CHECK_TRUE(r.s.status().earlyWarningResting);
    r.s.setEarlyWarningEnabled(false);
    const size_t n = r.radio.calls.size();
    CHECK_STR("ble-stop", r.radio.calls[n - 1].c_str());
    CHECK_STR("wifi-stop", r.radio.calls[n - 2].c_str());
    CHECK_FALSE(r.s.status().earlyWarningEnabled);
    CHECK_FALSE(r.s.status().earlyWarningResting);
    CHECK_FALSE(r.s.sweepingForBackground());
    r.after(60000);
    CHECK_INT(n, r.radio.calls.size());
    // Enabling twice is one enable.
    r.s.setEarlyWarningEnabled(true);
    r.s.setEarlyWarningEnabled(true);
    CHECK_INT(n + 1, r.radio.calls.size());
}

void a_manual_session_pauses_early_warning_and_stop_manual_resumes_it()
{
    Rig r;
    r.s.setEarlyWarningEnabled(true);
    r.after(650);
    r.s.start(ReconTarget::AirTag);
    CHECK_FALSE(r.s.sweepingForBackground());
    CHECK_TRUE(r.s.status().earlyWarningEnabled);
    CHECK_STR("ble-start 10 1800", r.last().c_str());
    r.after(60000); // no sweep while manual
    CHECK_FALSE(r.s.sweepingForBackground());
    r.s.stopManual();
    // stop, then the sweep re-armed on Wi-Fi channel 1.
    const size_t n = r.radio.calls.size();
    CHECK_STR("wifi-start", r.radio.calls[n - 1].c_str());
    CHECK_STR("ble-stop", r.radio.calls[n - 2].c_str());
    CHECK_STR("wifi-stop", r.radio.calls[n - 3].c_str());
    CHECK_TRUE(r.s.sweepingForBackground());
    CHECK_FALSE(r.s.status().monitoring);
    CHECK_INT(static_cast<int>(ReconTarget::None), static_cast<int>(r.s.status().selected));
}

void disabling_early_warning_during_a_manual_session_leaves_the_session_alone()
{
    Rig r;
    r.s.setEarlyWarningEnabled(true);
    r.s.start(ReconTarget::Deauth);
    const size_t n = r.radio.calls.size();
    r.s.setEarlyWarningEnabled(false);
    CHECK_INT(n, r.radio.calls.size());
    CHECK_TRUE(r.s.status().monitoring);
    r.s.stopManual();
    CHECK_STR("ble-stop", r.last().c_str()); // no re-arm
}

void enabling_early_warning_during_a_manual_session_arms_only_at_stop_manual()
{
    Rig r;
    r.s.start(ReconTarget::Deauth);
    const size_t n = r.radio.calls.size();
    r.s.setEarlyWarningEnabled(true);
    CHECK_INT(n, r.radio.calls.size());
    CHECK_TRUE(r.s.status().earlyWarningEnabled);
    CHECK_FALSE(r.s.sweepingForBackground());
    r.s.stopManual();
    CHECK_STR("wifi-start", r.last().c_str());
    CHECK_TRUE(r.s.sweepingForBackground());
}

// ------------------------------------------------------------ stop and restart

void stop_idles_both_radios_and_forgets_the_selection_but_not_the_enable()
{
    Rig r;
    r.s.setEarlyWarningEnabled(true);
    r.s.start(ReconTarget::All);
    r.after(12000);
    r.s.stop();
    CHECK_STR("ble-stop", r.last().c_str());
    CHECK_FALSE(r.s.status().monitoring);
    CHECK_INT(static_cast<int>(ReconTarget::None), static_cast<int>(r.s.status().selected));
    CHECK_INT(static_cast<int>(ReconTarget::None), static_cast<int>(r.s.status().active));
    CHECK_TRUE(r.s.status().earlyWarningEnabled);
    CHECK_FALSE(r.s.sweepingForBackground());
    // Nothing runs on its own after stop(): the sweep needs stopManual().
    const size_t n = r.radio.calls.size();
    r.after(60000);
    CHECK_INT(n, r.radio.calls.size());
}

void starting_none_is_a_stop()
{
    Rig r;
    r.s.start(ReconTarget::Deauth);
    r.s.start(ReconTarget::None);
    CHECK_FALSE(r.s.status().monitoring);
    CHECK_STR("ble-stop", r.last().c_str());
    const size_t n = r.radio.calls.size();
    r.after(650);
    CHECK_INT(n, r.radio.calls.size());
}

void restarting_wifi_restarts_the_hop_clock_from_the_radio_clock()
{
    // The old code stamped the hop clock inside startWifiMonitoring with its
    // own millis() reading, taken after the settle delays that precede it;
    // the scheduler reads the clock at the same point, so the first hop after
    // a resume is 650 ms after the radio came up, not after the poll began.
    Rig r;
    r.s.start(ReconTarget::All);
    for (int i = 0; i < 18; ++i) r.after(650);
    r.after(300); // 12000 ms: BLE burst
    CHECK_INT(18, r.hops());
    r.radio.settleMs = 120; // the platform's delays inside the resume
    r.after(1800);          // burst over: resume, the clock moves 120 ms inside the call
    CHECK_STR("wifi-start", r.last().c_str());
    r.radio.settleMs = 0;
    r.after(649); // 769 ms after the poll began, 649 after the radio came up
    CHECK_INT(18, r.hops());
    r.after(1);
    CHECK_INT(19, r.hops());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_timing_constants_are_the_ones_reconservice_used);
    CASE(idle_scheduler_touches_no_radio);
    CASE(wifi_only_detector_starts_on_channel_1_and_hops_every_650_ms);
    CASE(the_hop_cursor_wraps_from_11_to_1);
    CASE(a_late_poll_hops_once_not_several_times);
    CASE(ble_only_detector_scans_at_once_and_rescans_5_s_after_the_last_start);
    CASE(ble_only_never_starts_a_scan_over_one_still_running);
    CASE(all_starts_on_wifi_showing_deauth_and_bursts_ble_every_12_s);
    CASE(a_mixed_group_alternates_but_keeps_its_own_active_detector);
    CASE(early_warning_sweeps_10_s_bursts_ble_then_rests_60_s);
    CASE(disabling_early_warning_idles_both_radios_and_clears_resting);
    CASE(a_manual_session_pauses_early_warning_and_stop_manual_resumes_it);
    CASE(disabling_early_warning_during_a_manual_session_leaves_the_session_alone);
    CASE(enabling_early_warning_during_a_manual_session_arms_only_at_stop_manual);
    CASE(stop_idles_both_radios_and_forgets_the_selection_but_not_the_enable);
    CASE(starting_none_is_a_stop);
    CASE(restarting_wifi_restarts_the_hop_clock_from_the_radio_clock);
    CHECK_SUMMARY();
}
