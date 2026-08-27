#pragma once

#include <lvgl.h>
#include "../services/MeshService.h"

class MeshScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(MeshService *service, BackCallback backCallback, void *userData);
    void show(const MeshStatus &status);
    void render(const MeshStatus &status);

private:
    static void backEventThunk(lv_event_t *event);
    static void composeEventThunk(lv_event_t *event);
    static void sendEventThunk(lv_event_t *event);
    static void cancelEventThunk(lv_event_t *event);
    static const char *nodeTypeName(uint8_t type);

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

    MeshService *_service = nullptr;
    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
