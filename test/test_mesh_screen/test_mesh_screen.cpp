// Characterization tests for src/ui/MeshScreen, the MeshCore screen. Written
// in Phase 0 Step 5a against the screen as it stood, before its send moved
// behind a core command and its phrase list moved into core. Step 5 rewired
// the screen; only the Harness changed, and every case is as written in 5a.
//
// The screen is compiled against the test-only LVGL fake, driven by its own
// buttons, and read back from its labels. MeshService needs the SX1262 and
// crypto, so the one method the screen's send path reaches is a test-only
// fake below that records what would have gone out. Everything the tests
// need from the app goes through the Harness, so that when the screen is
// rewired only the Harness changes and every case stays as written.

#include "check.h"

#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>

#include "core/app/LayerTimeCore.h"
#include "platform/twatch_ultra/TUltraMeshTransports.h"
#include "ui/MeshScreen.h"

// ---------------------------------------------------------------- link seams (test only)

namespace fake_meshcore {
inline std::vector<std::string> g_sent;
}

bool MeshService::sendPublicMessage(const char *text)
{
    fake_meshcore::g_sent.push_back(text ? text : "");
    return true;
}

// ---------------------------------------------------------------- harness

namespace {

// The core wired to the T-Ultra MeshCore transport, as WatchApp wires it.
struct Harness {
    MeshService service;
    layertime::twatch_ultra::TUltraMeshCoreTransport transport{service};
    layertime::LayerTimeCore core;
    MeshScreen screen;
    std::unique_ptr<MeshStatus> status{new MeshStatus()};

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 100000;
        fake_meshcore::g_sent.clear();
        layertime::CorePorts p;
        p.mesh[static_cast<uint8_t>(layertime::MeshNetwork::MeshCore)] = &transport;
        core.attach(p);
        screen.create(&core, &service, nullptr, nullptr);
    }
    ~Harness() { fake_lv::reset(); }

    MeshStatus &st() { return *status; }
    void show() { screen.show(st()); }
    void render() { screen.render(st()); }
    const std::vector<std::string> &sent() const { return fake_meshcore::g_sent; }

    void node(uint8_t slot, const char *name, uint8_t type, float rssi, uint32_t lastSeenMs)
    {
        auto &n = st().nodes[slot];
        n.used = true;
        snprintf(n.name, sizeof(n.name), "%s", name);
        n.type = type;
        n.rssi = rssi;
        n.lastSeenMs = lastSeenMs;
        ++st().nodeCount;
    }
    void message(uint8_t slot, const char *text, float rssi)
    {
        auto &m = st().messages[slot];
        m.used = true;
        snprintf(m.text, sizeof(m.text), "%s", text);
        m.rssi = rssi;
        ++st().messageCount;
    }

    std::string labelStarting(std::initializer_list<const char *> prefixes)
    {
        for (const char *p : prefixes) {
            lv_obj_t *l = fake_lv::findVisibleLabelStartingWith(p);
            if (l) return l->text;
        }
        return "<none>";
    }
    std::string statusLine()
    {
        return labelStarting({"RADIO NOT SUPPORTED", "MESHCORE OFF", "ONLINE", "RADIO ERROR"});
    }
    uint32_t statusColor()
    {
        for (const char *p : {"RADIO NOT SUPPORTED", "MESHCORE OFF", "ONLINE", "RADIO ERROR"}) {
            lv_obj_t *l = fake_lv::findVisibleLabelStartingWith(p);
            if (l) return l->textColor;
        }
        return 0;
    }
    std::string summary()
    {
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.find(" NODES  |  ") != std::string::npos) return l->text;
        return "<none>";
    }
    std::string nodes()
    {
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.rfind("No node adverts", 0) == 0 || l->text.find("dBm ") != std::string::npos)
                return l->text;
        return "<none>";
    }
    std::string messages() { return labelStarting({"No public messages", "RX  ", "TX  "}); }

    // ---- composer
    bool composerOpen() { return fake_lv::findVisibleLabel("PUBLIC MESH CHAT") != nullptr; }
    lv_obj_t *textArea() { return fake_lv::findVisibleTextarea(); }
    std::string text()
    {
        lv_obj_t *t = textArea();
        return t ? t->text : std::string("<closed>");
    }
    void tap(const char *label) { fake_lv::click(fake_lv::findVisibleLabel(label)); }
    void type(const std::string &s) { lv_textarea_set_text(textArea(), s.c_str()); }
    std::vector<std::string> phrases()
    {
        std::vector<std::string> out;
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->parent && l->parent->kind == FakeKind::Button && l->parent->userData != nullptr)
                out.push_back(l->text);
        return out;
    }
};

