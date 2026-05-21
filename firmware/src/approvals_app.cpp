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

const char* screen_label(ApprovalsScreen screen) {
  switch (screen) {
    case ApprovalsScreen::Inbox: return "APPROVALS";
    case ApprovalsScreen::Detail: return "DETAIL";
  }
  return "APPROVALS";
}
}

const char* ApprovalsApp::title() const { return "Approvals"; }

void ApprovalsApp::setBridge(MiddlewareLink* bridge) { bridge_ = bridge; }

void ApprovalsApp::requestInbox(DeviceState& state) {
  if (bridge_ != nullptr) {
    bridge_->sendApprovalInboxRequest();
    state.status_line = "Refreshing approvals";
    append_activity_event(state, state.status_line);
  }
}

void ApprovalsApp::showInbox(DeviceState& state, const String& message) {
  state.approvals.screen = ApprovalsScreen::Inbox;
  state.ui_mode = UiMode::Menu;
  detail_armed_ = false;
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

void ApprovalsApp::showDetail(DeviceState& state, const String& message) {
  state.approvals.screen = ApprovalsScreen::Detail;
  state.ui_mode = UiMode::Approval;
  detail_armed_ = false;
  if (message.length() > 0) {
    state.status_line = message;
    append_activity_event(state, state.status_line);
  }
}

size_t ApprovalsApp::selectedApprovalIndex(const DeviceState& state) const {
  if (state.approvals.approval_count == 0) return 0;
  if (state.approvals.selected_approval_id.length() > 0) {
    for (size_t i = 0; i < state.approvals.approval_count; ++i) {
      if (state.approvals.approvals[i].approval_id == state.approvals.selected_approval_id) return i;
    }
  }
  if (state.approvals.active_run_id.length() > 0) {
    for (size_t i = 0; i < state.approvals.approval_count; ++i) {
      if (state.approvals.approvals[i].run_id == state.approvals.active_run_id) return i;
    }
  }
  return state.approvals.selected_index < state.approvals.approval_count ? state.approvals.selected_index : 0;
}

const ApprovalInboxItem* ApprovalsApp::selectedApproval(const DeviceState& state) const {
  if (state.approvals.approval_count == 0) return nullptr;
  const size_t index = selectedApprovalIndex(state);
  return index < state.approvals.approval_count ? &state.approvals.approvals[index] : nullptr;
}

void ApprovalsApp::syncSelectionFromState(DeviceState& state) {
  if (state.approvals.approval_count == 0) {
    state.approvals.selected_index = 0;
    state.approvals.selected_approval_id = "";
    return;
  }
  const size_t index = selectedApprovalIndex(state);
  state.approvals.selected_index = index;
  state.approvals.selected_approval_id = state.approvals.approvals[index].approval_id;
  if (state.approvals.scroll_offset > index) {
    state.approvals.scroll_offset = index;
  } else if (index >= state.approvals.scroll_offset + kVisibleRows) {
    state.approvals.scroll_offset = index - (kVisibleRows - 1);
  }
}

bool ApprovalsApp::respond(DeviceState& state, bool approved) {
  const ApprovalInboxItem* item = selectedApproval(state);
  if (item == nullptr || bridge_ == nullptr || item->approval_id.length() == 0) {
    return false;
  }
  if (approved && item->danger_level == "high") {
    if (!detail_armed_) {
      detail_armed_ = true;
      state.status_line = "Press Enter again to approve";
      append_activity_event(state, state.status_line);
      return false;
    }
  }
  detail_armed_ = false;
  return bridge_->sendApprovalResponse(approved, item->approval_id);
}

void ApprovalsApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Menu;
  state.approvals.screen = ApprovalsScreen::Inbox;
  detail_armed_ = false;
  syncSelectionFromState(state);
  requestInbox(state);
  state.status_line = "Browse approvals";
  append_activity_event(state, state.status_line);
}

void ApprovalsApp::onExit(DeviceState& state) {
  (void)state;
  detail_armed_ = false;
}

void ApprovalsApp::onCommand(const String& command, DeviceState& state) {
  const String trimmed = trim_copy(command);
  if (trimmed == "refresh" || trimmed == "/refresh") {
    requestInbox(state);
  }
}

