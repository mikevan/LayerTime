#include "MeshtasticScreen.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "Theme.h"

namespace {
constexpr uint32_t kListRefreshMs = 2000;

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y, int w,
                    const lv_font_t *font, lv_color_t color,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                     lv_color_t border, lv_color_t textColor, lv_event_cb_t cb, void *userData)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, Theme::background(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, border, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, textColor, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return button;
}

lv_obj_t *makeList(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_pos(list, 0, 64);
    lv_obj_set_size(list, 410, 438);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    return list;
}

void ageText(uint32_t ageMs, char *out, size_t outSize)
{
    const uint32_t s = ageMs / 1000;
    if (s < 60) snprintf(out, outSize, "%lus", static_cast<unsigned long>(s));
    else if (s < 3600) snprintf(out, outSize, "%lum", static_cast<unsigned long>(s / 60));
    else snprintf(out, outSize, "%luh", static_cast<unsigned long>(s / 3600));
}
}

// ---------------------------------------------------------------- build

void MeshtasticScreen::create(MeshtasticService *service, BackCallback backCallback, void *userData)
{
    _service = service;
    _backCallback = backCallback;
    _userData = userData;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    buildHome();
    buildNodesPage();
    buildChatsPage();
    buildThreadPage();
    buildInfoPage();
    buildComposer();
    buildChannelsPage();
    buildChannelEditor();
    buildMapPage();
    buildTopBar(); // last, so it draws above every page
    showPage(Page::Home);
}

