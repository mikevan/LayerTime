// Characterization tests for what src/ui/MeshtasticScreen asks the mesh to
// do: send on a channel, send direct, and add, edit, or delete a channel.
// Written in Phase 0 Step 5a against the screen as it stood, before those
// actions moved behind core commands.
//
// The screen is compiled against the test-only LVGL fake, driven by its own
// buttons, and read back from its labels. MeshtasticService needs the SX1262
// and crypto, so the methods the screen reaches are test-only fakes below
// that record each call and return what the test sets. Everything the tests
// need from the app goes through the Harness, so that when the screen is
// rewired only the Harness changes and every case stays as written.
//
// One limit of this harness: the screen reads the service's own channel
// table when it opens the channel editor, and that table is private to the
// service. So the editor always opens as for an unused slot, with DELETE
// hidden. pressHidden() fires DELETE's callback directly to reach that path.

#include "check.h"

#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>

#include "core/app/LayerTimeCore.h"
#include "platform/twatch_ultra/TUltraMeshTransports.h"
#include "ui/MeshtasticScreen.h"

// ---------------------------------------------------------------- link seams (test only)

namespace fake_mt {
struct Call {
    std::string what;
    uint32_t to = 0;
    uint8_t index = 0;
    std::string a, b;
};
inline std::vector<Call> g_calls;
inline bool g_setChannelOk = true;
inline uint8_t g_freeSlot = MeshtasticStatus::kMaxChannels;
}

bool MeshtasticService::sendChannelMessage(uint8_t channelIndex, const char *text)
{
    fake_mt::g_calls.push_back({"channel", 0, channelIndex, text ? text : "", ""});
    return true;
}
bool MeshtasticService::sendDirectMessage(uint32_t toNum, const char *text)
{
    fake_mt::g_calls.push_back({"direct", toNum, 0, text ? text : "", ""});
    return true;
}
bool MeshtasticService::setChannel(uint8_t index, const char *name, const char *key)
{
    fake_mt::g_calls.push_back({"setChannel", 0, index, name ? name : "", key ? key : ""});
    return fake_mt::g_setChannelOk;
}
bool MeshtasticService::removeChannel(uint8_t index)
{
    fake_mt::g_calls.push_back({"removeChannel", 0, index, "", ""});
    return true;
}
uint8_t MeshtasticService::freeChannelSlot() const { return fake_mt::g_freeSlot; }
void MeshtasticService::channelKeyText(const MeshtasticChannel &, char *out, size_t outSize)
{
    if (outSize) out[0] = '\0';
}
bool MeshtasticService::ownPosition(double &, double &) const { return false; }

void MapView::create(lv_obj_t *, int, int, int) {}
void MapView::setCenterCallback(CenterCallback, void *) {}
void MapView::setCenter(double, double) {}
void MapView::clearMarkers() {}
bool MapView::addMarker(double, double, const char *, lv_color_t, bool) { return true; }
void MapView::render() {}

// ---------------------------------------------------------------- harness

namespace {

// The core wired to the T-Ultra Meshtastic transport, as WatchApp wires it.
// Since Phase 0 Step 5 the conversation table and the screen's actions live
// in the core; only this Harness changed for that.
struct Harness {
    MeshtasticService service;
    layertime::twatch_ultra::TUltraMeshtasticTransport transport{service};
    layertime::LayerTimeCore core;
    MeshtasticScreen screen;
    std::unique_ptr<MeshtasticStatus> status{new MeshtasticStatus()};

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 100000;
        fake_mt::g_calls.clear();
        fake_mt::g_setChannelOk = true;
        fake_mt::g_freeSlot = MeshtasticStatus::kMaxChannels;
        layertime::CorePorts p;
        p.mesh[static_cast<uint8_t>(layertime::MeshNetwork::Meshtastic)] = &transport;
        core.attach(p);
        screen.create(&core, &service, nullptr, nullptr);
    }
    ~Harness() { fake_lv::reset(); }

    MeshtasticStatus &st() { return *status; }
    const std::vector<fake_mt::Call> &calls() const { return fake_mt::g_calls; }

    void channel(uint8_t i, const char *name)
    {
        st().channels[i].used = true;
        snprintf(st().channels[i].name, sizeof(st().channels[i].name), "%s", name);
        st().channels[i].pskLen = 16;
    }
    void directFrom(uint32_t from, const char *text)
    {
        MeshtasticMessage &m = st().messages[st().messageCount++];
        m = MeshtasticMessage{};
        m.used = true;
        m.fromNum = from;
        m.toNum = 0;  // us: nodeNum() is 0 until the radio starts
        m.receivedMs = 1000;
        snprintf(m.text, sizeof(m.text), "%s", text);
    }

