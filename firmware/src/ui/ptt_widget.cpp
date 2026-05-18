#include "ptt_widget.h"

#if USE_LVGL_UI
namespace {
constexpr int kPanelX = 8;
constexpr int kPanelY = 64;
constexpr int kPanelWidth = 224;
constexpr int kPanelHeight = 44;

const char* kIdleText = "IDLE";
const char* kArmedText = "ARMED";
const char* kRecordingText = "RECORDING";
const char* kReadyText = "READY";
const char* kErrorText = "ERROR";
const char* kUnknownText = "PTT";

lv_color_t mixColor(uint8_t r, uint8_t g, uint8_t b) {
  return lv_color_hex((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b);
}
}  // namespace

const char* PttWidget::stateLabel(PushToTalkState state) {
  switch (state) {
    case PushToTalkState::Idle:
      return kIdleText;
    case PushToTalkState::Armed:
      return kArmedText;
    case PushToTalkState::Recording:
      return kRecordingText;
    case PushToTalkState::Ready:
      return kReadyText;
    case PushToTalkState::Error:
      return kErrorText;
  }
  return kUnknownText;
}

lv_color_t PttWidget::stateColor(PushToTalkState state) {
  switch (state) {
    case PushToTalkState::Idle:
      return mixColor(0x74, 0x88, 0x96);
    case PushToTalkState::Armed:
      return mixColor(0x7F, 0xE7, 0xFF);
    case PushToTalkState::Recording:
      return mixColor(0xFF, 0x61, 0x61);
    case PushToTalkState::Ready:
      return mixColor(0x56, 0xF0, 0x98);
    case PushToTalkState::Error:
      return mixColor(0xFF, 0x8B, 0x5C);
  }
  return mixColor(0x7F, 0xE7, 0xFF);
}

String PttWidget::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}

void PttWidget::begin(lv_obj_t* parent) {
  panel_ = lv_obj_create(parent);
  lv_obj_remove_style_all(panel_);
  lv_obj_set_size(panel_, kPanelWidth, kPanelHeight);
  lv_obj_align(panel_, LV_ALIGN_TOP_LEFT, kPanelX, kPanelY);
  lv_obj_set_style_bg_color(panel_, lv_color_hex(0x102030), 0);
  lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel_, 1, 0);
  lv_obj_set_style_border_color(panel_, lv_color_hex(0x31506B), 0);
  lv_obj_set_style_radius(panel_, 6, 0);
  lv_obj_set_style_pad_all(panel_, 0, 0);
  lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);

  title_ = lv_label_create(panel_);
  lv_obj_set_style_text_color(title_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 8, 4);
  lv_label_set_text(title_, "Push to Codex");

  state_ = lv_label_create(panel_);
  lv_obj_set_style_text_color(state_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(state_, LV_ALIGN_TOP_RIGHT, -8, 4);
  lv_label_set_text(state_, kIdleText);

  detail_ = lv_label_create(panel_);
  lv_obj_set_width(detail_, kPanelWidth - 16);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0xD7E0EA), 0);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, 8, 18);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_CLIP);
  lv_label_set_text(detail_, "");

  samples_ = lv_label_create(panel_);
  lv_obj_set_style_text_color(samples_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(samples_, LV_ALIGN_BOTTOM_LEFT, 8, -6);
  lv_label_set_text(samples_, "");

  amplitude_ = lv_bar_create(panel_);
  lv_obj_set_size(amplitude_, kPanelWidth - 16, 6);
  lv_obj_align(amplitude_, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_bar_set_range(amplitude_, 0, 255);
  lv_bar_set_value(amplitude_, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(amplitude_, lv_color_hex(0x203246), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(amplitude_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(amplitude_, lv_color_hex(0x7FE7FF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(amplitude_, LV_OPA_COVER, LV_PART_INDICATOR);
}

void PttWidget::setVisible(bool visible) {
  visible_ = visible;
  if (panel_ == nullptr) {
    return;
  }

  if (visible) {
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel_);
  } else {
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
  }
}

void PttWidget::sync(const DeviceState& state, bool active_app_visible, bool modal_visible) {
  const bool should_show = !modal_visible && (active_app_visible || state.ptt_state != PushToTalkState::Idle);
  if (!should_show) {
    setVisible(false);
    return;
  }

  const String title = String("Push to Codex");
  const String state_text = stateLabel(state.ptt_state);
  String detail = state.ptt_detail_line.length() > 0 ? state.ptt_detail_line : String("Hold SPACE to record");
  const size_t sample_limit = state.ptt_sample_limit > 0 ? state.ptt_sample_limit : 0;
  const size_t captured = state.ptt_samples_captured;
  const int peak = constrain(state.ptt_peak_amplitude, 0, 255);

  String samples_text = String(captured);
  if (sample_limit > 0) {
    samples_text += " / ";
    samples_text += sample_limit;
  }

  if (state.ptt_state == PushToTalkState::Recording && sample_limit > 0) {
    const size_t percent = (captured * 100UL) / sample_limit;
    samples_text += " ";
    samples_text += percent;
    samples_text += "%";
  }

  if (state.ptt_state == PushToTalkState::Ready && state.ptt_detail_line.length() == 0) {
    detail = "Processing complete";
  }

  if (state.ptt_state == PushToTalkState::Error && state.ptt_detail_line.length() == 0) {
    detail = "Recording error";
  }

  if (title_ != nullptr && last_title_ != title) {
    last_title_ = title;
    lv_label_set_text(title_, title.c_str());
  }
  if (state_ != nullptr && last_state_ != state_text) {
    last_state_ = state_text;
    lv_label_set_text(state_, state_text.c_str());
    lv_obj_set_style_text_color(state_, stateColor(state.ptt_state), 0);
  }
  if (detail_ != nullptr) {
    const String clipped = shortText(detail, 34);
    if (last_detail_ != clipped) {
      last_detail_ = clipped;
      lv_label_set_text(detail_, clipped.c_str());
    }
  }
  if (samples_ != nullptr) {
    const String clipped = shortText(samples_text, 18);
    if (last_samples_ != clipped) {
      last_samples_ = clipped;
      lv_label_set_text(samples_, clipped.c_str());
    }
  }
  if (amplitude_ != nullptr && last_amplitude_ != peak) {
    last_amplitude_ = peak;
    lv_bar_set_value(amplitude_, peak, LV_ANIM_OFF);
  }

  if (panel_ != nullptr) {
    lv_obj_set_style_border_color(panel_, stateColor(state.ptt_state), 0);
  }

  setVisible(true);
}

bool PttWidget::visible() const {
  return visible_;
}
#endif
