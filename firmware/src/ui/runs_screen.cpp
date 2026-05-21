#include "runs_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kWidth = 224;
constexpr int kHeight = 84;
}

void RunsDashboardScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) return;
  cache = value;
  lv_label_set_text(obj, value.c_str());
}

String RunsDashboardScreen::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) return value;
  if (max_chars <= 1) return value.substring(0, max_chars);
  return value.substring(0, max_chars - 1) + "~";
}

void RunsDashboardScreen::attach(lv_obj_t* parent, lv_group_t* group) {
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

void RunsDashboardScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  title_ = nullptr;
  for (auto& label : body_) label = nullptr;
  last_title_ = "";
  for (auto& item : last_body_) item = "";
}

void RunsDashboardScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) return;

  const char* mode = state.runs.screen == RunsScreen::List ? "Runs"
                  : state.runs.screen == RunsScreen::Detail ? "Detail"
                  : state.runs.screen == RunsScreen::Actions ? "Actions"
                  : state.runs.screen == RunsScreen::Diff ? "Diff"
                  : "Tests";
  String title = String(mode);
  if (state.runs.active_run_id.length() > 0) {
    title += " | ";
    title += state.runs.active_run_id;
  }
  setLabelText(title_, last_title_, title);

  std::array<String, 4> lines{};
  if (state.runs.screen == RunsScreen::List) {
    if (state.runs.run_count == 0) {
      lines[0] = "(no runs yet)";
    } else {
      const size_t start = state.runs.scroll_offset < state.runs.run_count ? state.runs.scroll_offset : 0;
      const size_t end = min(state.runs.run_count, start + 4);
      size_t row = 0;
      for (size_t i = start; i < end && row < lines.size(); ++i, ++row) {
        const RunSummaryView& run = state.runs.runs[i];
        String line = i == state.runs.selected_index ? "> " : "  ";
        line += shortText(run.title.length() > 0 ? run.title : run.run_id, 12);
        line += " ";
        line += shortText(run.mode, 8);
        line += " ";
        line += shortText(run.status, 8);
        if (run.approval_id.length() > 0) {
          line += " !";
          line += shortText(run.approval_title.length() > 0 ? run.approval_title : String("approval"), 10);
        } else if (run.last_event.length() > 0) {
          line += " ";
          line += shortText(run.last_event, 10);
        }
        lines[row] = line;
      }
    }
  } else {
    const RunDetailView& d = state.runs.detail;
    lines[0] = "Branch: " + shortText(d.branch, 20);
    lines[1] = "Mode: " + shortText(d.mode, 20);
    if (state.runs.screen == RunsScreen::Detail) {
      lines[2] = "Step: " + shortText(d.step.length() > 0 ? d.step : d.last_event, 20);
      lines[3] = d.approval_pending ? String("Approval: ") + shortText(d.approval_title, 14) : (d.merge_ready ? String("Merge ready") : String("Enter: actions"));
    } else if (state.runs.screen == RunsScreen::Actions) {
      lines[2] = "Action: " + (state.runs.action_count > 0 ? state.runs.actions[state.runs.selected_action_index] : String("-"));
      lines[3] = "Enter run / Del back";
    } else if (state.runs.screen == RunsScreen::Diff) {
      lines[2] = String(d.diff_files) + " files";
      lines[3] = String(d.diff_insertions) + " + / " + String(d.diff_deletions) + " -";
    } else {
      lines[2] = String(d.test_passed) + " passed / " + String(d.test_failed) + " failed";
      lines[3] = shortText(d.test_summary.length() > 0 ? d.test_summary : String("-"), 22);
    }
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    setLabelText(body_[i], last_body_[i], lines[i]);
  }
}
#endif