// The T-Ultra's shipped phrase list, in order (contracts/vectors/
// quick_messages_default.json).
const char *const kLibrary[] = {"Yes", "No", "OK", "On my way", "Be there in 10", "Almost there",
                                "Running late", "Where are you?", "Im here", "Heading back",
                                "Wait for me", "Need help", "All clear", "Copy that", "Standby",
                                "Call me", "Cant talk", "Radio check", "Low battery", "Good night"};

} // namespace

// ---------------------------------------------------------------- status

void status_line_follows_the_radio_state()
{
    Harness h;
    h.show();
    CHECK_STR("RADIO NOT SUPPORTED", h.statusLine().c_str());
    CHECK_INT(0xD99A24, h.statusColor());

    h.st().supported = true;
    h.render();
    CHECK_STR("MESHCORE OFF - enable in Settings", h.statusLine().c_str());

    h.st().radioEnabled = true;
    h.st().radioError = -2;
    h.render();
    CHECK_STR("RADIO ERROR -2", h.statusLine().c_str());
    CHECK_INT(0xD99A24, h.statusColor());

    h.st().radioReady = true;
    snprintf(h.st().nodeName, sizeof(h.st().nodeName), "%s", "LT-ab12");
    h.render();
    CHECK_STR("ONLINE  LT-ab12", h.statusLine().c_str());
    CHECK_INT(0x63E06B, h.statusColor());
}

void summary_counts_nodes_messages_and_frames()
{
    Harness h;
    h.node(0, "alpha", 1, -80.0f, 100000);
    h.message(0, "hi", -70.0f);
    h.st().packetCount = 1234;
    h.show();
    CHECK_STR("1 NODES  |  1 MSG  |  1234 FRAMES", h.summary().c_str());
}

void empty_lists_say_so()
{
    Harness h;
    h.show();
    CHECK_STR("No node adverts heard yet.", h.nodes().c_str());
    CHECK_STR("No public messages heard yet.\nTap CHAT to transmit.", h.messages().c_str());
}

void node_lines_show_name_type_rssi_and_age_in_slot_order()
{
    Harness h;
    h.node(0, "alpha", 1, -80.4f, 100000 - 12500);
    h.node(2, "rpt", 2, -101.6f, 100000);
    h.node(3, "room", 3, -60.0f, 100000 - 1000);
    h.node(5, "sens", 4, -70.0f, 100000);
    h.node(6, "odd", 9, -75.0f, 100000);
    h.show();
    CHECK_STR("alpha [CHAT] -80dBm 12s\nrpt [REPEATER] -102dBm 0s\nroom [ROOM] -60dBm 1s\n"
              "sens [SENSOR] -70dBm 0s\nodd [NODE] -75dBm 0s\n",
              h.nodes().c_str());
}

void messages_are_rx_when_rssi_is_set_and_tx_otherwise()
{
    Harness h;
    h.message(0, "LT-ab12: hello", 0.0f);
    h.message(1, "bob: hi back", -88.0f);
    h.message(3, "carol: later", -91.0f);
    h.show();
    CHECK_STR("TX  LT-ab12: hello\nRX  bob: hi back\nRX  carol: later\n", h.messages().c_str());
}

