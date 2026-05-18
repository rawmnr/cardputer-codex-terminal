#include "bridge_screen.h"

#if USE_LVGL_UI
void BridgeScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

void BridgeScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, 224, 84);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  status_ = lv_label_create(root_);
  lv_obj_set_width(status_, 224);
  lv_obj_set_style_text_color(status_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(status_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_CLIP);

  title_ = lv_label_create(root_);
  lv_obj_set_width(title_, 224);
  lv_obj_set_style_text_color(title_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 14);
  lv_label_set_long_mode(title_, LV_LABEL_LONG_CLIP);

  detail_ = lv_label_create(root_);
  lv_obj_set_width(detail_, 224);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0xD7E0EA), 0);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, 0, 28);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_WRAP);

  for (size_t i = 0; i < 3; ++i) {
    options_[i] = lv_label_create(root_);
    lv_obj_set_width(options_[i], 224);
    lv_obj_set_style_text_color(options_[i], lv_color_hex(0x9FB0BF), 0);
    lv_obj_align(options_[i], LV_ALIGN_TOP_LEFT, 0, 52 + static_cast<int>(i) * 10);
    lv_label_set_long_mode(options_[i], LV_LABEL_LONG_CLIP);
  }
}

void BridgeScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  status_ = nullptr;
  title_ = nullptr;
  detail_ = nullptr;
  for (auto& option : options_) {
    option = nullptr;
  }
  last_status_ = "";
  last_title_ = "";
  last_detail_ = "";
  for (auto& text : last_options_) {
    text = "";
  }
}

void BridgeScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  setLabelText(status_, last_status_, state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("Bridge idle"));

  String title;
  String detail;
  switch (state.bridge_prompt_kind) {
    case BridgePromptKind::None:
      title = "No active prompt";
      detail = "Try /notify, /ask, /confirm";
      break;
    case BridgePromptKind::Notification:
      title = "Notification";
      detail = state.bridge_prompt_title;
      break;
    case BridgePromptKind::Question:
      title = "Question";
      detail = state.bridge_prompt_title;
      break;
    case BridgePromptKind::Confirmation:
      title = "Confirmation";
      detail = state.bridge_prompt_title;
      break;
  }
  if (state.bridge_prompt_detail.length() > 0) {
    detail += detail.length() > 0 ? String(" | ") : String("");
    detail += state.bridge_prompt_detail;
  }
  setLabelText(title_, last_title_, title);
  setLabelText(detail_, last_detail_, detail);

  for (size_t i = 0; i < 3; ++i) {
    String text;
    if (state.bridge_prompt_kind == BridgePromptKind::None) {
      text = i == 0 ? "/notify <text>" : i == 1 ? "/ask t|d|a|b|c" : "/confirm <text>";
    } else if (i < state.bridge_prompt_option_count) {
      text = state.bridge_prompt_options[i];
      if (i == state.bridge_prompt_selected_index) {
        text = String("> ") + text;
      } else {
        text = String("  ") + text;
      }
    }
    setLabelText(options_[i], last_options_[i], text);
  }
}
#endif
