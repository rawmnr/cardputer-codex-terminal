#pragma once

#include <array>
#include <Arduino.h>

#include "device_state.h"
#include "ui_actions.h"

class MiddlewareLink;

class App {
 public:
  virtual ~App() = default;
  virtual const char* title() const = 0;
  virtual void onEnter(DeviceState& state) = 0;
  virtual void onExit(DeviceState& state) = 0;
  virtual void onCommand(const String& command, DeviceState& state) = 0;
  virtual void onTextInput(const String& text, bool backspace, DeviceState& state) {
    (void)text;
    (void)backspace;
    (void)state;
  }
  virtual void onSubmit(const String& command, DeviceState& state) { onCommand(command, state); }
  virtual void onAction(UiAction action, DeviceState& state) {
    (void)action;
    (void)state;
  }
  virtual void onPushToTalk(bool pressed, DeviceState& state) { (void)pressed; (void)state; }
  virtual void tick(DeviceState& state) = 0;
  virtual void render(Print& out, const DeviceState& state) = 0;
};

class BuddyApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;
};

class PushToCodexApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onTextInput(const String& text, bool backspace, DeviceState& state) override;
  void onSubmit(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void onPushToTalk(bool pressed, DeviceState& state) override;
  void setBridge(MiddlewareLink* bridge);
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;

 private:
  static constexpr size_t kChunkSamples = 256;
  static constexpr size_t kMaxSamples = 24000;

  void beginRecording(DeviceState& state);
  void finishRecording(DeviceState& state);
  void appendChunk(const int16_t* data, size_t length, DeviceState& state);
  void updatePttState(DeviceState& state);
  void emitVoicePromptEnvelope(const DeviceState& state) const;

  String draft_;
  std::array<int16_t, kChunkSamples> chunk_buffer_{}; 
  std::array<int16_t, kMaxSamples> captured_samples_{};
  size_t captured_sample_count_ = 0;
  size_t chunk_index_ = 0;
  int peak_amplitude_ = 0;
  bool recording_ = false;
  bool mic_started_ = false;
  MiddlewareLink* bridge_ = nullptr;
};

class PagerApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onTextInput(const String& text, bool backspace, DeviceState& state) override;
  void onSubmit(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void setBridge(MiddlewareLink* bridge);
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;

 private:
  void showCompose(DeviceState& state, const String& message);
  void showInbox(DeviceState& state, const String& message);
  void showDetail(DeviceState& state, const String& message);
  void selectSession(DeviceState& state, size_t index);
  bool sendReply(DeviceState& state, const String& prompt);
  bool canInterrupt(const DeviceState& state) const;
  size_t selectedSessionIndex(const DeviceState& state) const;
  const PagerSessionSummary* selectedSession(const DeviceState& state) const;
  void syncSelectionFromState(DeviceState& state);

  String compose_draft_;
  String detail_note_;
  bool detail_reply_mode_ = false;
  MiddlewareLink* bridge_ = nullptr;
};

class UsageApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onTextInput(const String& text, bool backspace, DeviceState& state) override;
  void onSubmit(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;
};

class McpBridgeApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void onSubmit(const String& command, DeviceState& state) override;
  void setBridge(MiddlewareLink* bridge);
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;

 private:
  void presentNotification(const String& title, const String& detail, DeviceState& state);
  void presentQuestion(const String& title, const String& detail, const String& option1, const String& option2, const String& option3, DeviceState& state);
  void presentConfirmation(const String& detail, DeviceState& state);
  void respondToPrompt(bool accepted, DeviceState& state);

  MiddlewareLink* bridge_ = nullptr;
};

class SettingsApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void onAction(UiAction action, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Print& out, const DeviceState& state) override;

 private:
  static constexpr size_t kItemCount = 4;
  size_t selected_index_ = 0;
};
