#include "approvals_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kWidth = 224;
constexpr int kHeight = 84;
}

void ApprovalsInboxScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) return;
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

String ApprovalsInboxScreen::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) return value;
  if (max_chars <= 1) return value.substring(0, max_chars);
  return value.substring(0, max_chars - 1) + "~";
}

void ApprovalsInboxScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, kWidth, kHeight);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  title_ = lv_label_create(root_);
  lv_obj_set_width(title_, kWidth);
  lv_obj_set_style_text_color(title_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 0);

  for (size_t i = 0; i < 4; ++i) {
    body_[i] = lv_label_create(root_);
    lv_obj_set_width(body_[i], kWidth);
    lv_obj_set_style_text_color(body_[i], lv_color_hex(0xD7E0EA), 0);
    lv_obj_align(body_[i], LV_ALIGN_TOP_LEFT, 0, 15 + static_cast<int>(i) * 14);
    lv_label_set_long_mode(body_[i], LV_LABEL_LONG_CLIP);
  }
}

void ApprovalsInboxScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  title_ = nullptr;
  for (auto& label : body_) label = nullptr;
  last_title_ = "";
  for (auto& item : last_body_) item = "";
}

void ApprovalsInboxScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) return;

  String title = state.approvals.screen == ApprovalsScreen::Inbox ? "Approvals" : "Approval detail";
  if (state.approvals.active_run_id.length() > 0) {
    title += " | ";
    title += state.approvals.active_run_id;
  }
  setLabelText(title_, last_title_, title);

  std::array<String, 4> lines{};
  if (state.approvals.screen == ApprovalsScreen::Inbox) {
    if (state.approvals.approval_count == 0) {
      lines[0] = "(no approvals)";
    } else {
      const size_t start = state.approvals.scroll_offset < state.approvals.approval_count ? state.approvals.scroll_offset : 0;
      const size_t end = min(state.approvals.approval_count, start + 4);
      size_t row = 0;
      for (size_t i = start; i < end && row < lines.size(); ++i, ++row) {
        const ApprovalInboxItem& item = state.approvals.approvals[i];
        String line = i == state.approvals.selected_index ? "> " : "  ";
        line += shortText(item.run_title.length() > 0 ? item.run_title : item.run_id, 10);
        line += " ";
        line += shortText(item.title, 14);
        line += " ";
        line += item.danger_level == "high" ? "DANGER" : "normal";
        lines[row] = line;
      }
    }
  } else {
    const ApprovalInboxItem* item = nullptr;
    if (state.approvals.approval_count > 0) {
      const size_t index = state.approvals.selected_index < state.approvals.approval_count ? state.approvals.selected_index : 0;
      item = &state.approvals.approvals[index];
    }
    if (item == nullptr) {
      lines[0] = "(no approval selected)";
    } else {
      lines[0] = "Run: " + shortText(item->run_title.length() > 0 ? item->run_title : item->run_id, 20);
      lines[1] = "Action: " + shortText(item->title, 20);
      lines[2] = "Mode: " + shortText(item->mode, 20);
      lines[3] = item->danger_level == "high" ? String("Enter twice to approve") : String("Enter approve / Del reject");
    }
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    setLabelText(body_[i], last_body_[i], lines[i]);
  }
}
#endif