// ---------------------------------------------------------------- composer

void chat_opens_an_empty_composer_capped_at_110()
{
    Harness h;
    h.show();
    CHECK_FALSE(h.composerOpen());
    h.tap("CHAT");
    CHECK_TRUE(h.composerOpen());
    CHECK_STR("", h.text().c_str());
    CHECK_INT(110, h.textArea() ? h.textArea()->maxLength : 0);
}

void phrases_are_the_library_in_order()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    const std::vector<std::string> p = h.phrases();
    CHECK_INT(20, p.size());
    for (size_t i = 0; i < p.size() && i < 20; ++i) CHECK_STR(kLibrary[i], p[i].c_str());
}

void a_phrase_appends_with_a_space()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("On my way");
    CHECK_STR("On my way", h.text().c_str());
    h.tap("Be there in 10");
    CHECK_STR("On my way Be there in 10", h.text().c_str());
}

void clear_empties_the_text()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("Yes");
    h.tap("CLEAR");
    CHECK_STR("", h.text().c_str());
    CHECK_TRUE(h.composerOpen());
}

void cancel_closes_without_sending()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("Yes");
    h.tap("CANCEL");
    CHECK_FALSE(h.composerOpen());
    CHECK_INT(0, h.sent().size());
}

void send_transmits_the_text_on_the_public_channel_then_closes()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("Need help");
    h.tap("SEND");
    CHECK_FALSE(h.composerOpen());
    CHECK_INT(1, h.sent().size());
    if (!h.sent().empty()) CHECK_STR("Need help", h.sent()[0].c_str());
}

void send_passes_typed_text_through_whole()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    const std::string full(110, 'x');
    h.type(full);
    h.tap("SEND");
    CHECK_INT(1, h.sent().size());
    if (!h.sent().empty()) CHECK_STR(full.c_str(), h.sent()[0].c_str());
}

void send_with_no_text_transmits_nothing_and_closes()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("SEND");
    CHECK_FALSE(h.composerOpen());
    CHECK_INT(0, h.sent().size());
}

void reopening_the_composer_starts_empty()
{
    Harness h;
    h.show();
    h.tap("CHAT");
    h.tap("Yes");
    h.tap("CANCEL");
    h.tap("CHAT");
    CHECK_STR("", h.text().c_str());
}

void after_send_the_screen_redraws_from_the_services_own_status()
{
    // The screen redraws straight after SEND from the service's status, not
    // from the status it was last handed. A default service is unsupported.
    Harness h;
    h.st().supported = true;
    h.st().radioEnabled = true;
    h.st().radioReady = true;
    h.show();
    CHECK_STR("ONLINE  ", h.statusLine().c_str());
    h.tap("CHAT");
    h.tap("Yes");
    h.tap("SEND");
    CHECK_STR("RADIO NOT SUPPORTED", h.statusLine().c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(status_line_follows_the_radio_state);
    CASE(summary_counts_nodes_messages_and_frames);
    CASE(empty_lists_say_so);
    CASE(node_lines_show_name_type_rssi_and_age_in_slot_order);
    CASE(messages_are_rx_when_rssi_is_set_and_tx_otherwise);
    CASE(chat_opens_an_empty_composer_capped_at_110);
    CASE(phrases_are_the_library_in_order);
    CASE(a_phrase_appends_with_a_space);
    CASE(clear_empties_the_text);
    CASE(cancel_closes_without_sending);
    CASE(send_transmits_the_text_on_the_public_channel_then_closes);
    CASE(send_passes_typed_text_through_whole);
    CASE(send_with_no_text_transmits_nothing_and_closes);
    CASE(reopening_the_composer_starts_empty);
    CASE(after_send_the_screen_redraws_from_the_services_own_status);
    CHECK_SUMMARY();
}
