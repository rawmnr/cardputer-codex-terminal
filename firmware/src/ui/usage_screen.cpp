#include "usage_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kWidth = 224;
constexpr int kHeight = 84;

lv_color_t usageColor(int percent) {
  if (percent < 50) {
    return lv_color_hex(0x56F098);
  }
  if (percent < 80) {
    return lv_color_hex(0xFECC60);
  }
  return lv_color_hex(0xF28A8A);
}
}  // namespace

void UsageScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

String UsageScreen::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}

void UsageScreen::setBarColor(lv_obj_t* bar, int percent) {
  lv_obj_set_style_bg_color(bar, usageColor(percent), LV_PART_INDICATOR);
}

void UsageScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, kWidth, kHeight);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  primary_title_ = lv_label_create(root_);
  lv_obj_set_style_text_color(primary_title_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_set_style_text_font(primary_title_, &lv_font_montserrat_14, 0);
  lv_obj_align(primary_title_, LV_ALIGN_TOP_LEFT, 0, 0);

  primary_value_ = lv_label_create(root_);
  lv_obj_set_style_text_color(primary_value_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(primary_value_, LV_ALIGN_TOP_RIGHT, 0, 0);

  primary_bar_ = lv_bar_create(root_);
  lv_obj_set_size(primary_bar_, kWidth, 10);
  lv_obj_align(primary_bar_, LV_ALIGN_TOP_LEFT, 0, 16);
  lv_bar_set_range(primary_bar_, 0, 100);
  lv_obj_set_style_bg_color(primary_bar_, lv_color_hex(0x203246), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(primary_bar_, LV_OPA_COVER, LV_PART_MAIN);

  secondary_title_ = lv_label_create(root_);
  lv_obj_set_style_text_color(secondary_title_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_set_style_text_font(secondary_title_, &lv_font_montserrat_14, 0);
  lv_obj_align(secondary_title_, LV_ALIGN_TOP_LEFT, 0, 30);

  secondary_value_ = lv_label_create(root_);
  lv_obj_set_style_text_color(secondary_value_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(secondary_value_, LV_ALIGN_TOP_RIGHT, 0, 30);

  secondary_bar_ = lv_bar_create(root_);
  lv_obj_set_size(secondary_bar_, kWidth, 10);
  lv_obj_align(secondary_bar_, LV_ALIGN_TOP_LEFT, 0, 46);
  lv_bar_set_range(secondary_bar_, 0, 100);
  lv_obj_set_style_bg_color(secondary_bar_, lv_color_hex(0x203246), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(secondary_bar_, LV_OPA_COVER, LV_PART_MAIN);

  reset_ = lv_label_create(root_);
  lv_obj_set_width(reset_, kWidth);
  lv_obj_set_style_text_color(reset_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(reset_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_label_set_long_mode(reset_, LV_LABEL_LONG_CLIP);
}

void UsageScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  primary_title_ = nullptr;
  primary_bar_ = nullptr;
  primary_value_ = nullptr;
  secondary_title_ = nullptr;
  secondary_bar_ = nullptr;
  secondary_value_ = nullptr;
  reset_ = nullptr;
  last_primary_title_ = "";
  last_primary_value_ = "";
  last_secondary_title_ = "";
  last_secondary_value_ = "";
  last_reset_ = "";
}

void UsageScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  const String primary_title = "session";
  String primary_value = state.codex_usage_percent >= 0 ? String(state.codex_usage_percent) + "%" : String("--");
  if (state.codex_usage_window_minutes > 0) {
    primary_value += " / ";
    primary_value += state.codex_usage_window_minutes;
    primary_value += "m";
  }
  setLabelText(primary_title_, last_primary_title_, primary_title);
  setLabelText(primary_value_, last_primary_value_, primary_value);
  lv_bar_set_value(primary_bar_, state.codex_usage_percent >= 0 ? state.codex_usage_percent : 0, LV_ANIM_OFF);
  setBarColor(primary_bar_, state.codex_usage_percent >= 0 ? state.codex_usage_percent : 0);

  const String secondary_title = "weekly";
  String secondary_value = state.codex_usage_secondary_percent >= 0 ? String(state.codex_usage_secondary_percent) + "%" : String("--");
  if (state.codex_usage_secondary_window_minutes > 0) {
    secondary_value += " / ";
    secondary_value += state.codex_usage_secondary_window_minutes;
    secondary_value += "m";
  }
  setLabelText(secondary_title_, last_secondary_title_, secondary_title);
  setLabelText(secondary_value_, last_secondary_value_, secondary_value);
  lv_bar_set_value(secondary_bar_, state.codex_usage_secondary_percent >= 0 ? state.codex_usage_secondary_percent : 0, LV_ANIM_OFF);
  setBarColor(secondary_bar_, state.codex_usage_secondary_percent >= 0 ? state.codex_usage_secondary_percent : 0);

  String reset = state.codex_usage_reset_line.length() > 0 ? state.codex_usage_reset_line : String("Reset data pending...");
  setLabelText(reset_, last_reset_, shortText(reset, 40));
}
#endif
