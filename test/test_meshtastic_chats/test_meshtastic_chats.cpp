// Characterization tests for the Meshtastic conversation and unread rules in
// src/ui/MeshtasticScreen.cpp, as it stands.
//
// The screen is compiled unchanged against the test-only LVGL fake in
// test/stubs/, driven through its public create/show/render API, and read
// back from the labels it puts on screen, exactly what the wearer sees:
// the CHATS tile, the chat rows, their unread badges, and the thread.
//
// Link seams: MeshtasticScreen calls into MeshtasticService and MapView,
// whose real implementations need the radio, crypto, and the SD card. The
// few methods it references are defined below as test-only fakes. They are
// not reached by any test here except where noted.
//
// Our own node number comes from MeshtasticService::nodeNum(), an inline
// accessor over a private field that is 0 until the radio starts. The tests
// therefore run with "us" = node 0, and a direct message to us is one whose
// toNum is 0.

#include "check.h"

#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>

#include "ui/MeshtasticScreen.h"

// ---------------------------------------------------------------- link seams (test only)

namespace fake_mesh {
struct Sent { bool channel; uint32_t to; uint8_t index; std::string text; };
inline std::vector<Sent> g_sent;
}

bool MeshtasticService::sendChannelMessage(uint8_t channelIndex, const char *text)
{
    fake_mesh::g_sent.push_back({true, 0, channelIndex, text ? text : ""});
    return true;
}
bool MeshtasticService::sendDirectMessage(uint32_t toNum, const char *text)
{
    fake_mesh::g_sent.push_back({false, toNum, 0, text ? text : ""});
    return true;
}
bool MeshtasticService::setChannel(uint8_t, const char *, const char *) { return true; }
bool MeshtasticService::removeChannel(uint8_t) { return true; }
uint8_t MeshtasticService::freeChannelSlot() const { return MeshtasticStatus::kMaxChannels; }
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

constexpr uint32_t kUs = 0;

struct Harness {
    MeshtasticService service;
    MeshtasticScreen screen;
    std::unique_ptr<MeshtasticStatus> status{new MeshtasticStatus()};

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 100000;
        fake_mesh::g_sent.clear();
        screen.create(&service, nullptr, nullptr);
    }
    ~Harness() { fake_lv::reset(); }

    MeshtasticStatus &st() { return *status; }

    void channel(uint8_t i, const char *name, uint8_t pskLen)
    {
        st().channels[i].used = true;
        snprintf(st().channels[i].name, sizeof(st().channels[i].name), "%s", name);
        st().channels[i].pskLen = pskLen;
    }

    void node(uint32_t num, const char *longName, const char *shortName)
    {
        for (MeshtasticNode &n : st().nodes) {
            if (n.used) continue;
            n.used = true;
            n.num = num;
            snprintf(n.longName, sizeof(n.longName), "%s", longName);
            snprintf(n.shortName, sizeof(n.shortName), "%s", shortName);
            ++st().nodeCount;
            return;
        }
    }

    // Appends a message in the next free slot, as the service would.
    MeshtasticMessage &message(uint32_t from, uint32_t to, uint8_t ch, const char *text,
                               uint32_t receivedMs, bool ours = false)
    {
        MeshtasticMessage &m = st().messages[st().messageCount++];
        m = MeshtasticMessage{};
        m.used = true;
        m.fromNum = from;
        m.toNum = to;
        m.channel = ch;
        m.isOurs = ours;
        m.receivedMs = receivedMs;
        snprintf(m.text, sizeof(m.text), "%s", text);
        return m;
    }

    // Opens the screen the way WatchApp does, then runs one 250 ms tick.
    // show() renders before it loads the screen, and render() does nothing
    // while the screen is not active, so the tick is what first fills it in.
    void show()
    {
        screen.show(st());
        render();
    }

    // Advances past the 2 s list refresh so the next render rebuilds lists.
    void render()
    {
        fake_arduino::g_millis += 2000;
        screen.render(st());
    }

    // The CHATS tile shows either "N new" or the total message count.
    std::string chatsTile()
    {
        lv_obj_t *name = fake_lv::findVisibleLabel("CHATS");
        if (!name || !name->parent || name->parent->children.empty()) return "<no tile>";
        return name->parent->children.front()->text;
    }

    void openChats()
    {
        fake_lv::click(fake_lv::findVisibleLabel("CHATS"));
        render();
    }

    // A chat row, found by its title label.
    lv_obj_t *row(const std::string &title)
    {
        lv_obj_t *t = fake_lv::findVisibleLabel(title);
        return t ? t->parent : nullptr;
    }

    // Texts of the labels on a row, in order: title, preview, [badge].
    std::vector<std::string> rowTexts(const std::string &title)
    {
        std::vector<std::string> out;
        lv_obj_t *r = row(title);
        if (!r) return out;
        for (lv_obj_t *c : r->children)
            if (c->kind == FakeKind::Label) out.push_back(c->text);
        return out;
    }

    std::vector<std::string> chatTitles()
    {
        std::vector<std::string> out;
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.rfind("# ", 0) == 0 || l->text.rfind("@ ", 0) == 0) out.push_back(l->text);
        return out;
    }

    void openThread(const std::string &title)
    {
        fake_lv::click(fake_lv::findVisibleLabel(title));
        render();
    }

    void back() { fake_lv::click(fake_lv::findVisibleLabel("BACK")); }

    // Message bodies in the open thread, oldest first.
    std::vector<std::string> threadBodies()
    {
        std::vector<std::string> out;
        const std::vector<lv_obj_t *> labels = fake_lv::visibleLabels();
        for (lv_obj_t *l : labels) {
            // A bubble holds a header label then a body label.
            if (l->parent && l->parent->children.size() >= 2 && l->parent->children[1] == l &&
                l->parent->children[0]->kind == FakeKind::Label)
                out.push_back(l->text);
        }
        return out;
    }
};

