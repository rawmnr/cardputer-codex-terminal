#include "terminal_snapshot.h"

#ifndef StringPrint_h
#include "string_print.h"
#endif
void writeTerminalSnapshot(Print& out, const DeviceState& state, App* active_app, const String& input_line) {
  out.println(state.firmware_name);
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("App: ");
  out.println(active_app != nullptr ? active_app->title() : "none");
  out.print("Menu: ");
  out.println(state.menu.app_menu_open ? "open" : "closed");
  out.print("Mode: ");
  switch (state.ui_mode) {
    case UiMode::Home:
      out.println("home");
      break;
    case UiMode::Menu:
      out.println("menu");
      break;
    case UiMode::Input:
      out.println("input");
      break;
    case UiMode::Modal:
      out.println("modal");
      break;
    case UiMode::Approval:
      out.println("approval");
      break;
    case UiMode::BridgePrompt:
      out.println("bridge prompt");
      break;
  }
  out.print("Battery: ");
  out.print(state.battery_percent);
  out.print("% / ");
  out.println(state.battery_voltage_mv);
  out.print("Codex: ");
  switch (state.codex_state) {
    case CodexState::Offline:
      out.println("offline");
      break;
    case CodexState::Idle:
      out.println("idle");
      break;
    case CodexState::Busy:
      out.println("busy");
      break;
    case CodexState::WaitingForApproval:
      out.println("waiting for approval");
      break;
  }
  if (state.codex_stream_line.length() > 0) {
    out.print("Stream: ");
    out.println(state.codex_stream_line);
  }
  if (state.approval_pending) {
    out.print("Approval: ");
    out.println(state.approval_title.length() > 0 ? state.approval_title : "(untitled)");
    if (state.approval_detail_line.length() > 0) {
      out.print("Approval detail: ");
      out.println(state.approval_detail_line);
    }
  }
  if (state.bridge_prompt_pending) {
    out.print("Bridge prompt: ");
    out.println(state.bridge_prompt_title.length() > 0 ? state.bridge_prompt_title : "(untitled)");
    if (state.bridge_prompt_detail.length() > 0) {
      out.print("Bridge detail: ");
      out.println(state.bridge_prompt_detail);
    }
  }
  out.println();
  if (active_app != nullptr) {
    active_app->render(out, state);
  }
  out.println();
  if (state.menu.command_palette_open) {
    out.print("/ ");
    out.println(input_line);
  } else {
    out.print("> ");
    out.println(input_line);
  }
}

String buildTerminalSnapshot(const DeviceState& state, App* active_app, const String& input_line) {
  StringPrint out;
  writeTerminalSnapshot(out, state, active_app, input_line);
  return out.str();
}
