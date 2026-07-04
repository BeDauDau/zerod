#include "display.h"
#include "display.hpp"

#include <lvgl.h>

#include "board_conf.h"

namespace display {

static LGFX _tft;
static lv_display_t *_lvd = nullptr;

static lv_color_t _buf1[SCREEN_WIDTH * SCREEN_HEIGHT / 10];
static lv_color_t _buf2[SCREEN_WIDTH * SCREEN_HEIGHT / 10];

static lv_indev_t *_indev = nullptr;

void _flush_display(lv_display_t *display, const lv_area_t *area, uint8_t *color);
void _read_touchscreen(lv_indev_t *indev, lv_indev_data_t *data);

void init() {
  _tft.init();
  _tft.setBrightness(0);

  _lvd = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(_lvd, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(_lvd, _buf1, _buf2, sizeof(_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(_lvd, _flush_display);

  _indev = lv_indev_create();
  lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(_indev, _read_touchscreen);

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_STATE_DEFAULT);
}

void set_backlight(uint8_t target) {
  _tft.setBrightness(target);
}

void _flush_display(lv_display_t *display, const lv_area_t *area, uint8_t *color) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(color, w * h);

  if (_tft.getStartCount() == 0) {
    _tft.endWrite();
  }

  _tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)color);
  lv_display_flush_ready(display);
}

void _read_touchscreen(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = _tft.getTouch(&touchX, &touchY);

  if (!touched) {
    data->state = LV_INDEV_STATE_RELEASED;
  } else {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

}