#include "apps.h"
#include "middleware_link.h"

namespace {
String trim_copy(String value) {
  value.trim();
  return value;
}

String short_text(String value, size_t max_chars) {
  value.trim();
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}

const char* screen_label(RunsScreen screen) {
  switch (screen) {
    case RunsScreen::List: return "RUNS";
    case RunsScreen::Detail: return "DETAIL";
    case RunsScreen::Actions: return "ACTIONS";
    case RunsScreen::Diff: return "DIFF";
    case RunsScreen::Tests: return "TESTS";
  }
  return "RUNS";
}

String mode_label(const String& mode) {
  if (mode == "yolo_worktree") return "yolo";
  if (mode == "review_only") return "review";
  if (mode.length() == 0) return "safe";
  return short_text(mode, 8);
}

String status_label(const String& status) {
  return status.length() == 0 ? String("idle") : short_text(status, 8);
}
}  // namespace

const char* RunsApp::title() const { return "Runs"; }

void RunsApp::setBridge(MiddlewareLink* bridge) { bridge_ = bridge; }

void RunsApp::requestRunList(DeviceState& state) {
  if (bridge_ != nullptr) {
    bridge_->sendRunListRequest();
    state.status_line = "Refreshing runs";
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::requestRunDetail(DeviceState& state, const String& run_id) {
  if (bridge_ != nullptr && run_id.length() > 0) {
    bridge_->sendRunDetailRequest(run_id);
    state.status_line = String("Loading run ") + run_id;
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::showList(DeviceState& state, const String& message) {
  state.runs.screen = RunsScreen::List;
  state.ui_mode = UiMode::Menu;
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::showDetail(DeviceState& state, const String& message) {
  state.runs.screen = RunsScreen::Detail;
  state.ui_mode = UiMode::Menu;
  syncActionsFromDetail(state);
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::showActions(DeviceState& state, const String& message) {
  state.runs.screen = RunsScreen::Actions;
  state.ui_mode = UiMode::Menu;
  state.runs.selected_action_index = 0;
  syncActionsFromDetail(state);
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::showDiff(DeviceState& state, const String& message) {
  state.runs.screen = RunsScreen::Diff;
  state.ui_mode = UiMode::Menu;
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

void RunsApp::showTests(DeviceState& state, const String& message) {
  state.runs.screen = RunsScreen::Tests;
  state.ui_mode = UiMode::Menu;
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

size_t RunsApp::selectedRunIndex(const DeviceState& state) const {
  if (state.runs.run_count == 0) return 0;
  if (state.runs.selected_run_id.length() > 0) {
    for (size_t i = 0; i < state.runs.run_count; ++i) {
      if (state.runs.runs[i].run_id == state.runs.selected_run_id) return i;
    }
  }
  if (state.runs.active_run_id.length() > 0) {
    for (size_t i = 0; i < state.runs.run_count; ++i) {
      if (state.runs.runs[i].run_id == state.runs.active_run_id) return i;
    }
  }
  return state.runs.selected_index < state.runs.run_count ? state.runs.selected_index : 0;
}

const RunSummaryView* RunsApp::selectedRun(const DeviceState& state) const {
  if (state.runs.run_count == 0) return nullptr;
  const size_t index = selectedRunIndex(state);
  return index < state.runs.run_count ? &state.runs.runs[index] : nullptr;
}

void RunsApp::syncSelectionFromState(DeviceState& state) {
  if (state.runs.run_count == 0) {
    state.runs.selected_index = 0;
    state.runs.selected_run_id = "";
    return;
  }
  const size_t index = selectedRunIndex(state);
  state.runs.selected_index = index;
  state.runs.selected_run_id = state.runs.runs[index].run_id;
  if (state.runs.scroll_offset > index) {
    state.runs.scroll_offset = index;
  } else if (index >= state.runs.scroll_offset + kVisibleRows) {
    state.runs.scroll_offset = index - (kVisibleRows - 1);
  }
}

void RunsApp::syncActionsFromDetail(DeviceState& state) {
  state.runs.action_count = 0;
  state.runs.selected_action_index = 0;
  auto push = [&](const char* id) {
    if (state.runs.action_count < state.runs.actions.size()) {
      state.runs.actions[state.runs.action_count++] = id;
    }
  };
  const RunDetailView& d = state.runs.detail;
  if (d.approval_pending) {
    push("approve_once");
    push("reject");
    push("stop");
  }
  if (d.diff_summary.length() > 0 || d.diff_files > 0) push("show_diff");
  if (d.test_summary.length() > 0 || d.test_run > 0) push("show_tests");
  if (d.thread_id.length() > 0) push("open_voice_reply");
  if (!d.merge_ready && (d.status == "done" || d.status == "failed")) push("mark_for_merge");
  if (state.runs.action_count == 0 && d.approval_pending) {
    push("approve_once");
    push("reject");
  }
}

bool RunsApp::runAction(DeviceState& state, const String& action_id) {
  const RunDetailView& d = state.runs.detail;
  if (action_id == "approve_once") {
    return bridge_ != nullptr && d.approval_id.length() > 0 && bridge_->sendApprovalResponse(true, d.approval_id);
  }
  if (action_id == "reject") {
    return bridge_ != nullptr && d.approval_id.length() > 0 && bridge_->sendApprovalResponse(false, d.approval_id);
  }
  if (action_id == "stop") {
    return bridge_ != nullptr && d.thread_id.length() > 0 && bridge_->sendInterrupt(d.thread_id);
  }
  if (action_id == "show_diff") {
    showDiff(state, "Diff summary");
    return true;
  }
  if (action_id == "show_tests") {
    showTests(state, "Test summary");
    return true;
  }
  if (action_id == "open_voice_reply") {
    if (bridge_ != nullptr && d.thread_id.length() > 0) {
      bridge_->sendThreadSelect(d.thread_id);
      state.status_line = "Thread selected for reply";
      append_activity_event(state, state.status_line);
      return true;
    }
    return false;
  }
  if (action_id == "mark_for_merge") {
    return bridge_ != nullptr && d.run_id.length() > 0 && bridge_->sendRunAction("mark_for_merge", d.run_id);
  }
  if (action_id == "pause_run" || action_id == "resume_run" || action_id == "collect_diff" || action_id == "run_tests" || action_id == "generate_merge_report") {
    return bridge_ != nullptr && d.run_id.length() > 0 && bridge_->sendRunAction(action_id, d.run_id);
  }
  return false;
}

void RunsApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Menu;
  state.runs.screen = RunsScreen::List;
  syncSelectionFromState(state);
  requestRunList(state);
  state.status_line = "Browse active runs";
  append_activity_event(state, state.status_line);
}

void RunsApp::onExit(DeviceState& state) { (void)state; }

void RunsApp::onCommand(const String& command, DeviceState& state) {
  const String trimmed = trim_copy(command);
  if (trimmed == "refresh" || trimmed == "/refresh") {
    requestRunList(state);
  }
}

void RunsApp::onAction(UiAction action, DeviceState& state) {
  if (state.runs.run_count == 0) {
    if (action == UiAction::Back) {
      state.status_line = "No runs to browse";
      append_activity_event(state, state.status_line);
    }
    return;
  }

  if (action == UiAction::Up) {
    if (state.runs.screen == RunsScreen::Actions && state.runs.action_count > 0) {
      state.runs.selected_action_index = state.runs.selected_action_index == 0 ? state.runs.action_count - 1 : state.runs.selected_action_index - 1;
      return;
    }
    state.runs.selected_index = selectedRunIndex(state) == 0 ? state.runs.run_count - 1 : selectedRunIndex(state) - 1;
    syncSelectionFromState(state);
    requestRunDetail(state, state.runs.selected_run_id);
    return;
  }

  if (action == UiAction::Down) {
    if (state.runs.screen == RunsScreen::Actions && state.runs.action_count > 0) {
      state.runs.selected_action_index = (state.runs.selected_action_index + 1) % state.runs.action_count;
      return;
    }
    state.runs.selected_index = (selectedRunIndex(state) + 1) % state.runs.run_count;
    syncSelectionFromState(state);
    requestRunDetail(state, state.runs.selected_run_id);
    return;
  }

  if (action == UiAction::Select) {
    if (state.runs.screen == RunsScreen::List) {
      const RunSummaryView* run = selectedRun(state);
      if (run != nullptr) {
        state.runs.selected_run_id = run->run_id;
        requestRunDetail(state, run->run_id);
        showDetail(state, "Run detail");
      }
      return;
    }
    if (state.runs.screen == RunsScreen::Detail) {
      showActions(state, "Run actions");
      return;
    }
    if (state.runs.screen == RunsScreen::Actions && state.runs.action_count > 0 && state.runs.selected_action_index < state.runs.action_count) {
      const String action_id = state.runs.actions[state.runs.selected_action_index];
      if (action_id == "show_diff") {
        showDiff(state, "Diff summary");
        return;
      }
      if (action_id == "show_tests") {
        showTests(state, "Test summary");
        return;
      }
      runAction(state, action_id);
      return;
    }
    if (state.runs.screen == RunsScreen::Diff || state.runs.screen == RunsScreen::Tests) {
      showDetail(state, "Run detail");
      return;
    }
  }

  if (action == UiAction::Back) {
    if (state.runs.screen == RunsScreen::Actions || state.runs.screen == RunsScreen::Diff || state.runs.screen == RunsScreen::Tests) {
      showDetail(state, "Run detail");
      return;
    }
    if (state.runs.screen == RunsScreen::Detail) {
      showList(state, "Browse active runs");
      return;
    }
  }

  if (action == UiAction::Menu) {
    requestRunList(state);
  }
}

void RunsApp::tick(DeviceState& state) { (void)state; }

void RunsApp::render(Print& out, const DeviceState& state) {
  out.print("[ ");
  out.print(screen_label(state.runs.screen));
  out.print(" ]");
  if (state.runs.active_run_id.length() > 0) {
    out.print("  active: ");
    out.print(state.runs.active_run_id);
  }
  out.println();
  out.println();

  if (state.runs.screen == RunsScreen::List) {
    if (state.runs.run_count == 0) {
      out.println("(no runs yet)");
      return;
    }
    const size_t start = state.runs.scroll_offset < state.runs.run_count ? state.runs.scroll_offset : 0;
    const size_t end = min(state.runs.run_count, start + kVisibleRows);
    for (size_t i = start; i < end; ++i) {
      const RunSummaryView& run = state.runs.runs[i];
      out.print(i == selectedRunIndex(state) ? "> " : "  ");
      out.print(short_text(run.title.length() > 0 ? run.title : run.run_id, 12));
      out.print("  ");
      out.print(mode_label(run.mode));
      out.print("  ");
      out.print(status_label(run.status));
      out.println();
      out.print("    ");
      if (run.approval_id.length() > 0) {
        out.print("! ");
        out.println(short_text(run.approval_title.length() > 0 ? run.approval_title : String("approval"), 24));
      } else {
        out.println(short_text(run.last_event.length() > 0 ? run.last_event : String("-"), 24));
      }
    }
    return;
  }

  const RunDetailView& d = state.runs.detail;
  if (d.run_id.length() == 0) {
    out.println("(no run selected)");
    return;
  }

  if (state.runs.screen == RunsScreen::Detail) {
    out.print("Branch: ");
    out.println(short_text(d.branch, 24));
    out.print("Mode:   ");
    out.println(short_text(d.mode, 24));
    out.print("Step:   ");
    out.println(short_text(d.step.length() > 0 ? d.step : d.last_event, 24));
    out.print("Tests:  ");
    if (d.test_run > 0) {
      out.print(d.test_passed);
      out.print(" passed / ");
      out.print(d.test_failed);
      out.print(" failed");
      out.println();
    } else {
      out.println(short_text(d.test_summary.length() > 0 ? d.test_summary : String("-"), 24));
    }
    out.print("Diff:   ");
    if (d.diff_files > 0) {
      out.print(d.diff_insertions);
      out.print(" + / ");
      out.print(d.diff_deletions);
      out.println(" -");
    } else {
      out.println(short_text(d.diff_summary.length() > 0 ? d.diff_summary : String("-"), 24));
    }
    if (d.approval_pending) {
      out.print("Approval: ");
      out.println(short_text(d.approval_title, 22));
    } else if (d.merge_ready) {
      out.println("Merge: ready");
    }
    out.println();
    out.println("Enter: actions");
    out.println("Del: back");
    return;
  }

  if (state.runs.screen == RunsScreen::Actions) {
    out.println("ACTIONS");
    out.println();
    if (state.runs.action_count == 0) {
      out.println("(no actions)");
      return;
    }
    for (size_t i = 0; i < state.runs.action_count && i < kVisibleRows; ++i) {
      out.print(i == state.runs.selected_action_index ? "> " : "  ");
      out.println(state.runs.actions[i]);
    }
    return;
  }

  if (state.runs.screen == RunsScreen::Diff) {
    out.println("DIFF SUMMARY");
    out.println();
    out.print(d.diff_files);
    out.println(" files");
    out.print(d.diff_insertions);
    out.print(" + / ");
    out.print(d.diff_deletions);
    out.println(" -");
    if (d.diff_summary.length() > 0) {
      out.println(short_text(d.diff_summary, 34));
    }
    return;
  }

  out.println("TESTS");
  out.println();
  out.print(d.test_run);
  out.print(" tests  ");
  out.print(d.test_passed);
  out.print(" passed / ");
  out.print(d.test_failed);
  out.print(" failed");
  out.println();
  if (d.test_summary.length() > 0) {
    out.println(short_text(d.test_summary, 34));
  }
}
