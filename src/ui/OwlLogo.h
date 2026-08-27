#pragma once

#include <lvgl.h>

class OwlLogo {
public:
    void create(lv_obj_t *parent, int x, int y, int width, int height);

private:
    lv_obj_t *_image = nullptr;
};
