#pragma once

#include <Arduino.h>

#include "device_state.h"

class App {
 public:
  virtual ~App() = default;
  virtual const char* title() const = 0;
  virtual void onEnter(DeviceState& state) = 0;
  virtual void onExit(DeviceState& state) = 0;
  virtual void onCommand(const String& command, DeviceState& state) = 0;
  virtual void tick(DeviceState& state) = 0;
  virtual void render(Stream& out, const DeviceState& state) = 0;
};

class BuddyApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Stream& out, const DeviceState& state) override;
};

class PushToCodexApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Stream& out, const DeviceState& state) override;

 private:
  String draft_;
};

class PagerApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Stream& out, const DeviceState& state) override;
};

class McpBridgeApp final : public App {
 public:
  const char* title() const override;
  void onEnter(DeviceState& state) override;
  void onExit(DeviceState& state) override;
  void onCommand(const String& command, DeviceState& state) override;
  void tick(DeviceState& state) override;
  void render(Stream& out, const DeviceState& state) override;
};

