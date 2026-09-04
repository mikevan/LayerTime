#pragma once

#include <lvgl.h>

class OwlLogo {
public:
    void create(lv_obj_t *parent, int x, int y, int width, int height);
    void setHidden(bool hidden);

private:
    lv_obj_t *_image = nullptr;
};
