#include "lvgl_port.h"
#include "runtime/resource_guard.h"

#if USE_LVGL_UI && defined(ARDUINO)
#include <M5Cardputer.h>
#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

namespace {
LvglPort* g_port = nullptr;
}

void LvglPort::begin() {
  lv_init();

  g_port = this;
  last_tick_ms_ = millis();
  metrics_ = {};

  // M5GFX expects 16-bit flush buffers in swapped RGB565 byte order here.
  M5Cardputer.Display.setSwapBytes(true);

  display_ = lv_display_create(kScreenWidth, kScreenHeight);
  if (display_ == nullptr) {
    metrics_.ready = false;
    return;
  }

  lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
#if defined(ESP32)
  buffer_bytes_ = kBufferPixels * sizeof(lv_color_t);
  buffer_a_ = static_cast<lv_color_t*>(heap_caps_malloc(buffer_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  buffer_b_ = static_cast<lv_color_t*>(heap_caps_malloc(buffer_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (buffer_a_ == nullptr || buffer_b_ == nullptr) {
    Serial.println("[lvgl] partial buffer alloc failed");
    if (buffer_a_ != nullptr) {
      heap_caps_free(buffer_a_);
      buffer_a_ = nullptr;
    }
    if (buffer_b_ != nullptr) {
      heap_caps_free(buffer_b_);
      buffer_b_ = nullptr;
    }
    metrics_.ready = false;
    return;
  }
  metrics_.buffer_bytes = static_cast<uint32_t>(buffer_bytes_);
  metrics_.double_buffered = true;
  metrics_.free_internal_heap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  lv_display_set_buffers(display_, buffer_a_, buffer_b_, buffer_bytes_, LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
  lv_display_set_buffers(display_, buffer_.data(), nullptr, sizeof(buffer_), LV_DISPLAY_RENDER_MODE_PARTIAL);
  metrics_.buffer_bytes = static_cast<uint32_t>(sizeof(buffer_));
  metrics_.double_buffered = false;
#endif
  lv_display_set_flush_cb(display_, flushCb);
  lv_display_set_default(display_);

  group_ = lv_group_create();
  lv_group_set_default(group_);

  keypad_ = lv_indev_create();
  lv_indev_set_type(keypad_, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(keypad_, readCb);
  lv_indev_set_group(keypad_, group_);
  metrics_.ready = display_ != nullptr && keypad_ != nullptr && group_ != nullptr;
}

void LvglPort::tick() {
  const unsigned long now = millis();
  const uint32_t elapsed = now >= last_tick_ms_ ? static_cast<uint32_t>(now - last_tick_ms_) : 0;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    last_tick_ms_ = now;
  }

  if (!ready()) {
    return;
  }

  lv_timer_handler();
}

void LvglPort::pushKey(lv_key_t key, bool pressed) {
  pushKeyEvent(key, pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED);
}

bool LvglPort::ready() const {
  return display_ != nullptr && keypad_ != nullptr && group_ != nullptr;
}

lv_group_t* LvglPort::group() const {
  return group_;
}

void LvglPort::flushCb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
  if (g_port != nullptr) {
    g_port->flushArea(area, px_map);
  }
  lv_display_flush_ready(display);
}

void LvglPort::readCb(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  if (g_port == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
    return;
  }

  KeyEvent event;
  if (g_port->popKey(event)) {
    data->state = event.state;
    data->key = event.key;
    return;
  }

  data->state = LV_INDEV_STATE_RELEASED;
  data->key = 0;
}

void LvglPort::flushArea(const lv_area_t* area, const uint8_t* px_map) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  if (width <= 0 || height <= 0) {
    return;
  }

  const uint32_t bytes = static_cast<uint32_t>(width * height * static_cast<int32_t>(sizeof(lv_color_t)));
  const uint32_t start_us = micros();
  const ResourceGuard::ScopedLock spi_guard(ResourceGuard::Kind::Spi, 5);
  if (!spi_guard.acquired()) {
    ++metrics_.flush_failures;
    return;
  }

  M5Cardputer.Display.startWrite();
  M5Cardputer.Display.pushImage(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t*>(px_map));
  M5Cardputer.Display.endWrite();

  const uint32_t elapsed_us = static_cast<uint32_t>(micros() - start_us);
  ++metrics_.flush_count;
  metrics_.flush_bytes += bytes;
  metrics_.flush_total_us += elapsed_us;
  if (elapsed_us > metrics_.flush_max_us) {
    metrics_.flush_max_us = elapsed_us;
  }
}

bool LvglPort::popKey(KeyEvent& event) {
  if (key_head_ == key_tail_) {
    return false;
  }

  event = key_queue_[key_tail_];
  key_tail_ = (key_tail_ + 1) % kKeyQueueSize;
  return true;
}

void LvglPort::pushKeyEvent(lv_key_t key, lv_indev_state_t state) {
  const size_t next_head = (key_head_ + 1) % kKeyQueueSize;
  if (next_head == key_tail_) {
    key_tail_ = (key_tail_ + 1) % kKeyQueueSize;
  }

  key_queue_[key_head_].key = key;
  key_queue_[key_head_].state = state;
  key_head_ = next_head;
}
#endif
