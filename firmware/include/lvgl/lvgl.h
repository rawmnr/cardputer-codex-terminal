#pragma once

#if defined(USE_LVGL_UI) && USE_LVGL_UI
#include LVGL_EXTERNAL_HEADER
#else
#include M5GFX_LVGL_SHIM_HEADER
typedef struct lv_font_t lv_font_t;
#endif
