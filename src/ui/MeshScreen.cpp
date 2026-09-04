// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "MeshScreen.h"
#include "QuickPhrases.h"
#include "Theme.h"

namespace {
// Phrase strings are static literals, so stashing the pointer on the button
// itself is safe for the life of the screen and avoids another context pool.
lv_obj_t *makePhraseButton(lv_obj_t *parent, const char *phrase, lv_event_cb_t cb,
                           void *userData)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 178, 40);
    lv_obj_set_style_bg_color(button, Theme::background(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, Theme::gold(), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_user_data(button, const_cast<char *>(phrase));
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, phrase);
    lv_obj_set_style_text_color(label, Theme::white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return button;
}
} // namespace

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

void MeshScreen::create(MeshService *service, BackCallback backCallback, void *userData)
{
    _service = service;
    _backCallback = backCallback;
    _userData = userData;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);

    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_size(back, 86, 42);
    lv_obj_set_pos(back, 12, 12);
    lv_obj_set_style_bg_color(back, Theme::background(), 0);
    lv_obj_set_style_border_color(back, Theme::gold(), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_radius(back, 8, 0);
    lv_obj_add_event_cb(back, backEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *backLabel = lv_label_create(back);
    lv_label_set_text(backLabel, "BACK");
    lv_obj_set_style_text_color(backLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(backLabel);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "MESHCORE");
    lv_obj_set_width(title, 190);
    lv_obj_set_pos(title, 110, 14);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::gold(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    lv_obj_t *compose = lv_button_create(_screen);
    lv_obj_set_size(compose, 98, 42);
    lv_obj_set_pos(compose, 300, 12);
    lv_obj_set_style_bg_color(compose, Theme::background(), 0);
    lv_obj_set_style_border_color(compose, Theme::green(), 0);
    lv_obj_set_style_border_width(compose, 2, 0);
    lv_obj_set_style_radius(compose, 8, 0);
    lv_obj_add_event_cb(compose, composeEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *composeLabel = lv_label_create(compose);
    lv_label_set_text(composeLabel, "CHAT");
    lv_obj_set_style_text_color(composeLabel, Theme::green(), 0);
    lv_obj_set_style_text_font(composeLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(composeLabel);

    lv_obj_t *preset = lv_label_create(_screen);
    lv_label_set_text(preset, "MESHCORE US/CANADA  910.525 MHz");
    lv_obj_set_width(preset, 390);
    lv_obj_set_pos(preset, 10, 66);
    lv_obj_set_style_text_align(preset, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(preset, Theme::teal(), 0);
    lv_obj_set_style_text_font(preset, &lv_font_montserrat_18, 0);

    _status = lv_label_create(_screen);
    lv_obj_set_width(_status, 390);
    lv_obj_set_pos(_status, 10, 96);
    lv_obj_set_style_text_align(_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_20, 0);

    _summary = lv_label_create(_screen);
    lv_obj_set_width(_summary, 390);
    lv_obj_set_pos(_summary, 10, 126);
    lv_obj_set_style_text_align(_summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_summary, Theme::gold(), 0);
    lv_obj_set_style_text_font(_summary, &lv_font_montserrat_20, 0);

    lv_obj_t *nodesTitle = lv_label_create(_screen);
    lv_label_set_text(nodesTitle, "HEARD NODES");
    lv_obj_set_pos(nodesTitle, 18, 158);
    lv_obj_set_style_text_color(nodesTitle, Theme::green(), 0);
    lv_obj_set_style_text_font(nodesTitle, &lv_font_montserrat_20, 0);

    // Wrapped in its own scrollable container so the label can grow to fit
    // however many nodes have been heard - previously this was a fixed-
    // height label that silently clipped anything past its first ~5 lines.
    lv_obj_t *nodesBox = lv_obj_create(_screen);
    lv_obj_set_size(nodesBox, 374, 105);
    lv_obj_set_pos(nodesBox, 18, 182);
    lv_obj_set_style_bg_opa(nodesBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(nodesBox, 0, 0);
    lv_obj_set_style_pad_all(nodesBox, 0, 0);
    lv_obj_set_scroll_dir(nodesBox, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(nodesBox, LV_SCROLLBAR_MODE_AUTO);

    _nodes = lv_label_create(nodesBox);
    lv_obj_set_width(_nodes, 358);
    lv_obj_set_pos(_nodes, 0, 0);
    lv_obj_set_style_text_color(_nodes, Theme::green(), 0);
    lv_obj_set_style_text_font(_nodes, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_line_space(_nodes, 3, 0);
    lv_label_set_long_mode(_nodes, LV_LABEL_LONG_WRAP);

    lv_obj_t *messagesTitle = lv_label_create(_screen);
    lv_label_set_text(messagesTitle, "PUBLIC CHAT");
    lv_obj_set_pos(messagesTitle, 18, 296);
    lv_obj_set_style_text_color(messagesTitle, Theme::teal(), 0);
    lv_obj_set_style_text_font(messagesTitle, &lv_font_montserrat_20, 0);

    // Same treatment as nodesBox above - scrollable so older chat history
    // beyond the visible area is reachable instead of silently hidden.
    lv_obj_t *messagesBox = lv_obj_create(_screen);
    lv_obj_set_size(messagesBox, 374, 122);
    lv_obj_set_pos(messagesBox, 18, 320);
    lv_obj_set_style_bg_opa(messagesBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(messagesBox, 0, 0);
    lv_obj_set_style_pad_all(messagesBox, 0, 0);
    lv_obj_set_scroll_dir(messagesBox, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(messagesBox, LV_SCROLLBAR_MODE_AUTO);

    _messages = lv_label_create(messagesBox);
    lv_obj_set_width(_messages, 358);
    lv_obj_set_pos(_messages, 0, 0);
    lv_obj_set_style_text_color(_messages, Theme::white(), 0);
    lv_obj_set_style_text_font(_messages, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_line_space(_messages, 4, 0);
    lv_label_set_long_mode(_messages, LV_LABEL_LONG_WRAP);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  T-WATCH ULTRA");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 455);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_20, 0);

    // Full-screen composer, hidden until CHAT is tapped.
    _composer = lv_obj_create(_screen);
    lv_obj_set_size(_composer, 410, 502);
    lv_obj_set_pos(_composer, 0, 0);
    lv_obj_set_style_bg_color(_composer, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_composer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_composer, 0, 0);
    lv_obj_set_style_pad_all(_composer, 10, 0);
    lv_obj_add_flag(_composer, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *composeTitle = lv_label_create(_composer);
    lv_label_set_text(composeTitle, "PUBLIC MESH CHAT");
    lv_obj_set_width(composeTitle, 250);
    lv_obj_set_pos(composeTitle, 80, 8);
    lv_obj_set_style_text_align(composeTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(composeTitle, Theme::gold(), 0);
    lv_obj_set_style_text_font(composeTitle, &lv_font_montserrat_20, 0);

    _textArea = lv_textarea_create(_composer);
    lv_obj_set_size(_textArea, 380, 70);
    lv_obj_set_pos(_textArea, 5, 46);
    lv_textarea_set_one_line(_textArea, false);
    lv_textarea_set_max_length(_textArea, 110);
    lv_textarea_set_placeholder_text(_textArea, "Message public channel...");
    lv_obj_set_style_text_font(_textArea, &lv_font_montserrat_20, 0);

    lv_obj_t *cancel = lv_button_create(_composer);
    lv_obj_set_size(cancel, 105, 42);
    lv_obj_set_pos(cancel, 25, 126);
    lv_obj_set_style_bg_color(cancel, Theme::background(), 0);
    lv_obj_set_style_border_color(cancel, Theme::gold(), 0);
    lv_obj_set_style_border_width(cancel, 2, 0);
    lv_obj_add_event_cb(cancel, cancelEventThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *cancelLabel = lv_label_create(cancel);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_set_style_text_color(cancelLabel, Theme::gold(), 0);
    lv_obj_center(cancelLabel);

    lv_obj_t *send = lv_button_create(_composer);
    lv_obj_set_size(send, 105, 42);
    lv_obj_set_pos(send, 270, 126);
    lv_obj_set_style_bg_color(send, Theme::green(), 0);
    lv_obj_add_event_cb(send, sendEventThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *sendLabel = lv_label_create(send);
    lv_label_set_text(sendLabel, "SEND");
    lv_obj_set_style_text_color(sendLabel, Theme::background(), 0);
    lv_obj_center(sendLabel);

    lv_obj_t *clear = lv_button_create(_composer);
    lv_obj_set_size(clear, 105, 42);
    lv_obj_set_pos(clear, 147, 126);
    lv_obj_set_style_bg_color(clear, Theme::background(), 0);
    lv_obj_set_style_border_color(clear, Theme::gold(), 0);
    lv_obj_set_style_border_width(clear, 2, 0);
    lv_obj_add_event_cb(clear, clearEventThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *clearLabel = lv_label_create(clear);
    lv_label_set_text(clearLabel, "CLEAR");
    lv_obj_set_style_text_color(clearLabel, Theme::gold(), 0);
    lv_obj_center(clearLabel);

    // Quick phrases instead of a keyboard: two columns of buttons in a
    // wrapping flex row, filling exactly the rectangle the keyboard held.
    _phraseList = lv_obj_create(_composer);
    lv_obj_set_size(_phraseList, 390, 305);
    lv_obj_set_pos(_phraseList, 0, 177);
    lv_obj_set_style_bg_opa(_phraseList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_phraseList, 0, 0);
    lv_obj_set_style_pad_all(_phraseList, 8, 0);
    lv_obj_set_style_pad_row(_phraseList, 6, 0);
    lv_obj_set_style_pad_column(_phraseList, 6, 0);
    lv_obj_set_flex_flow(_phraseList, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(_phraseList, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_phraseList, LV_SCROLLBAR_MODE_AUTO);
    for (size_t i = 0; i < QuickPhrases::kCount; ++i) {
        makePhraseButton(_phraseList, QuickPhrases::kPhrases[i], phraseEventThunk, this);
    }
}

void MeshScreen::show(const MeshStatus &status)
{
    render(status);
    lv_screen_load(_screen);
}

const char *MeshScreen::nodeTypeName(uint8_t type)
{
    switch (type) {
    case 1: return "CHAT";
    case 2: return "REPEATER";
    case 3: return "ROOM";
    case 4: return "SENSOR";
    default: return "NODE";
    }
}

void MeshScreen::render(const MeshStatus &status)
{
    if (_screen == nullptr) return;

    if (!status.supported) {
        lv_label_set_text(_status, "RADIO NOT SUPPORTED");
        lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    } else if (!status.radioEnabled) {
        lv_label_set_text(_status, "MESHCORE OFF - enable in Settings");
        lv_obj_set_style_text_color(_status, Theme::muted(), 0);
    } else if (status.radioReady) {
        lv_label_set_text_fmt(_status, "ONLINE  %s", status.nodeName);
        lv_obj_set_style_text_color(_status, Theme::green(), 0);
    } else {
        lv_label_set_text_fmt(_status, "RADIO ERROR %d", status.radioError);
        lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    }

    lv_label_set_text_fmt(_summary, "%u NODES  |  %lu MSG  |  %lu FRAMES",
        status.nodeCount,
        static_cast<unsigned long>(status.messageCount),
        static_cast<unsigned long>(status.packetCount));

    if (status.nodeCount == 0) {
        lv_label_set_text(_nodes, "No node adverts heard yet.");
    } else {
        char output[520];
        output[0] = '\0';
        size_t used = 0;
        const uint32_t now = millis();
        for (uint8_t i = 0; i < MeshStatus::kMaxNodes; ++i) {
            const MeshNode &node = status.nodes[i];
            if (!node.used) continue;
            const uint32_t ageSec = (now - node.lastSeenMs) / 1000;
            char line[110];
            snprintf(line, sizeof(line), "%s [%s] %.0fdBm %lus\n",
                node.name, nodeTypeName(node.type), node.rssi,
                static_cast<unsigned long>(ageSec));
            const size_t n = strlen(line);
            if (used + n + 1 >= sizeof(output)) break;
            memcpy(output + used, line, n + 1);
            used += n;
        }
        lv_label_set_text(_nodes, output);
    }

    if (status.messageCount == 0) {
        lv_label_set_text(_messages, "No public messages heard yet.\nTap CHAT to transmit.");
    } else {
        char output[640];
        output[0] = '\0';
        size_t used = 0;
        // Ring buffer is written sequentially; display every used slot for now.
        for (uint8_t i = 0; i < MeshStatus::kMaxMessages; ++i) {
            const MeshMessage &msg = status.messages[i];
            if (!msg.used) continue;
            char line[145];
            snprintf(line, sizeof(line), "%s%s\n",
                msg.rssi != 0.0f ? "RX  " : "TX  ", msg.text);
            const size_t n = strlen(line);
            if (used + n + 1 >= sizeof(output)) break;
            memcpy(output + used, line, n + 1);
            used += n;
        }
        lv_label_set_text(_messages, output);
    }
}

void MeshScreen::showComposer()
{
    lv_textarea_set_text(_textArea, "");
    lv_obj_clear_flag(_composer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_composer);
    lv_obj_add_state(_textArea, LV_STATE_FOCUSED);
}

void MeshScreen::hideComposer()
{
    lv_obj_add_flag(_composer, LV_OBJ_FLAG_HIDDEN);
}

void MeshScreen::backEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) self->_backCallback(self->_userData);
}

void MeshScreen::composeEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showComposer();
}

void MeshScreen::sendEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_service == nullptr) return;
    const char *text = lv_textarea_get_text(self->_textArea);
    if (text != nullptr && text[0] != '\0') self->_service->sendPublicMessage(text);
    self->hideComposer();
    self->render(self->_service->status());
}

void MeshScreen::phraseEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;
    lv_obj_t *button = lv_event_get_target_obj(event);
    if (button == nullptr) return;
    const char *phrase = static_cast<const char *>(lv_obj_get_user_data(button));
    if (phrase == nullptr) return;
    const char *current = lv_textarea_get_text(self->_textArea);
    if (current != nullptr && current[0] != '\0') lv_textarea_add_text(self->_textArea, " ");
    lv_textarea_add_text(self->_textArea, phrase);
}

void MeshScreen::clearEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) lv_textarea_set_text(self->_textArea, "");
}

void MeshScreen::cancelEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideComposer();
}
