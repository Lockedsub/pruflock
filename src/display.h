// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Display interface: screens, backlight, battery level.

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "tracker.h"

struct DeviceStatus {
  String ssid;
  int rssi = 0;
  String username;
  int locked = -1;     // 1 / 0 / -1 unknown
  int battery = -1;    // percent, -1 = USB
};

/** 240×135 landscape UI. Everything is drawn into a sprite (16-bit, 8-bit if memory is short) and
 *  pushed at once (no flicker). */
class Display {
public:
  void begin();
  void setBrightness(uint8_t level);
  /** Backlight off + panel sleep. Safe to call when not initialised. */
  void off();

  /** Network shown in the top row of status screens; rssi 0 = unknown (no bars). */
  void setNetwork(const String& ssid, int rssi) { ssid_ = ssid; rssi_ = rssi; }

  /** `nameLine`: line1 is the username, drawn large when it fits. */
  void showStatus(const String& title, const String& line1, const String& line2 = "",
                  uint16_t titleColor = TFT_WHITE, bool nameLine = false);
  void showInspection(const Inspection& ins, int pendingCount, time_t now, bool offline, int batteryPct);
  void showDeviceStatus(const DeviceStatus& st);
  void showPortal(const String& ap, const String& apPass, bool cancelHint);
  /** A phone has joined but hasn't opened the page: iOS can take ~45 s to show it by itself. */
  void showPortalJoined(const String& url, bool cancelHint);

private:
  void text(const String& s, int x, int y, uint8_t font);
  int textWidth(const String& s, uint8_t font);
  /** Username, centred: large font if it fits the width, else small. */
  void name(const String& s, int y);
  const uint8_t* loaded_ = nullptr;   // smooth font currently loaded in the sprite
  /** Top row: name left; signal bars (or red "offline") left of the power text at the right. */
  void topRow(const String& ssid, int rssi, int batteryPct, bool offline = false);
  int power(int batteryPct);   // top right: "NN%" (red < 20 %) or "USB"; returns its width
  String ssid_;
  int rssi_ = 0;
  void push();
  TFT_eSPI tft_;
  TFT_eSprite spr_{&tft_};
  bool inited_ = false;
  uint8_t brightness_ = 0;
};

/** Battery in percent, or -1 when on USB power. */
int batteryPercent();
