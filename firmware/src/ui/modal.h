#pragma once

#include <Arduino.h>
#include <array>

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

enum class ModalKind {
  None,
  Notification,
  Question,
  Confirmation,
  Approval,
  Error,
};

class ModalWidget {
 public:
  void begin(lv_obj_t* parent, lv_group_t* group);
  void setVisible(bool visible);
  void setContent(ModalKind kind, const String& title, const String& detail,
                  const std::array<String, 4>& options, size_t option_count, size_t selected_index);
  bool visible() const;
  ModalKind kind() const;
  size_t selectedIndex() const;
  const char* selectedLabel() const;
  void focus();

 private:
  static constexpr size_t kMaxOptions = 4;
  static const char* kindLabel(ModalKind kind);
  static void onButtonEvent(lv_event_t* event);
  void refreshButtons();

  lv_obj_t* overlay_ = nullptr;
  lv_obj_t* panel_ = nullptr;
  lv_obj_t* kind_label_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  lv_obj_t* buttons_ = nullptr;
  lv_group_t* group_ = nullptr;
  ModalKind kind_ = ModalKind::None;
  bool visible_ = false;
  size_t selected_index_ = 0;
  size_t option_count_ = 0;
  String title_text_;
  String detail_text_;
  std::array<String, kMaxOptions> option_texts_{};
  std::array<const char*, kMaxOptions + 1> map_{};
};
#else
class ModalWidget {
 public:
  void begin(void*, void*) {}
  void setVisible(bool) {}
  void setContent(int, const String&, const String&, const std::array<String, 4>&, size_t, size_t) {}
  bool visible() const { return false; }
  int kind() const { return 0; }
  size_t selectedIndex() const { return 0; }
  const char* selectedLabel() const { return ""; }
  void focus() {}
};
#endif