std::string join(const std::vector<std::string> &v)
{
    std::string s;
    for (const std::string &x : v) s += (s.empty() ? "" : " | ") + x;
    return s;
}

} // namespace

// ---------------------------------------------------------------- which conversations exist

void every_configured_channel_is_a_conversation_even_when_empty()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    h.channel(3, "Team", 32);
    h.show();
    h.openChats();
    CHECK_STR("# LongFast | # Team", join(h.chatTitles()).c_str());
    CHECK_STR("# Team | AES256", join(h.rowTexts("# Team")).c_str());
    CHECK_STR("# LongFast | AES128", join(h.rowTexts("# LongFast")).c_str());
}

void channel_without_a_key_is_labelled_open()
{
    Harness h;
    h.channel(0, "Public", 0);
    h.show();
    h.openChats();
    CHECK_STR("# Public | OPEN", join(h.rowTexts("# Public")).c_str());
}

void a_direct_conversation_starts_with_a_message_either_way()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    h.node(0x1111, "Alpha", "ALP");
    h.node(0x2222, "Bravo", "BRV");
    h.message(0x1111, kUs, 0, "to us", 1000);            // inbound DM
    h.message(kUs, 0x2222, 0, "from us", 2000, true);    // outbound DM
    h.show();
    h.openChats();
    CHECK_STR("# LongFast | @ Alpha | @ Bravo", join(h.chatTitles()).c_str());
}

void traffic_between_two_other_nodes_is_not_a_conversation()
{
    Harness h;
    h.message(0x1111, 0x2222, 0, "not for us", 1000);
    h.show();
    h.openChats();
    CHECK_STR("", join(h.chatTitles()).c_str());
}

void a_direct_conversation_outlives_its_messages()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "hi", 1000);
    h.show();
    h.st().messages[0] = MeshtasticMessage{};
    h.st().messageCount = 0;
    h.openChats();
    CHECK_STR("@ Alpha | No messages yet", join(h.rowTexts("@ Alpha")).c_str());
}

void unknown_nodes_are_named_by_hex_number_then_short_name()
{
    Harness h;
    h.node(0x2222, "", "BRV");
    h.message(0xABCD1234, kUs, 0, "a", 1000);
    h.message(0x2222, kUs, 0, "b", 1001);
    h.show();
    h.openChats();
    CHECK_STR("@ !abcd1234 | @ BRV", join(h.chatTitles()).c_str());
}

void conversation_list_is_capped_at_24()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    for (uint32_t i = 1; i <= 24; ++i) h.message(0x1000 + i, kUs, 0, "x", 1000 + i);
    h.show();
    h.openChats();
    const std::vector<std::string> titles = h.chatTitles();
    CHECK_INT(24, titles.size());  // the channel plus the first 23 peers
    CHECK_TRUE(h.row("@ !00001017") != nullptr);
    CHECK_TRUE(h.row("@ !00001018") == nullptr);
}

// ---------------------------------------------------------------- which messages belong where

void channel_messages_follow_their_channel_index()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    h.channel(1, "Team", 32);
    h.message(0x1111, kMeshtasticBroadcast, 1, "team msg", 1000);
    h.message(0x1111, kMeshtasticBroadcast, 0, "public msg", 1001);
    h.show();
    h.openChats();
    h.openThread("# Team");
    CHECK_STR("team msg", join(h.threadBodies()).c_str());
}

void a_direct_thread_ignores_the_channel_it_arrived_on()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "on zero", 1000);
    h.message(0x1111, kUs, 2, "on two", 1001);
    h.message(kUs, 0x1111, 5, "reply", 1002, true);
    h.show();
    h.openChats();
    CHECK_STR("@ Alpha", join(h.chatTitles()).c_str());
    h.openThread("@ Alpha");
    CHECK_STR("on zero | on two | reply", join(h.threadBodies()).c_str());
}

void preview_is_the_latest_message_with_you_prefix_and_36_characters()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "older", 1000);
    h.message(kUs, 0x1111, 0, "0123456789012345678901234567890123456789", 2000, true);
    h.show();
    h.openChats();
    CHECK_STR("You: 012345678901234567890123456789012345",
              h.rowTexts("@ Alpha")[1].c_str());
}

