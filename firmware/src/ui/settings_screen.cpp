#include "settings_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kWidth = 224;
constexpr int kHeight = 84;
constexpr int kListWidth = 88;
constexpr int kDetailX = 96;
constexpr int kRowH = 18;
}  // namespace

void SettingsScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

void SettingsScreen::applySelectedStyle(lv_obj_t* row, lv_obj_t* label, bool selected) {
  if (row == nullptr || label == nullptr) {
    return;
  }
  if (selected) {
    lv_obj_set_style_bg_color(row, lv_color_hex(0x7FE7FF), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x071521), 0);
  } else {
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  }
}

void SettingsScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, kWidth, kHeight);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  for (size_t i = 0; i < 4; ++i) {
    rows_[i] = lv_obj_create(root_);
    lv_obj_set_size(rows_[i], kListWidth, kRowH);
    lv_obj_align(rows_[i], LV_ALIGN_TOP_LEFT, 0, static_cast<int>(i) * 18);
    lv_obj_set_style_radius(rows_[i], 3, 0);
    lv_obj_set_style_border_width(rows_[i], 0, 0);
    lv_obj_set_style_pad_all(rows_[i], 0, 0);
    lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_SCROLLABLE);

    labels_[i] = lv_label_create(rows_[i]);
    lv_obj_set_style_text_font(labels_[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(labels_[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(labels_[i], LV_ALIGN_LEFT_MID, 6, 0);
    lv_label_set_text(labels_[i], "");
  }

  detail_ = lv_label_create(root_);
  lv_obj_set_size(detail_, kWidth - kDetailX, kHeight);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, kDetailX, 0);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0xD7E0EA), 0);
  lv_obj_set_style_text_font(detail_, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_WRAP);
}

void SettingsScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  for (auto& row : rows_) {
    row = nullptr;
  }
  for (auto& label : labels_) {
    label = nullptr;
  }
  detail_ = nullptr;
  last_detail_ = "";
  for (auto& label : last_labels_) {
    label = "";
  }
}

void SettingsScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  const std::array<String, 4> labels = {"Wi-Fi", "Bridge", "Keymap", "About"};
  for (size_t i = 0; i < labels.size(); ++i) {
    if (rows_[i] != nullptr && labels_[i] != nullptr) {
      setLabelText(labels_[i], last_labels_[i], labels[i]);
      applySelectedStyle(rows_[i], labels_[i], state.menu.selected_index == i);
    }
  }

  String detail;
  switch (state.menu.selected_index) {
    case 0:
      detail = state.wifi_connected
                 ? String("SSID ") + (state.wifi_ssid.length() > 0 ? state.wifi_ssid : String("(none)")) +
                     "\nIP   " + (state.wifi_ip.length() > 0 ? state.wifi_ip : String("-"))
                 : (state.network_status_line.length() > 0 ? state.network_status_line : String("offline"));
      break;
    case 1:
      detail = state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("(idle)");
      break;
    case 2:
      detail = "Ctrl-M menu\nFn+;/. move\nEnter select\nDel back\nSpace PTT";
      break;
    case 3:
      detail = state.firmware_name;
      break;
    default:
      detail = "";
      break;
  }
  setLabelText(detail_, last_detail_, detail);
}
#endif