lv_obj_t *MeshtasticScreen::makePage()
{
    lv_obj_t *page = lv_obj_create(_screen);
    lv_obj_set_size(page, 410, 502);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

void MeshtasticScreen::buildTopBar()
{
    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_size(back, 82, 40);
    lv_obj_set_pos(back, 12, 12);
    lv_obj_set_style_bg_color(back, Theme::background(), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(back, Theme::gold(), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_radius(back, 8, 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backLabel = lv_label_create(back);
    lv_label_set_text(backLabel, "BACK");
    lv_obj_set_style_text_color(backLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(backLabel);

    _title = makeLabel(_screen, "MESHTASTIC", 105, 16, 300, &lv_font_montserrat_24,
                       Theme::green(), LV_TEXT_ALIGN_CENTER);
}

lv_obj_t *MeshtasticScreen::makeTile(lv_obj_t *parent, const char *label, int x, int y,
                                     lv_color_t color, lv_event_cb_t cb, lv_obj_t **valueOut)
{
    lv_obj_t *tile = lv_button_create(parent);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, 184, 110);
    lv_obj_set_style_bg_color(tile, Theme::background(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tile, color, 0);
    lv_obj_set_style_border_width(tile, 2, 0);
    lv_obj_set_style_radius(tile, 12, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, this);

    lv_obj_t *value = makeLabel(tile, "", 0, 14, 184, &lv_font_montserrat_28, Theme::white(),
                                LV_TEXT_ALIGN_CENTER);
    lv_obj_t *name = makeLabel(tile, label, 0, 68, 184, &lv_font_montserrat_16, color,
                               LV_TEXT_ALIGN_CENTER);
    (void)name;
    if (valueOut != nullptr) *valueOut = value;
    return tile;
}

void MeshtasticScreen::buildHome()
{
    _home = makePage();
    _statusLine = makeLabel(_home, "", 12, 66, 386, &lv_font_montserrat_16, Theme::muted(),
                            LV_TEXT_ALIGN_CENTER);

    // MUI's home is a dashboard: status, then big tiles into each section.
    makeTile(_home, "NODES", 12, 104, Theme::teal(), nodesTileThunk, &_nodesValue);
    makeTile(_home, "CHATS", 214, 104, Theme::green(), chatsTileThunk, &_chatsValue);
    makeTile(_home, "CHANNELS", 12, 226, Theme::gold(), channelsTileThunk, &_channelsValue);
    lv_obj_t *mapValue = nullptr;
    makeTile(_home, "MAP", 214, 226, Theme::blue(), mapTileThunk, &mapValue);
    lv_label_set_text(mapValue, "--");
    lv_obj_t *infoValue = nullptr;
    makeTile(_home, "INFO", 12, 348, Theme::muted(), infoTileThunk, &infoValue);
    lv_label_set_text(infoValue, "i");

    makeLabel(_home, "US / LongFast\n906.875 MHz", 214, 380, 184, &lv_font_montserrat_16,
              Theme::muted(), LV_TEXT_ALIGN_CENTER);
}

void MeshtasticScreen::buildNodesPage()
{
    _nodesPage = makePage();
    _nodesList = makeList(_nodesPage);
}

void MeshtasticScreen::buildChatsPage()
{
    _chatsPage = makePage();
    _chatsList = makeList(_chatsPage);
}

void MeshtasticScreen::buildThreadPage()
{
    _threadPage = makePage();
    _threadList = makeList(_threadPage);
    lv_obj_set_size(_threadList, 410, 382);
    makeButton(_threadPage, "WRITE", 12, 452, 386, 42, Theme::green(), Theme::green(),
               writeThunk, this);
}

void MeshtasticScreen::buildInfoPage()
{
    _infoPage = makePage();
    _infoText = makeLabel(_infoPage, "", 16, 72, 378, &lv_font_montserrat_16, Theme::white());
    lv_label_set_long_mode(_infoText, LV_LABEL_LONG_WRAP);
}

void MeshtasticScreen::buildComposer()
{
    _composer = makePage();
    lv_obj_set_style_bg_color(_composer, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_composer, LV_OPA_COVER, 0);

    _composeTitle = makeLabel(_composer, "", 12, 66, 386, &lv_font_montserrat_16, Theme::muted(),
                              LV_TEXT_ALIGN_CENTER);

    _textArea = lv_textarea_create(_composer);
    lv_obj_set_size(_textArea, 386, 90);
    lv_obj_set_pos(_textArea, 12, 92);
    lv_textarea_set_one_line(_textArea, false);
    lv_textarea_set_max_length(_textArea, 160);
    lv_textarea_set_placeholder_text(_textArea, "Message...");
    lv_obj_set_style_text_font(_textArea, &lv_font_montserrat_20, 0);

    makeButton(_composer, "CANCEL", 12, 190, 186, 44, Theme::muted(), Theme::white(),
               cancelThunk, this);
    makeButton(_composer, "SEND", 212, 190, 186, 44, Theme::green(), Theme::green(),
               sendThunk, this);

    _keyboard = lv_keyboard_create(_composer);
    lv_obj_set_size(_keyboard, 410, 256);
    lv_obj_set_pos(_keyboard, 0, 246);
    lv_keyboard_set_textarea(_keyboard, _textArea);
}

void MeshtasticScreen::buildChannelsPage()
{
    _channelsPage = makePage();
    _channelsList = makeList(_channelsPage);
    lv_obj_set_size(_channelsList, 410, 382);
    _addChannelButton = makeButton(_channelsPage, "ADD CHANNEL", 12, 452, 386, 42, Theme::gold(),
                                   Theme::gold(), addChannelThunk, this);
}

void MeshtasticScreen::buildChannelEditor()
{
    _channelEditor = makePage();
    lv_obj_set_style_bg_color(_channelEditor, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_channelEditor, LV_OPA_COVER, 0);

    makeLabel(_channelEditor, "NAME", 12, 66, 80, &lv_font_montserrat_14, Theme::muted());
    _channelNameInput = lv_textarea_create(_channelEditor);
    lv_obj_set_size(_channelNameInput, 306, 42);
    lv_obj_set_pos(_channelNameInput, 92, 58);
    lv_textarea_set_one_line(_channelNameInput, true);
    lv_textarea_set_max_length(_channelNameInput, 11);
    lv_textarea_set_placeholder_text(_channelNameInput, "as in the app");
    lv_obj_set_style_text_font(_channelNameInput, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(_channelNameInput, channelFieldThunk, LV_EVENT_CLICKED, this);

    makeLabel(_channelEditor, "KEY", 12, 116, 80, &lv_font_montserrat_14, Theme::muted());
    _channelKeyInput = lv_textarea_create(_channelEditor);
    lv_obj_set_size(_channelKeyInput, 306, 42);
    lv_obj_set_pos(_channelKeyInput, 92, 108);
    lv_textarea_set_one_line(_channelKeyInput, true);
    lv_textarea_set_max_length(_channelKeyInput, 48);
    lv_textarea_set_placeholder_text(_channelKeyInput, "base64 PSK or 0-10");
    lv_obj_set_style_text_font(_channelKeyInput, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(_channelKeyInput, channelFieldThunk, LV_EVENT_CLICKED, this);

    _channelError = makeLabel(_channelEditor, "", 12, 158, 386, &lv_font_montserrat_14, Theme::danger(),
                              LV_TEXT_ALIGN_CENTER);

    makeButton(_channelEditor, "CANCEL", 12, 180, 120, 42, Theme::muted(), Theme::white(), cancelThunk, this);
    makeButton(_channelEditor, "SAVE", 145, 180, 120, 42, Theme::green(), Theme::green(), channelSaveThunk, this);
    _channelDeleteButton = makeButton(_channelEditor, "DELETE", 278, 180, 120, 42, Theme::danger(),
                                      Theme::danger(), channelDeleteThunk, this);

    _channelKeyboard = lv_keyboard_create(_channelEditor);
    lv_obj_set_size(_channelKeyboard, 410, 270);
    lv_obj_set_pos(_channelKeyboard, 0, 232);
    lv_keyboard_set_textarea(_channelKeyboard, _channelNameInput);
}

void MeshtasticScreen::buildMapPage()
{
    _mapPage = makePage();
    // Canvas from just under the top bar to just above the button row.
    _map.create(_mapPage, 64, 410, 382);
    _map.setCenterCallback(mapCenterThunk, this);
}

// ---------------------------------------------------------------- navigation

void MeshtasticScreen::showPage(Page page)
{
    _page = page;
    lv_obj_t *pages[] = {_home, _nodesPage, _chatsPage, _threadPage, _infoPage, _composer,
                         _channelsPage, _channelEditor, _mapPage};
    for (lv_obj_t *p : pages) if (p) lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *target = _home;
    const char *title = "MESHTASTIC";
    switch (page) {
        case Page::Nodes: target = _nodesPage; title = "NODES"; break;
        case Page::Chats: target = _chatsPage; title = "CHATS"; break;
        case Page::Thread: target = _threadPage; title = "CHAT"; break;
        case Page::Info: target = _infoPage; title = "INFO"; break;
        case Page::Compose: target = _composer; title = "WRITE"; break;
        case Page::Channels: target = _channelsPage; title = "CHANNELS"; break;
        case Page::ChannelEdit: target = _channelEditor; title = "CHANNEL"; break;
        case Page::Map: target = _mapPage; title = "MAP"; break;
        default: break;
    }
    if (target) lv_obj_remove_flag(target, LV_OBJ_FLAG_HIDDEN);
    if (_title) lv_label_set_text(_title, title);
    _listDirty = true;
}

void MeshtasticScreen::show(const MeshtasticStatus &status)
{
    showPage(Page::Home);
    render(status);
    lv_screen_load(_screen);
}

void MeshtasticScreen::openThread(uint32_t peer, uint8_t channel)
{
    _threadPeer = peer;
    _threadChannel = channel;
    Conversation *c = findOrAddConversation(peer, channel);
    if (c) c->lastViewedMs = millis();
    showPage(Page::Thread);
}

void MeshtasticScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    switch (self->_page) {
        case Page::Compose: self->showPage(self->_pageBeforeCompose); break;
        case Page::Thread: self->showPage(Page::Chats); break;
        case Page::ChannelEdit: self->showPage(Page::Channels); break;
        case Page::Nodes:
        case Page::Chats:
        case Page::Channels:
        case Page::Map:
        case Page::Info: self->showPage(Page::Home); break;
        default:
            if (self->_backCallback) self->_backCallback(self->_userData);
            break;
    }
}

void MeshtasticScreen::nodesTileThunk(lv_event_t *e) { auto *s = static_cast<MeshtasticScreen *>(lv_event_get_user_data(e)); if (s) s->showPage(Page::Nodes); }
void MeshtasticScreen::chatsTileThunk(lv_event_t *e) { auto *s = static_cast<MeshtasticScreen *>(lv_event_get_user_data(e)); if (s) s->showPage(Page::Chats); }
void MeshtasticScreen::infoTileThunk(lv_event_t *e)  { auto *s = static_cast<MeshtasticScreen *>(lv_event_get_user_data(e)); if (s) s->showPage(Page::Info); }
void MeshtasticScreen::mapTileThunk(lv_event_t *e)   { auto *s = static_cast<MeshtasticScreen *>(lv_event_get_user_data(e)); if (s) s->showPage(Page::Map); }
void MeshtasticScreen::channelsTileThunk(lv_event_t *e) { auto *s = static_cast<MeshtasticScreen *>(lv_event_get_user_data(e)); if (s) s->showPage(Page::Channels); }

void MeshtasticScreen::rowThunk(lv_event_t *event)
{
    auto *ctx = static_cast<RowContext *>(lv_event_get_user_data(event));
    if (ctx && ctx->screen) ctx->screen->openThread(ctx->peer, ctx->channel);
}

void MeshtasticScreen::channelRowThunk(lv_event_t *event)
{
    auto *ctx = static_cast<RowContext *>(lv_event_get_user_data(event));
    if (ctx && ctx->screen && ctx->channel != 0) ctx->screen->openChannelEditor(ctx->channel);
}

void MeshtasticScreen::addChannelThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    const uint8_t slot = self->_service->freeChannelSlot();
    if (slot >= MeshtasticStatus::kMaxChannels) return;
    self->openChannelEditor(slot);
}

void MeshtasticScreen::openChannelEditor(uint8_t slot)
{
    _editSlot = slot;
    const MeshtasticChannel &ch = _service->status().channels[slot];
    char keyText[48] = {0};
    if (ch.used) {
        lv_textarea_set_text(_channelNameInput, ch.name);
        MeshtasticService::channelKeyText(ch, keyText, sizeof(keyText));
        lv_textarea_set_text(_channelKeyInput, keyText);
        lv_obj_remove_flag(_channelDeleteButton, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_textarea_set_text(_channelNameInput, "");
        lv_textarea_set_text(_channelKeyInput, "");
        lv_obj_add_flag(_channelDeleteButton, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(_channelError, "");
    lv_keyboard_set_textarea(_channelKeyboard, _channelNameInput);
    showPage(Page::ChannelEdit);
    lv_obj_add_state(_channelNameInput, LV_STATE_FOCUSED);
}

void MeshtasticScreen::channelFieldThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    // Whichever field was tapped last is what the keyboard types into.
    lv_keyboard_set_textarea(self->_channelKeyboard, lv_event_get_target_obj(event));
}

void MeshtasticScreen::channelSaveThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    const char *name = lv_textarea_get_text(self->_channelNameInput);
    const char *key = lv_textarea_get_text(self->_channelKeyInput);
    if (!self->_service->setChannel(self->_editSlot, name, key)) {
        lv_label_set_text(self->_channelError, "Need a name (1-11 chars) and a base64 key or 0-10");
        return;
    }
    self->showPage(Page::Channels);
}

void MeshtasticScreen::channelDeleteThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    self->_service->removeChannel(self->_editSlot);
    // Drop the conversation so Chats stops listing it.
    for (Conversation &c : self->_conversations) {
        if (c.used && c.peer == kMeshtasticBroadcast && c.channel == self->_editSlot) c.used = false;
    }
    self->showPage(Page::Channels);
}

void MeshtasticScreen::writeThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    self->_pageBeforeCompose = self->_page;
    char who[48];
    if (self->_threadPeer == kMeshtasticBroadcast) {
        snprintf(who, sizeof(who), "To channel: %s", self->_service->status().channels[self->_threadChannel].name);
    } else {
        char name[40];
        const MeshtasticNode *n = self->_service ? self->findNode(self->_service->status(), self->_threadPeer) : nullptr;
        snprintf(who, sizeof(who), "Direct to: %s", nodeLabel(n, self->_threadPeer, name, sizeof(name)));
    }
    lv_label_set_text(self->_composeTitle, who);
    lv_textarea_set_text(self->_textArea, "");
    self->showPage(Page::Compose);
    lv_obj_add_state(self->_textArea, LV_STATE_FOCUSED);
}

void MeshtasticScreen::sendThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    const char *text = lv_textarea_get_text(self->_textArea);
    if (text && text[0]) {
        if (self->_threadPeer == kMeshtasticBroadcast) self->_service->sendChannelMessage(self->_threadChannel, text);
        else self->_service->sendDirectMessage(self->_threadPeer, text);
    }
    self->showPage(Page::Thread);
}

void MeshtasticScreen::cancelThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->_page == Page::ChannelEdit) self->showPage(Page::Channels);
    else self->showPage(self->_pageBeforeCompose);
}

// ---------------------------------------------------------------- model helpers

const MeshtasticNode *MeshtasticScreen::findNode(const MeshtasticStatus &status, uint32_t num) const
{
    for (const MeshtasticNode &n : status.nodes) {
        if (n.used && n.num == num) return &n;
    }
    return nullptr;
}

const char *MeshtasticScreen::nodeLabel(const MeshtasticNode *node, uint32_t num, char *buf, size_t bufSize)
{
    if (node && node->longName[0]) return node->longName;
    if (node && node->shortName[0]) return node->shortName;
    // No NodeInfo heard yet: Meshtastic's convention is !<hex node number>.
    snprintf(buf, bufSize, "!%08lx", static_cast<unsigned long>(num));
    return buf;
}

lv_color_t MeshtasticScreen::deliveryColor(MeshtasticDelivery delivery)
{
    // Same code MUI uses on message outlines.
    switch (delivery) {
        case MeshtasticDelivery::Acked: return Theme::green();
        case MeshtasticDelivery::Failed: return Theme::danger();
        case MeshtasticDelivery::Relayed: return Theme::gold();
        case MeshtasticDelivery::Pending: return Theme::muted();
        default: return Theme::teal();
    }
}

const char *MeshtasticScreen::channelTag(const MeshtasticChannel &channel)
{
    // No lock glyph in the LVGL built-in font, so the encryption state is a
    // word: what MUI shows as an open/closed padlock.
    if (channel.pskLen == 32) return "AES256";
    if (channel.pskLen == 16) return "AES128";
    return "OPEN";
}

bool MeshtasticScreen::messageInConversation(const MeshtasticMessage &m, uint32_t peer, uint8_t channel) const
{
    const uint32_t us = _service ? _service->nodeNum() : 0;
    if (peer == kMeshtasticBroadcast) return m.toNum == kMeshtasticBroadcast && m.channel == channel;
    if (m.isOurs) return m.toNum == peer;
    return m.fromNum == peer && m.toNum == us;
}

MeshtasticScreen::Conversation *MeshtasticScreen::findOrAddConversation(uint32_t peer, uint8_t channel)
{
    if (peer != kMeshtasticBroadcast) channel = 0; // a DM thread is per node, whatever carried it
    for (Conversation &c : _conversations) {
        if (c.used && c.peer == peer && c.channel == channel) return &c;
    }
    for (Conversation &c : _conversations) {
        if (!c.used) {
            c.used = true;
            c.peer = peer;
            c.channel = channel;
            c.lastViewedMs = 0;
            return &c;
        }
    }
    return nullptr;
}

void MeshtasticScreen::syncConversations(const MeshtasticStatus &status)
{
    // Every configured channel is a conversation; a DM conversation exists
    // once any message has passed either way with that node.
    for (uint8_t i = 0; i < MeshtasticStatus::kMaxChannels; ++i) {
        if (status.channels[i].used) findOrAddConversation(kMeshtasticBroadcast, i);
    }
    const uint32_t us = _service ? _service->nodeNum() : 0;
    for (const MeshtasticMessage &m : status.messages) {
        if (!m.used || m.toNum == kMeshtasticBroadcast) continue;
        if (m.isOurs) findOrAddConversation(m.toNum, 0);
        else if (m.toNum == us) findOrAddConversation(m.fromNum, 0);
    }
}

uint32_t MeshtasticScreen::unreadFor(const Conversation &c, const MeshtasticStatus &status) const
{
    uint32_t n = 0;
    for (const MeshtasticMessage &m : status.messages) {
        if (m.used && !m.isOurs && m.receivedMs > c.lastViewedMs && messageInConversation(m, c.peer, c.channel)) ++n;
    }
    return n;
}

// ---------------------------------------------------------------- render

void MeshtasticScreen::render(const MeshtasticStatus &status)
{
    if (_screen == nullptr || lv_screen_active() != _screen) return;

    syncConversations(status);
    renderHome(status);

    const uint32_t now = millis();
    const bool due = _listDirty || (now - _lastListRebuildMs >= kListRefreshMs);
    if (!due) return;
    _lastListRebuildMs = now;
    _listDirty = false;

    switch (_page) {
        case Page::Nodes: rebuildNodes(status); break;
        case Page::Chats: rebuildChats(status); break;
        case Page::Thread: rebuildThread(status); break;
        case Page::Channels: rebuildChannels(status); break;
        case Page::Map: renderMap(status); break;
        case Page::Info: renderInfo(status); break;
        default: break;
    }
}

void MeshtasticScreen::renderHome(const MeshtasticStatus &status)
{
    if (!status.supported) {
        lv_label_set_text(_statusLine, "RADIO NOT SUPPORTED");
        lv_obj_set_style_text_color(_statusLine, Theme::gold(), 0);
    } else if (!status.radioEnabled) {
        lv_label_set_text(_statusLine, "OFF - enable MESHTASTIC in Settings");
        lv_obj_set_style_text_color(_statusLine, Theme::muted(), 0);
    } else if (status.radioReady) {
        lv_label_set_text_fmt(_statusLine, "%s  %s", status.advertisingEnabled ? "ONLINE" : "LISTENING",
                              status.longName);
        lv_obj_set_style_text_color(_statusLine, Theme::green(), 0);
    } else {
        lv_label_set_text_fmt(_statusLine, "RADIO ERROR %d", status.radioError);
        lv_obj_set_style_text_color(_statusLine, Theme::danger(), 0);
    }

    lv_label_set_text_fmt(_nodesValue, "%u", status.nodeCount);
    unsigned channels = 0;
    for (const MeshtasticChannel &ch : status.channels) if (ch.used) ++channels;
    lv_label_set_text_fmt(_channelsValue, "%u", channels);

    uint32_t unread = 0;
    for (const Conversation &c : _conversations) if (c.used) unread += unreadFor(c, status);
    if (unread > 0) {
        lv_label_set_text_fmt(_chatsValue, "%lu new", static_cast<unsigned long>(unread));
        lv_obj_set_style_text_color(_chatsValue, Theme::gold(), 0);
    } else {
        lv_label_set_text_fmt(_chatsValue, "%u", status.messageCount);
        lv_obj_set_style_text_color(_chatsValue, Theme::white(), 0);
    }
}

void MeshtasticScreen::rebuildNodes(const MeshtasticStatus &status)
{
    lv_obj_clean(_nodesList);
    if (status.nodeCount == 0) {
        makeLabel(_nodesList, "No nodes heard yet.", 0, 0, 380, &lv_font_montserrat_16, Theme::muted());
        return;
    }

    const uint32_t now = millis();
    size_t ctx = 0;
    for (const MeshtasticNode &node : status.nodes) {
        if (!node.used || ctx >= MeshtasticStatus::kMaxNodes) continue;

        RowContext &rc = _rowContexts[ctx++];
        rc.screen = this;
        rc.peer = node.num;

        lv_obj_t *row = lv_button_create(_nodesList);
        lv_obj_set_size(row, 386, 60);
        lv_obj_set_style_bg_color(row, Theme::background(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, Theme::teal(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_add_event_cb(row, rowThunk, LV_EVENT_CLICKED, &rc);

        // Short-name badge, the way MUI identifies nodes at a glance.
        lv_obj_t *badge = lv_obj_create(row);
        lv_obj_set_pos(badge, 8, 10);
        lv_obj_set_size(badge, 62, 40);
        lv_obj_set_style_bg_color(badge, Theme::teal(), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 6, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);
        makeLabel(badge, node.shortName[0] ? node.shortName : "?", 0, 10, 62,
                  &lv_font_montserrat_16, Theme::background(), LV_TEXT_ALIGN_CENTER);

        char nameBuf[40];
        makeLabel(row, nodeLabel(&node, node.num, nameBuf, sizeof(nameBuf)), 80, 8, 220,
                  &lv_font_montserrat_18, Theme::white());

        char age[12];
        ageText(now - node.lastSeenMs, age, sizeof(age));
        char detail[64];
        const unsigned hops = (node.hopStart >= node.hopLimit) ? (node.hopStart - node.hopLimit) : 0;
        if (node.hasTelemetry) {
            snprintf(detail, sizeof(detail), "%u hop%s  %s  %u%%", hops, hops == 1 ? "" : "s", age,
                     static_cast<unsigned>(node.batteryPercent));
        } else {
            snprintf(detail, sizeof(detail), "%u hop%s  %s", hops, hops == 1 ? "" : "s", age);
        }
        makeLabel(row, detail, 80, 34, 220, &lv_font_montserrat_14, Theme::muted());

        char rssi[16];
        snprintf(rssi, sizeof(rssi), "%.0f", node.rssi);
        makeLabel(row, rssi, 300, 8, 78, &lv_font_montserrat_16, Theme::teal(), LV_TEXT_ALIGN_RIGHT);
        makeLabel(row, node.hasPosition ? "GPS" : "", 300, 34, 78, &lv_font_montserrat_14,
                  Theme::green(), LV_TEXT_ALIGN_RIGHT);
    }
}

void MeshtasticScreen::rebuildChats(const MeshtasticStatus &status)
{
    lv_obj_clean(_chatsList);
    size_t ctx = MeshtasticStatus::kMaxNodes; // chats rows use the tail of the context pool
    for (const Conversation &c : _conversations) {
        if (!c.used || ctx >= sizeof(_rowContexts) / sizeof(_rowContexts[0])) continue;

        RowContext &rc = _rowContexts[ctx++];
        rc.screen = this;
        rc.peer = c.peer;
        rc.channel = c.channel;

        const uint32_t unread = unreadFor(c, status);
        const bool isChannel = (c.peer == kMeshtasticBroadcast);

        lv_obj_t *row = lv_button_create(_chatsList);
        lv_obj_set_size(row, 386, 64);
        lv_obj_set_style_bg_color(row, Theme::background(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        // Unread conversations highlight, as in MUI.
        lv_obj_set_style_border_color(row, unread ? Theme::gold() : (isChannel ? Theme::green() : Theme::teal()), 0);
        lv_obj_set_style_border_width(row, unread ? 2 : 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_add_event_cb(row, rowThunk, LV_EVENT_CLICKED, &rc);

        char nameBuf[40];
        char title[56];
        if (isChannel) {
            snprintf(title, sizeof(title), "# %s", status.channels[c.channel].name);
        } else {
            snprintf(title, sizeof(title), "@ %s", nodeLabel(findNode(status, c.peer), c.peer, nameBuf, sizeof(nameBuf)));
        }
        makeLabel(row, title, 12, 8, 290, &lv_font_montserrat_18, Theme::white());

        // Most recent line as the preview.
        const MeshtasticMessage *last = nullptr;
        for (const MeshtasticMessage &m : status.messages) {
            if (m.used && messageInConversation(m, c.peer, c.channel) && (!last || m.receivedMs >= last->receivedMs)) last = &m;
        }
        char preview[48];
        if (last) {
            snprintf(preview, sizeof(preview), "%s%.36s", last->isOurs ? "You: " : "", last->text);
        } else {
            snprintf(preview, sizeof(preview), "%s", isChannel ? channelTag(status.channels[c.channel]) : "No messages yet");
        }
        makeLabel(row, preview, 12, 36, 300, &lv_font_montserrat_14, Theme::muted());

        if (unread) {
            char badge[8];
            snprintf(badge, sizeof(badge), "%lu", static_cast<unsigned long>(unread));
            makeLabel(row, badge, 320, 18, 58, &lv_font_montserrat_20, Theme::gold(), LV_TEXT_ALIGN_RIGHT);
        }
    }
}

void MeshtasticScreen::rebuildThread(const MeshtasticStatus &status)
{
    lv_obj_clean(_threadList);

    char nameBuf[40];
    if (_threadPeer == kMeshtasticBroadcast) {
        char t[24];
        snprintf(t, sizeof(t), "# %s", status.channels[_threadChannel].name);
        lv_label_set_text(_title, t);
    } else {
        char t[48];
        snprintf(t, sizeof(t), "@ %s", nodeLabel(findNode(status, _threadPeer), _threadPeer, nameBuf, sizeof(nameBuf)));
        lv_label_set_text(_title, t);
    }

    // Oldest first, so the newest lands at the bottom where the eye expects it.
    // messages[] is a ring, so walk it from the oldest slot.
    bool any = false;
    const uint8_t count = status.messageCount;
    uint8_t start = 0;
    if (count == MeshtasticStatus::kMaxMessages) {
        // Full ring: the oldest is the one after the newest. Find newest by receivedMs.
        uint32_t newest = 0;
        for (uint8_t i = 0; i < count; ++i) {
            if (status.messages[i].used && status.messages[i].receivedMs >= status.messages[newest].receivedMs) newest = i;
        }
        start = static_cast<uint8_t>((newest + 1) % count);
    }
    for (uint8_t k = 0; k < count; ++k) {
        const MeshtasticMessage &m = status.messages[(start + k) % MeshtasticStatus::kMaxMessages];
        if (!m.used || !messageInConversation(m, _threadPeer, _threadChannel)) continue;
        any = true;

        lv_obj_t *bubble = lv_obj_create(_threadList);
        lv_obj_set_width(bubble, 386);
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(bubble, Theme::background(), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, m.isOurs ? deliveryColor(m.delivery) : Theme::teal(), 0);
        lv_obj_set_style_border_width(bubble, 2, 0);
        lv_obj_set_style_radius(bubble, 8, 0);
        lv_obj_set_style_pad_all(bubble, 8, 0);
        lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

        char header[64];
        char age[12];
        ageText(millis() - m.receivedMs, age, sizeof(age));
        if (m.isOurs) {
            const char *state = "";
            switch (m.delivery) {
                case MeshtasticDelivery::Acked: state = "delivered"; break;
                case MeshtasticDelivery::Failed: state = "FAILED"; break;
                case MeshtasticDelivery::Relayed: state = "relayed"; break;
                case MeshtasticDelivery::Pending: state = "sending"; break;
                default: break;
            }
            snprintf(header, sizeof(header), "You  %s  %s", age, state);
        } else {
            snprintf(header, sizeof(header), "%s  %s  %.0f dBm",
                     nodeLabel(findNode(status, m.fromNum), m.fromNum, nameBuf, sizeof(nameBuf)), age, m.rssi);
        }
        lv_obj_t *h = makeLabel(bubble, header, 0, 0, 366, &lv_font_montserrat_14,
                                m.isOurs ? deliveryColor(m.delivery) : Theme::teal());
        lv_obj_t *body = makeLabel(bubble, m.text, 0, 20, 366, &lv_font_montserrat_18, Theme::white());
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        (void)h;
    }

    if (!any) {
        makeLabel(_threadList, "No messages yet. Tap WRITE.", 0, 0, 380, &lv_font_montserrat_16, Theme::muted());
    } else {
        lv_obj_scroll_to_y(_threadList, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void MeshtasticScreen::rebuildChannels(const MeshtasticStatus &status)
{
    lv_obj_clean(_channelsList);
    for (uint8_t i = 0; i < MeshtasticStatus::kMaxChannels; ++i) {
        const MeshtasticChannel &ch = status.channels[i];
        if (!ch.used) continue;

        RowContext &rc = _channelContexts[i];
        rc.screen = this;
        rc.peer = kMeshtasticBroadcast;
        rc.channel = i;

        const bool editable = (i != 0);
        lv_obj_t *row = lv_button_create(_channelsList);
        lv_obj_set_size(row, 386, 60);
        lv_obj_set_style_bg_color(row, Theme::background(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, editable ? Theme::gold() : Theme::muted(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_add_event_cb(row, channelRowThunk, LV_EVENT_CLICKED, &rc);

        char title[24];
        snprintf(title, sizeof(title), "%u  %s", static_cast<unsigned>(i), ch.name);
        makeLabel(row, title, 12, 8, 240, &lv_font_montserrat_18, Theme::white());
        makeLabel(row, editable ? "tap to edit" : "primary - fixed", 12, 34, 240, &lv_font_montserrat_14,
                  Theme::muted());

        const bool open = (ch.pskLen == 0);
        makeLabel(row, channelTag(ch), 260, 8, 116, &lv_font_montserrat_16, open ? Theme::danger() : Theme::green(),
                  LV_TEXT_ALIGN_RIGHT);
        char hash[12];
        snprintf(hash, sizeof(hash), "hash %02X", ch.hash);
        makeLabel(row, hash, 260, 34, 116, &lv_font_montserrat_14, Theme::muted(), LV_TEXT_ALIGN_RIGHT);
    }

    const bool full = _service && _service->freeChannelSlot() >= MeshtasticStatus::kMaxChannels;
    if (full) lv_obj_add_flag(_addChannelButton, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(_addChannelButton, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------- map

void MeshtasticScreen::centerMap(const MeshtasticStatus &status)
{
    // CENTER alternates between the watch and the mesh: first press (and
    // the first time the page opens) centers on our own fix, the next on
    // the nodes' centroid, and so on. Falls back to whichever exists.
    double lat = 0.0, lon = 0.0;
    const bool haveOwn = _service && _service->ownPosition(lat, lon);
    double sumLat = 0.0, sumLon = 0.0;
    unsigned n = 0;
    for (const MeshtasticNode &node : status.nodes) {
        if (!node.used || !node.hasPosition) continue;
        sumLat += node.latitude;
        sumLon += node.longitude;
        ++n;
    }
    const bool wantNodes = _mapCenterOnNodes && n > 0;
    if (wantNodes) {
        _map.setCenter(sumLat / n, sumLon / n);
    } else if (haveOwn) {
        _map.setCenter(lat, lon);
    } else if (n > 0) {
        _map.setCenter(sumLat / n, sumLon / n);
    } else {
        return; // nothing to center on; MapView shows its 'no position' notice
    }
    _mapCentered = true;
}

void MeshtasticScreen::mapCenterThunk(void *userData)
{
    auto *self = static_cast<MeshtasticScreen *>(userData);
    if (!self || !self->_service) return;
    self->_mapCenterOnNodes = !self->_mapCenterOnNodes;
    self->centerMap(self->_service->status());
}

void MeshtasticScreen::renderMap(const MeshtasticStatus &status)
{
    // Rebuild the marker set, but only redraw (tile decode from SD) when
    // it actually changed - render() runs this every 2 s while the page
    // is showing.
    uint32_t signature = 0;
    _map.clearMarkers();
    double lat = 0.0, lon = 0.0;
    if (_service && _service->ownPosition(lat, lon)) {
        _map.addMarker(lat, lon, "ME", Theme::gold(), true);
        signature ^= static_cast<uint32_t>(lat * 1e5) * 31u + static_cast<uint32_t>(lon * 1e5);
    }
    for (const MeshtasticNode &node : status.nodes) {
        if (!node.used || !node.hasPosition) continue;
        char label[8];
        if (node.shortName[0]) snprintf(label, sizeof(label), "%s", node.shortName);
        else snprintf(label, sizeof(label), "%04lx", static_cast<unsigned long>(node.num & 0xFFFF));
        _map.addMarker(node.latitude, node.longitude, label, Theme::teal(), false);
        signature ^= (static_cast<uint32_t>(node.latitude * 1e5) * 31u + static_cast<uint32_t>(node.longitude * 1e5)) ^ node.num;
    }
    if (!_mapCentered) centerMap(status);
    if (signature != _mapMarkerSignature || !_mapCentered) {
        _mapMarkerSignature = signature;
        _map.render();
    }
}

void MeshtasticScreen::renderInfo(const MeshtasticStatus &status)
{
    lv_label_set_text_fmt(
        _infoText,
        "Node  !%08lx\nName  %s\n\nPackets heard  %lu\nDecoded  %lu\nAdverts sent  %lu\n"
        "Last RSSI  %.0f dBm\nLast SNR  %.1f dB\nLast packet  %u bytes\n\n"
        "Nodes  %u / %u\nMessages  %u / %u",
        static_cast<unsigned long>(_service ? _service->nodeNum() : 0), status.longName,
        static_cast<unsigned long>(status.packetCount), static_cast<unsigned long>(status.decodedCount),
        static_cast<unsigned long>(status.advertCount), status.lastRssi, status.lastSnr,
        static_cast<unsigned>(status.lastPacketBytes),
        status.nodeCount, MeshtasticStatus::kMaxNodes, status.messageCount, MeshtasticStatus::kMaxMessages);
}