void preview_tie_on_time_goes_to_the_later_slot()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "first", 5000);
    h.message(0x1111, kUs, 0, "second", 5000);
    h.show();
    h.openChats();
    CHECK_STR("second", h.rowTexts("@ Alpha")[1].c_str());
}

// ---------------------------------------------------------------- unread

void show_alone_leaves_the_home_tiles_empty_until_the_next_tick()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "a", 1000);
    h.screen.show(h.st());
    CHECK_STR("", h.chatsTile().c_str());
    h.screen.render(h.st());
    CHECK_STR("1 new", h.chatsTile().c_str());
}

void chats_tile_shows_new_count_or_total()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(kUs, 0x1111, 0, "mine", 1000, true);
    h.show();
    CHECK_STR("1", h.chatsTile().c_str());  // our own message is never unread
    h.message(0x1111, kUs, 0, "theirs", 2000);
    h.message(0x1111, kUs, 0, "again", 3000);
    h.render();
    CHECK_STR("2 new", h.chatsTile().c_str());
}

void unread_badge_counts_inbound_messages_per_conversation()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "a", 1000);
    h.message(0x1111, kUs, 0, "b", 1001);
    h.message(0x2222, kMeshtasticBroadcast, 0, "c", 1002);
    h.show();
    h.openChats();
    CHECK_STR("@ Alpha | b | 2", join(h.rowTexts("@ Alpha")).c_str());
    CHECK_STR("# LongFast | c | 1", join(h.rowTexts("# LongFast")).c_str());
}

void opening_a_thread_marks_it_read_at_the_current_time()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "a", 1000);
    h.show();
    h.openChats();
    h.openThread("@ Alpha");
    const uint32_t viewedAt = fake_arduino::g_millis - 2000;  // openThread ran before render()
    h.back();
    h.render();
    CHECK_STR("@ Alpha | a", join(h.rowTexts("@ Alpha")).c_str());

    // Arriving exactly at the viewed time is not unread; one ms later is.
    h.message(0x1111, kUs, 0, "same ms", viewedAt);
    h.render();
    CHECK_STR("@ Alpha | same ms", join(h.rowTexts("@ Alpha")).c_str());
    h.message(0x1111, kUs, 0, "later", viewedAt + 1);
    h.render();
    CHECK_STR("@ Alpha | later | 1", join(h.rowTexts("@ Alpha")).c_str());
}

void read_state_is_per_conversation()
{
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.node(0x2222, "Bravo", "BRV");
    h.message(0x1111, kUs, 0, "a", 1000);
    h.message(0x2222, kUs, 0, "b", 1000);
    h.show();
    h.openChats();
    h.openThread("@ Alpha");
    h.back();
    h.render();
    CHECK_STR("@ Alpha | a", join(h.rowTexts("@ Alpha")).c_str());
    CHECK_STR("@ Bravo | b | 1", join(h.rowTexts("@ Bravo")).c_str());
    h.back();  // Chats -> Home
    h.render();
    CHECK_STR("1 new", h.chatsTile().c_str());
}

void messages_before_boot_time_zero_are_unread_until_opened()
{
    // lastViewedMs starts at 0, so anything received after boot is unread.
    Harness h;
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kUs, 0, "a", 1);
    h.show();
    CHECK_STR("1 new", h.chatsTile().c_str());
}

void thread_lists_oldest_first_and_only_its_own_messages()
{
    Harness h;
    h.channel(0, "LongFast", 16);
    h.node(0x1111, "Alpha", "ALP");
    h.message(0x1111, kMeshtasticBroadcast, 0, "one", 1000);
    h.message(0x1111, kUs, 0, "dm", 1500);
    h.message(0x2222, kMeshtasticBroadcast, 0, "two", 2000);
    h.show();
    h.openChats();
    h.openThread("# LongFast");
    CHECK_STR("one | two", join(h.threadBodies()).c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(every_configured_channel_is_a_conversation_even_when_empty);
    CASE(channel_without_a_key_is_labelled_open);
    CASE(a_direct_conversation_starts_with_a_message_either_way);
    CASE(traffic_between_two_other_nodes_is_not_a_conversation);
    CASE(a_direct_conversation_outlives_its_messages);
    CASE(unknown_nodes_are_named_by_hex_number_then_short_name);
    CASE(conversation_list_is_capped_at_24);
    CASE(channel_messages_follow_their_channel_index);
    CASE(a_direct_thread_ignores_the_channel_it_arrived_on);
    CASE(preview_is_the_latest_message_with_you_prefix_and_36_characters);
    CASE(preview_tie_on_time_goes_to_the_later_slot);
    CASE(show_alone_leaves_the_home_tiles_empty_until_the_next_tick);
    CASE(chats_tile_shows_new_count_or_total);
    CASE(unread_badge_counts_inbound_messages_per_conversation);
    CASE(opening_a_thread_marks_it_read_at_the_current_time);
    CASE(read_state_is_per_conversation);
    CASE(messages_before_boot_time_zero_are_unread_until_opened);
    CASE(thread_lists_oldest_first_and_only_its_own_messages);
    CHECK_SUMMARY();
}