    void show()
    {
        screen.show(st());
        render();
    }
    void render()
    {
        fake_arduino::g_millis += 2000;
        screen.render(st());
    }
    void tap(const char *label)
    {
        fake_lv::click(fake_lv::findVisibleLabel(label));
        render();
    }
    bool shows(const char *label) { return fake_lv::findVisibleLabel(label) != nullptr; }
    std::string labelStarting(const char *prefix)
    {
        lv_obj_t *l = fake_lv::findVisibleLabelStartingWith(prefix);
        return l ? l->text : std::string("<none>");
    }
    void pressHidden(const char *label)
    {
        for (lv_obj_t *o : fake_lv::g_objects)
            if (o->kind == FakeKind::Label && o->text == label && fake_lv::within(o, fake_lv::g_active)) {
                fake_lv::click(o);
                render();
                return;
            }
    }

    std::vector<lv_obj_t *> textAreas()
    {
        std::vector<lv_obj_t *> out;
        for (lv_obj_t *o : fake_lv::g_objects)
            if (o->kind == FakeKind::Textarea && fake_lv::within(o, fake_lv::g_active) && fake_lv::visible(o))
                out.push_back(o);
        return out;
    }
    lv_obj_t *composerText()
    {
        std::vector<lv_obj_t *> t = textAreas();
        return t.size() == 1 ? t[0] : nullptr;
    }
    void type(const std::string &s) { lv_textarea_set_text(composerText(), s.c_str()); }
    void fillChannel(const char *name, const char *key)
    {
        std::vector<lv_obj_t *> t = textAreas();
        if (t.size() != 2) return;
        lv_textarea_set_text(t[0], name);
        lv_textarea_set_text(t[1], key);
    }

    void openChannelThread(const char *title)
    {
        tap("CHATS");
        tap(title);
    }
};

} // namespace

// ---------------------------------------------------------------- sending

void send_on_a_channel_thread_goes_to_that_channel()
{
    Harness h;
    h.channel(0, "LongFast");
    h.channel(2, "Ops");
    h.show();
    h.openChannelThread("# Ops");
    h.tap("WRITE");
    h.tap("Copy that");
    h.tap("SEND");
    CHECK_INT(1, h.calls().size());
    if (h.calls().empty()) return;
    CHECK_STR("channel", h.calls()[0].what.c_str());
    CHECK_INT(2, h.calls()[0].index);
    CHECK_STR("Copy that", h.calls()[0].a.c_str());
    CHECK_TRUE(h.shows("WRITE"));  // back on the thread
}

void send_on_a_direct_thread_goes_to_that_node()
{
    Harness h;
    h.channel(0, "LongFast");
    h.directFrom(0x1234ABCD, "ping");
    h.show();
    h.tap("CHATS");
    h.tap("@ !1234abcd");
    h.tap("WRITE");
    CHECK_STR("Direct to: !1234abcd", h.labelStarting("Direct to:").c_str());
    h.tap("Radio check");
    h.tap("SEND");
    CHECK_INT(1, h.calls().size());
    if (h.calls().empty()) return;
    CHECK_STR("direct", h.calls()[0].what.c_str());
    CHECK_INT(0x1234ABCD, h.calls()[0].to);
    CHECK_STR("Radio check", h.calls()[0].a.c_str());
}

void composer_is_capped_at_160_and_a_full_message_goes_whole()
{
    Harness h;
    h.channel(0, "LongFast");
    h.show();
    h.openChannelThread("# LongFast");
    h.tap("WRITE");
    CHECK_INT(160, h.composerText() ? h.composerText()->maxLength : 0);
    const std::string full(160, 'y');
    h.type(full);
    h.tap("SEND");
    CHECK_INT(1, h.calls().size());
    if (!h.calls().empty()) CHECK_INT(160, h.calls()[0].a.size());
}

void empty_send_transmits_nothing_and_returns_to_the_thread()
{
    Harness h;
    h.channel(0, "LongFast");
    h.show();
    h.openChannelThread("# LongFast");
    h.tap("WRITE");
    h.tap("SEND");
    CHECK_INT(0, h.calls().size());
    CHECK_TRUE(h.shows("WRITE"));
}

void phrases_append_with_a_space_and_clear_empties()
{
    Harness h;
    h.channel(0, "LongFast");
    h.show();
    h.openChannelThread("# LongFast");
    h.tap("WRITE");
    h.tap("Yes");
    h.tap("OK");
    CHECK_STR("Yes OK", h.composerText() ? h.composerText()->text.c_str() : "<none>");
    h.tap("CLEAR");
    CHECK_STR("", h.composerText() ? h.composerText()->text.c_str() : "<none>");
}

void composer_phrases_are_the_library_in_order()
{
    Harness h;
    h.channel(0, "LongFast");
    h.show();
    h.openChannelThread("# LongFast");
    h.tap("WRITE");
    std::vector<std::string> p;
    for (lv_obj_t *l : fake_lv::visibleLabels())
        if (l->parent && l->parent->kind == FakeKind::Button && l->parent->userData != nullptr &&
            l->parent->callbacks.size() == 1)
            p.push_back(l->text);
    const char *const lib[] = {"Yes", "No", "OK", "On my way", "Be there in 10", "Almost there",
                               "Running late", "Where are you?", "Im here", "Heading back",
                               "Wait for me", "Need help", "All clear", "Copy that", "Standby",
                               "Call me", "Cant talk", "Radio check", "Low battery", "Good night"};
    CHECK_INT(20, p.size());
    for (size_t i = 0; i < p.size() && i < 20; ++i) CHECK_STR(lib[i], p[i].c_str());
}

