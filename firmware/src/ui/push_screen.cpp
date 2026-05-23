#include "push_screen.h"

#if USE_LVGL_UI
void PushScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

void PushScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, 224, 84);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  ptt_.begin(root_);

  status_ = lv_label_create(root_);
  lv_obj_set_width(status_, 224);
  lv_obj_set_style_text_color(status_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(status_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_CLIP);

  intent_label_ = lv_label_create(root_);
  lv_obj_set_width(intent_label_, 224);
  lv_obj_set_style_text_color(intent_label_, lv_color_hex(0xFFCC00), 0);
  lv_obj_align(intent_label_, LV_ALIGN_TOP_LEFT, 0, 16);
  lv_label_set_long_mode(intent_label_, LV_LABEL_LONG_CLIP);

  target_label_ = lv_label_create(root_);
  lv_obj_set_width(target_label_, 224);
  lv_obj_set_style_text_color(target_label_, lv_color_hex(0x00FFCC), 0);
  lv_obj_align(target_label_, LV_ALIGN_TOP_LEFT, 0, 32);
  lv_label_set_long_mode(target_label_, LV_LABEL_LONG_CLIP);

  detail_ = lv_label_create(root_);
  lv_obj_set_width(detail_, 224);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(detail_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_CLIP);
}

void PushScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  status_ = nullptr;
  intent_label_ = nullptr;
  target_label_ = nullptr;
  detail_ = nullptr;
  last_status_ = "";
  last_intent_ = "";
  last_target_ = "";
  last_detail_ = "";
}

void PushScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  ptt_.sync(state, true, false);
  const String status = state.ptt_detail_line.length() > 0 ? state.ptt_detail_line : String("Hold SPACE to record");
  setLabelText(status_, last_status_, status);

  setLabelText(intent_label_, last_intent_, String("Intent: ") + state.voice_intent);
  setLabelText(target_label_, last_target_, String("Target: ") + state.voice_target);

  String detail = String("Samples ");
  detail += state.ptt_samples_captured;
  detail += "/";
  detail += state.ptt_sample_limit;
  detail += "  Peak ";
  detail += state.ptt_peak_amplitude;
  detail += "  ";
  detail += state.ptt_state == PushToTalkState::Idle ? "Idle"
          : state.ptt_state == PushToTalkState::Armed ? "Armed"
          : state.ptt_state == PushToTalkState::Recording ? "Recording"
          : state.ptt_state == PushToTalkState::Ready ? "Ready"
          : "Error";
  setLabelText(detail_, last_detail_, detail);
}
#endif
