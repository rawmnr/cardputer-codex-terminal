#include "modal.h"

#if USE_LVGL_UI
namespace {
constexpr size_t kPanelWidth = 220;
constexpr size_t kPanelHeight = 92;

const char* const kKindUnknown = "MODAL";

}  // namespace

const char* ModalWidget::kindLabel(ModalKind kind) {
  switch (kind) {
    case ModalKind::Notification:
      return "NOTIFICATION";
    case ModalKind::Question:
      return "QUESTION";
    case ModalKind::Confirmation:
      return "CONFIRMATION";
    case ModalKind::Approval:
      return "APPROVAL";
    case ModalKind::Error:
      return "ERROR";
    case ModalKind::None:
      return kKindUnknown;
  }
  return kKindUnknown;
}

void ModalWidget::begin(lv_obj_t* parent, lv_group_t* group) {
  group_ = group;
  overlay_ = lv_obj_create(parent);
  lv_obj_remove_style_all(overlay_);
  lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay_, LV_OPA_70, 0);
  lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);

  panel_ = lv_obj_create(overlay_);
  lv_obj_set_size(panel_, kPanelWidth, kPanelHeight);
  lv_obj_center(panel_);
  lv_obj_set_style_bg_color(panel_, lv_color_hex(0x102030), 0);
  lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel_, lv_color_hex(0x31506B), 0);
  lv_obj_set_style_border_width(panel_, 1, 0);
  lv_obj_set_style_radius(panel_, 6, 0);
  lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

  kind_label_ = lv_label_create(panel_);
  lv_obj_set_style_text_color(kind_label_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(kind_label_, LV_ALIGN_TOP_LEFT, 10, 8);
  lv_label_set_text(kind_label_, "MODAL");

  title_ = lv_label_create(panel_);
  lv_obj_set_width(title_, kPanelWidth - 20);
  lv_obj_set_style_text_color(title_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 10, 24);
  lv_label_set_long_mode(title_, LV_LABEL_LONG_CLIP);
  lv_label_set_text(title_, "");

  detail_ = lv_label_create(panel_);
  lv_obj_set_width(detail_, kPanelWidth - 20);
  lv_obj_set_height(detail_, 28);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0xD7E0EA), 0);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, 10, 42);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_WRAP);
  lv_label_set_text(detail_, "");

  buttons_ = lv_buttonmatrix_create(panel_);
  lv_obj_set_size(buttons_, kPanelWidth - 20, 24);
  lv_obj_align(buttons_, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_add_event_cb(buttons_, onButtonEvent, LV_EVENT_VALUE_CHANGED, this);
  if (group_ != nullptr) {
    lv_group_add_obj(group_, buttons_);
  }
  setVisible(false);
}

void ModalWidget::setVisible(bool visible) {
  visible_ = visible;
  if (overlay_ == nullptr) {
    return;
  }

  if (visible) {
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_);
    if (group_ != nullptr && buttons_ != nullptr && option_count_ > 0) {
      lv_group_focus_obj(buttons_);
    }
  } else {
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

void ModalWidget::refreshButtons() {
  if (buttons_ == nullptr) {
    return;
  }

  lv_buttonmatrix_set_map(buttons_, map_.data());
  lv_buttonmatrix_set_one_checked(buttons_, true);
  if (option_count_ > 0 && selected_index_ < option_count_) {
    lv_buttonmatrix_set_selected_button(buttons_, static_cast<uint32_t>(selected_index_));
  }
  if (visible_ && group_ != nullptr && option_count_ > 0) {
    lv_group_focus_obj(buttons_);
  }
}

void ModalWidget::setContent(ModalKind kind, const String& title, const String& detail,
                             const std::array<String, 4>& options, size_t option_count, size_t selected_index) {
  kind_ = kind;
  title_text_ = title;
  detail_text_ = detail;
  selected_index_ = option_count == 0 ? 0 : min(selected_index, option_count - 1);
  option_count_ = min(option_count, kMaxOptions);

  if (kind_ == ModalKind::Notification || kind_ == ModalKind::Error) {
    option_count_ = 1;
    option_texts_[0] = "OK";
  } else if (kind_ == ModalKind::Approval) {
    option_count_ = 2;
    option_texts_[0] = "Accept";
    option_texts_[1] = "Reject";
  } else {
    for (size_t i = 0; i < kMaxOptions; ++i) {
      option_texts_[i] = i < option_count_ ? options[i] : "";
    }
  }

  if (option_count_ == 0) {
    option_texts_[0] = "OK";
    option_count_ = 1;
  }

  map_[0] = option_texts_[0].c_str();
  for (size_t i = 1; i < option_count_; ++i) {
    map_[i] = option_texts_[i].c_str();
  }
  map_[option_count_] = nullptr;

  if (kind_label_ != nullptr) {
    lv_label_set_text(kind_label_, kindLabel(kind_));
  }
  if (title_ != nullptr) {
    lv_label_set_text(title_, title_text_.c_str());
  }
  if (detail_ != nullptr) {
    lv_label_set_text(detail_, detail_text_.c_str());
  }

  refreshButtons();
  setVisible(kind_ != ModalKind::None);
}

bool ModalWidget::visible() const {
  return visible_;
}

ModalKind ModalWidget::kind() const {
  return kind_;
}

size_t ModalWidget::selectedIndex() const {
  return selected_index_;
}

const char* ModalWidget::selectedLabel() const {
  if (selected_index_ >= option_count_) {
    return "";
  }
  return option_texts_[selected_index_].c_str();
}

void ModalWidget::focus() {
  if (visible_ && buttons_ != nullptr && option_count_ > 0) {
    lv_group_focus_obj(buttons_);
  }
}

void ModalWidget::onButtonEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  auto* self = static_cast<ModalWidget*>(lv_event_get_user_data(event));
  if (self == nullptr || self->buttons_ == nullptr) {
    return;
  }
  const uint32_t index = lv_buttonmatrix_get_selected_button(self->buttons_);
  if (index != LV_BUTTONMATRIX_BUTTON_NONE) {
    self->selected_index_ = index;
  }
}
#endif