void ApprovalsApp::onAction(UiAction action, DeviceState& state) {
  if (state.approvals.approval_count == 0) {
    if (action == UiAction::Back) {
      state.status_line = "No approvals to browse";
      append_activity_event(state, state.status_line);
    }
    return;
  }

  if (action == UiAction::Up) {
    state.approvals.selected_index = selectedApprovalIndex(state) == 0 ? state.approvals.approval_count - 1 : selectedApprovalIndex(state) - 1;
    detail_armed_ = false;
    syncSelectionFromState(state);
    return;
  }

  if (action == UiAction::Down) {
    state.approvals.selected_index = (selectedApprovalIndex(state) + 1) % state.approvals.approval_count;
    detail_armed_ = false;
    syncSelectionFromState(state);
    return;
  }

  if (action == UiAction::Select) {
    if (state.approvals.screen == ApprovalsScreen::Inbox) {
      const ApprovalInboxItem* item = selectedApproval(state);
      if (item != nullptr) {
        state.approvals.selected_approval_id = item->approval_id;
        showDetail(state, "Approval detail");
      }
      return;
    }
    if (state.approvals.screen == ApprovalsScreen::Detail) {
      if (respond(state, true)) {
        showInbox(state, "Approval sent");
      }
      return;
    }
  }

  if (action == UiAction::Back) {
    if (state.approvals.screen == ApprovalsScreen::Detail) {
      if (detail_armed_) {
        detail_armed_ = false;
        state.status_line = "Approval confirm cleared";
        append_activity_event(state, state.status_line);
        return;
      }
      showInbox(state, "Browse approvals");
      return;
    }
    if (state.approvals.screen == ApprovalsScreen::Inbox) {
      state.status_line = "Approvals closed";
      append_activity_event(state, state.status_line);
    }
  }

  if (action == UiAction::Menu) {
    requestInbox(state);
  }
}

void ApprovalsApp::tick(DeviceState& state) { (void)state; }

void ApprovalsApp::render(Print& out, const DeviceState& state) {
  out.print("[ ");
  out.print(screen_label(state.approvals.screen));
  out.print(" ]");
  if (state.approvals.active_run_id.length() > 0) {
    out.print("  active: ");
    out.print(state.approvals.active_run_id);
  }
  out.println();
  out.println();

  if (state.approvals.screen == ApprovalsScreen::Inbox) {
    if (state.approvals.approval_count == 0) {
      out.println("(no approvals)");
      return;
    }
    const size_t start = state.approvals.scroll_offset < state.approvals.approval_count ? state.approvals.scroll_offset : 0;
    const size_t end = min(state.approvals.approval_count, start + kVisibleRows);
    for (size_t i = start; i < end; ++i) {
      const ApprovalInboxItem& item = state.approvals.approvals[i];
      out.print(i == selectedApprovalIndex(state) ? "> " : "  ");
      out.print(short_text(item.run_title.length() > 0 ? item.run_title : item.run_id, 12));
      out.print("  ");
      out.print(short_text(item.title, 14));
      out.print("  ");
      out.println(item.danger_level == "high" ? "DANGER" : "normal");
      if (item.detail.length() > 0) {
        out.print("    ");
        out.println(short_text(item.detail, 26));
      }
    }
    return;
  }

  const ApprovalInboxItem* item = selectedApproval(state);
  if (item == nullptr) {
    out.println("(no approval selected)");
    return;
  }
  out.print("Run:   ");
  out.println(short_text(item->run_title.length() > 0 ? item->run_title : item->run_id, 24));
  out.print("Action:");
  out.println(short_text(item->title, 24));
  out.print("Mode:  ");
  out.println(short_text(item->mode, 24));
  out.print("State: ");
  out.println(short_text(item->status, 24));
  out.print("Danger:");
  out.println(item->danger_level == "high" ? "DANGER" : "normal");
  out.println();
  out.println(item->danger_level == "high" ? "Enter twice to approve" : "Enter approve  Del reject");
  out.println("Del: reject");
}
