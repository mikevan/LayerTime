#include "MeshtasticScreen.h"
#include "Theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

void MeshtasticScreen::create(MeshtasticService *service, BackCallback backCallback, void *userData)
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
    lv_obj_set_style_border_color(back, Theme::green(), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_radius(back, 8, 0);
    lv_obj_add_event_cb(back, backEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *backLabel = lv_label_create(back);
    lv_label_set_text(backLabel, "BACK");
    lv_obj_set_style_text_color(backLabel, Theme::green(), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(backLabel);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "MTASTIC");
    lv_obj_set_width(title, 190);
    lv_obj_set_pos(title, 110, 14);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::green(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    lv_obj_t *compose = lv_button_create(_screen);
    lv_obj_set_size(compose, 98, 42);
    lv_obj_set_pos(compose, 300, 12);
    lv_obj_set_style_bg_color(compose, Theme::background(), 0);
    lv_obj_set_style_border_color(compose, Theme::teal(), 0);
    lv_obj_set_style_border_width(compose, 2, 0);
    lv_obj_set_style_radius(compose, 8, 0);
    lv_obj_add_event_cb(compose, composeEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *composeLabel = lv_label_create(compose);
    lv_label_set_text(composeLabel, "CHAT");
    lv_obj_set_style_text_color(composeLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(composeLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(composeLabel);

    lv_obj_t *preset = lv_label_create(_screen);
    lv_label_set_text(preset, "MESHTASTIC US/LONGFAST  906.875 MHz");
    lv_obj_set_width(preset, 390);
    lv_obj_set_pos(preset, 10, 66);
    lv_obj_set_style_text_align(preset, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(preset, Theme::teal(), 0);
    lv_obj_set_style_text_font(preset, &lv_font_montserrat_16, 0);

    _status = lv_label_create(_screen);
    lv_obj_set_width(_status, 390);
    lv_obj_set_pos(_status, 10, 96);
    lv_obj_set_style_text_align(_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_18, 0);

    _summary = lv_label_create(_screen);
    lv_obj_set_width(_summary, 390);
    lv_obj_set_pos(_summary, 10, 126);
    lv_obj_set_style_text_align(_summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_summary, Theme::gold(), 0);
    lv_obj_set_style_text_font(_summary, &lv_font_montserrat_16, 0);

    lv_obj_t *nodesTitle = lv_label_create(_screen);
    lv_label_set_text(nodesTitle, "HEARD NODES");
    lv_obj_set_pos(nodesTitle, 18, 158);
    lv_obj_set_style_text_color(nodesTitle, Theme::green(), 0);
    lv_obj_set_style_text_font(nodesTitle, &lv_font_montserrat_16, 0);

    _nodes = lv_label_create(_screen);
    lv_obj_set_width(_nodes, 374);
    lv_obj_set_height(_nodes, 180);
    lv_obj_set_pos(_nodes, 18, 182);
    lv_obj_set_style_text_color(_nodes, Theme::green(), 0);
    lv_obj_set_style_text_font(_nodes, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(_nodes, 3, 0);
    lv_label_set_long_mode(_nodes, LV_LABEL_LONG_CLIP);

    lv_obj_t *messagesTitle = lv_label_create(_screen);
    lv_label_set_text(messagesTitle, "PUBLIC MESSAGES");
    lv_obj_set_pos(messagesTitle, 18, 370);
    lv_obj_set_style_text_color(messagesTitle, Theme::teal(), 0);
    lv_obj_set_style_text_font(messagesTitle, &lv_font_montserrat_16, 0);

    _messages = lv_label_create(_screen);
    lv_obj_set_width(_messages, 374);
    lv_obj_set_height(_messages, 72);
    lv_obj_set_pos(_messages, 18, 394);
    lv_obj_set_style_text_color(_messages, Theme::white(), 0);
    lv_obj_set_style_text_font(_messages, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(_messages, 4, 0);
    lv_label_set_long_mode(_messages, LV_LABEL_LONG_CLIP);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  T-WATCH ULTRA");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 470);
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
    lv_label_set_text(composeTitle, "PUBLIC MESHTASTIC CHAT");
    lv_obj_set_width(composeTitle, 280);
    lv_obj_set_pos(composeTitle, 60, 8);
    lv_obj_set_style_text_align(composeTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(composeTitle, Theme::green(), 0);
    lv_obj_set_style_text_font(composeTitle, &lv_font_montserrat_20, 0);

    _textArea = lv_textarea_create(_composer);
    lv_obj_set_size(_textArea, 380, 70);
    lv_obj_set_pos(_textArea, 5, 46);
    lv_textarea_set_one_line(_textArea, false);
    lv_textarea_set_max_length(_textArea, 200);
    lv_textarea_set_placeholder_text(_textArea, "Message public channel...");
    lv_obj_set_style_text_font(_textArea, &lv_font_montserrat_16, 0);

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

    _keyboard = lv_keyboard_create(_composer);
    lv_obj_set_size(_keyboard, 390, 305);
    lv_obj_set_pos(_keyboard, 0, 177);
    lv_keyboard_set_textarea(_keyboard, _textArea);
}

void MeshtasticScreen::show(const MeshtasticStatus &status)
{
    render(status);
    lv_screen_load(_screen);
}

void MeshtasticScreen::render(const MeshtasticStatus &status)
{
    if (_screen == nullptr) return;

    if (!status.supported) {
        lv_label_set_text(_status, "RADIO NOT SUPPORTED");
        lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    } else if (!status.radioEnabled) {
        lv_label_set_text(_status, "MTASTIC OFF - enable in Settings");
        lv_obj_set_style_text_color(_status, Theme::muted(), 0);
    } else if (status.radioReady) {
        lv_label_set_text_fmt(_status, "%s  %s", status.advertisingEnabled ? "ONLINE" : "LISTENING", status.longName);
        lv_obj_set_style_text_color(_status, Theme::green(), 0);
    } else {
        lv_label_set_text_fmt(_status, "RADIO ERROR %d", status.radioError);
        lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    }

    lv_label_set_text_fmt(_summary, "%u NODES  |  %lu MSG  |  %lu/%lu PKT",
        status.nodeCount,
        static_cast<unsigned long>(status.messageCount),
        static_cast<unsigned long>(status.decodedCount),
        static_cast<unsigned long>(status.packetCount));

    if (status.nodeCount == 0) {
        lv_label_set_text(_nodes, "No node adverts heard yet.");
    } else {
        char output[900];
        output[0] = '\0';
        size_t used = 0;
        const uint32_t now = millis();
        for (uint8_t i = 0; i < MeshtasticStatus::kMaxNodes; ++i) {
            const MeshtasticNode &node = status.nodes[i];
            if (!node.used) continue;
            const uint32_t ageSec = (now - node.lastSeenMs) / 1000;
            const char *name = node.longName[0] != '\0'
                ? node.longName
                : (node.shortName[0] != '\0' ? node.shortName : "UNKNOWN");
            char extra[24] = "";
            if (node.hasTelemetry) {
                snprintf(extra, sizeof(extra), " BAT%u%%", static_cast<unsigned>(node.batteryPercent));
            } else if (node.hasPosition) {
                snprintf(extra, sizeof(extra), " GPS");
            }
            char line[140];
            snprintf(line, sizeof(line), "%s H%u/%u %.0fdBm %lus%s\n",
                name, node.hopLimit, node.hopStart, node.rssi,
                static_cast<unsigned long>(ageSec), extra);
            const size_t n = strlen(line);
            if (used + n + 1 >= sizeof(output)) break;
            memcpy(output + used, line, n + 1);
            used += n;
        }
        lv_label_set_text(_nodes, output);
    }

    if (status.messageCount == 0) {
        lv_label_set_text(_messages, "No public messages yet.\nTap CHAT to transmit.");
    } else {
        char output[512];
        output[0] = '\0';
        size_t used = 0;
        for (uint8_t i = 0; i < MeshtasticStatus::kMaxMessages; ++i) {
            const MeshtasticMessage &msg = status.messages[i];
            if (!msg.used) continue;
            char line[180];
            snprintf(line, sizeof(line), "%s[h%u] %s\n", msg.rssi != 0.0f ? "RX " : "TX ", msg.hopLimit, msg.text);
            const size_t n = strlen(line);
            if (used + n + 1 >= sizeof(output)) break;
            memcpy(output + used, line, n + 1);
            used += n;
        }
        lv_label_set_text(_messages, output);
    }
}

void MeshtasticScreen::backEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) self->_backCallback(self->_userData);
}

void MeshtasticScreen::showComposer()
{
    lv_textarea_set_text(_textArea, "");
    lv_obj_clear_flag(_composer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_composer);
    lv_obj_add_state(_textArea, LV_STATE_FOCUSED);
}

void MeshtasticScreen::hideComposer()
{
    lv_obj_add_flag(_composer, LV_OBJ_FLAG_HIDDEN);
}

void MeshtasticScreen::composeEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showComposer();
}

void MeshtasticScreen::sendEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_service == nullptr) return;
    const char *text = lv_textarea_get_text(self->_textArea);
    if (text != nullptr && text[0] != '\0') self->_service->sendTextMessage(text);
    self->hideComposer();
    self->render(self->_service->status());
}

void MeshtasticScreen::cancelEventThunk(lv_event_t *event)
{
    auto *self = static_cast<MeshtasticScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideComposer();
}