// ---------------------------------------------------------------- channels

void add_channel_opens_the_editor_on_the_free_slot_and_saves_there()
{
    Harness h;
    h.channel(0, "LongFast");
    fake_mt::g_freeSlot = 3;
    h.show();
    h.tap("CHANNELS");
    CHECK_TRUE(h.shows("ADD CHANNEL"));
    h.tap("ADD CHANNEL");
    CHECK_TRUE(h.shows("SAVE"));
    CHECK_FALSE(h.shows("DELETE"));
    h.fillChannel("Ops", "AQ==");
    h.tap("SAVE");
    CHECK_INT(1, h.calls().size());
    if (h.calls().empty()) return;
    CHECK_STR("setChannel", h.calls()[0].what.c_str());
    CHECK_INT(3, h.calls()[0].index);
    CHECK_STR("Ops", h.calls()[0].a.c_str());
    CHECK_STR("AQ==", h.calls()[0].b.c_str());
    CHECK_TRUE(h.shows("ADD CHANNEL"));  // back on the channel list
}

void add_channel_is_hidden_when_no_slot_is_free()
{
    Harness h;
    h.channel(0, "LongFast");
    fake_mt::g_freeSlot = MeshtasticStatus::kMaxChannels;
    h.show();
    h.tap("CHANNELS");
    CHECK_FALSE(h.shows("ADD CHANNEL"));
}

void a_rejected_channel_shows_the_error_and_stays_in_the_editor()
{
    Harness h;
    h.channel(0, "LongFast");
    fake_mt::g_freeSlot = 1;
    fake_mt::g_setChannelOk = false;
    h.show();
    h.tap("CHANNELS");
    h.tap("ADD CHANNEL");
    h.fillChannel("", "zz");
    h.tap("SAVE");
    CHECK_INT(1, h.calls().size());
    CHECK_STR("Need a name (1-11 chars) and a base64 key or 0-10",
              h.labelStarting("Need a name").c_str());
    CHECK_TRUE(h.shows("SAVE"));
}

void the_primary_channel_row_does_not_open_the_editor()
{
    Harness h;
    h.channel(0, "LongFast");
    h.show();
    h.tap("CHANNELS");
    h.tap("0  LongFast");
    CHECK_FALSE(h.shows("SAVE"));
    CHECK_INT(0, h.calls().size());
}

void delete_removes_the_slot_and_drops_its_conversation()
{
    Harness h;
    h.channel(0, "LongFast");
    h.channel(2, "Ops");
    h.show();
    h.tap("CHATS");
    CHECK_TRUE(h.shows("# Ops"));
    h.tap("BACK");
    h.tap("CHANNELS");
    h.tap("2  Ops");
    CHECK_TRUE(h.shows("SAVE"));
    h.st().channels[2].used = false;  // what the service does on removal
    h.pressHidden("DELETE");
    CHECK_INT(1, h.calls().size());
    if (!h.calls().empty()) {
        CHECK_STR("removeChannel", h.calls()[0].what.c_str());
        CHECK_INT(2, h.calls()[0].index);
    }
    CHECK_TRUE(h.shows("ADD CHANNEL") || h.shows("0  LongFast"));  // back on the channel list
    h.tap("BACK");
    h.tap("CHATS");
    CHECK_FALSE(h.shows("# Ops"));
    CHECK_TRUE(h.shows("# LongFast"));
}

void cancel_in_the_editor_saves_nothing()
{
    Harness h;
    h.channel(0, "LongFast");
    fake_mt::g_freeSlot = 1;
    h.show();
    h.tap("CHANNELS");
    h.tap("ADD CHANNEL");
    h.fillChannel("Ops", "AQ==");
    h.tap("CANCEL");
    CHECK_INT(0, h.calls().size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(send_on_a_channel_thread_goes_to_that_channel);
    CASE(send_on_a_direct_thread_goes_to_that_node);
    CASE(composer_is_capped_at_160_and_a_full_message_goes_whole);
    CASE(empty_send_transmits_nothing_and_returns_to_the_thread);
    CASE(phrases_append_with_a_space_and_clear_empties);
    CASE(composer_phrases_are_the_library_in_order);
    CASE(add_channel_opens_the_editor_on_the_free_slot_and_saves_there);
    CASE(add_channel_is_hidden_when_no_slot_is_free);
    CASE(a_rejected_channel_shows_the_error_and_stays_in_the_editor);
    CASE(the_primary_channel_row_does_not_open_the_editor);
    CASE(delete_removes_the_slot_and_drops_its_conversation);
    CASE(cancel_in_the_editor_saves_nothing);
    CHECK_SUMMARY();
}
