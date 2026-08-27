#pragma once

#include <lvgl.h>
#include "../services/MeshtasticService.h"

// Receive-only Meshtastic viewer - deliberately separate from MeshScreen
// (MeshCore) so the two protocols can be compared side by side. No transmit
// UI yet; this is the first pass the user asked to evaluate.
class MeshtasticScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(MeshtasticService *service, BackCallback backCallback, void *userData);
    void show(const MeshtasticStatus &status);
    void render(const MeshtasticStatus &status);

private:
    static void backEventThunk(lv_event_t *event);
    static void composeEventThunk(lv_event_t *event);
    static void sendEventThunk(lv_event_t *event);
    static void cancelEventThunk(lv_event_t *event);

    void showComposer();
    void hideComposer();

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_summary = nullptr;
    lv_obj_t *_nodes = nullptr;
    lv_obj_t *_messages = nullptr;
    lv_obj_t *_composer = nullptr;
    lv_obj_t *_textArea = nullptr;
    lv_obj_t *_keyboard = nullptr;

    MeshtasticService *_service = nullptr;
    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
