#include "pager_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kWidth = 224;
constexpr int kHeight = 84;
}  // namespace

void PagerAppScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

String PagerAppScreen::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}

String PagerAppScreen::summarizeEvent(const PagerSessionEvent& event) {
  if (event.content.length() > 0) {
    return event.content;
  }
  if (event.type.length() > 0) {
    return event.type;
  }
  return "(event)";
}

void PagerAppScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  (void)group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, kWidth, kHeight);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  mode_ = lv_label_create(root_);
  lv_obj_set_width(mode_, kWidth);
  lv_obj_set_style_text_color(mode_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(mode_, LV_ALIGN_TOP_LEFT, 0, 0);

  title_ = lv_label_create(root_);
  lv_obj_set_width(title_, kWidth);
  lv_obj_set_style_text_color(title_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, 14);

  for (size_t i = 0; i < 4; ++i) {
    body_[i] = lv_label_create(root_);
    lv_obj_set_width(body_[i], kWidth);
    lv_obj_set_style_text_color(body_[i], lv_color_hex(0xD7E0EA), 0);
    lv_obj_align(body_[i], LV_ALIGN_TOP_LEFT, 0, 28 + static_cast<int>(i) * 13);
    lv_label_set_long_mode(body_[i], LV_LABEL_LONG_CLIP);
  }
}

void PagerAppScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  mode_ = nullptr;
  title_ = nullptr;
  for (auto& label : body_) {
    label = nullptr;
  }
  last_mode_ = "";
  last_title_ = "";
  for (auto& item : last_body_) {
    item = "";
  }
}

void PagerAppScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  String mode = state.pager_screen == PagerScreen::Inbox ? "Inbox"
              : state.pager_screen == PagerScreen::Detail ? "Detail"
                                                        : "Compose";
  if (state.pager.active_session_id.length() > 0) {
    mode += " | ";
    mode += state.pager.active_session_id;
  }
  setLabelText(mode_, last_mode_, mode);

  String title;
  switch (state.pager_screen) {
    case PagerScreen::Compose:
      title = "Compose prompt";
      break;
    case PagerScreen::Inbox:
      title = "Recent sessions";
      break;
    case PagerScreen::Detail:
      title = "Session detail";
      break;
  }
  setLabelText(title_, last_title_, title);

  std::array<String, 4> lines{};
  if (state.pager_screen == PagerScreen::Inbox) {
    if (state.pager.session_count == 0) {
      lines[0] = "(no sessions yet)";
    } else {
      const size_t start = state.menu.scroll_offset < state.pager.session_count ? state.menu.scroll_offset : 0;
      const size_t end = min(state.pager.session_count, start + 4);
      size_t row = 0;
      for (size_t i = start; i < end && row < lines.size(); ++i, ++row) {
        const PagerSessionSummary& session = state.pager.sessions[i];
        String line = i == state.menu.selected_index ? "> " : "  ";
        line += String(i + 1);
        line += ". ";
        line += shortText(session.title.length() > 0 ? session.title : String("(untitled)"), 18);
        if (session.last_event.length() > 0) {
          line += " | ";
          line += shortText(session.last_event, 16);
        }
        lines[row] = line;
      }
    }
  } else {
    const PagerSessionSummary* session = nullptr;
    if (state.pager.session_count > 0) {
      size_t index = state.menu.selected_index < state.pager.session_count ? state.menu.selected_index : 0;
      session = &state.pager.sessions[index];
    }
    if (session == nullptr) {
      lines[0] = "(no session selected)";
    } else if (state.pager_screen == PagerScreen::Compose) {
      lines[0] = "To: " + shortText(session->thread_id.length() > 0 ? session->thread_id : String("(new thread)"), 20);
      lines[1] = "Branch: " + shortText(session->branch.length() > 0 ? session->branch : String("-"), 20);
      lines[2] = "State: compose";
      lines[3] = state.status_line.length() > 0 ? shortText(state.status_line, 20) : String("Prompt staged");
    } else {
      lines[0] = "Title: " + shortText(session->title.length() > 0 ? session->title : session->session_id, 20);
      lines[1] = "Thread: " + shortText(session->thread_id.length() > 0 ? session->thread_id : String("-"), 20);
      lines[2] = "Branch: " + shortText(session->branch.length() > 0 ? session->branch : String("-"), 20);
      lines[3] = session->pending_approval_id.length() > 0 ? String("Approval: ") + session->pending_approval_id
                                                           : String("State: ") + session->status;
    }
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    setLabelText(body_[i], last_body_[i], lines[i]);
  }
}
#endif
