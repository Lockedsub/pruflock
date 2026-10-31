// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Display: draws every screen into a sprite and drives the backlight.

#include "display.h"
#include "config.h"
#include "fonts.h"
#include "i18n.h"

static constexpr int W = 240, H = 135;
static constexpr int BL_CHANNEL = 0;
static constexpr uint16_t COL_INSPECT = 0xFD20;  // orange
static constexpr uint16_t COL_UNLOCKED = 0xFA20; // orange-red (255,69,0): unlocked is the state to watch

void Display::begin() {
  if (inited_) return;
  gpio_hold_dis((gpio_num_t)PIN_BL);
  ledcAttachChannel(PIN_BL, 5000, 8, BL_CHANNEL);
  ledcWrite(PIN_BL, 0);
  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);
  // 16-bit keeps the anti-aliased font edges clean; 8-bit is the fallback if memory is short.
  spr_.setColorDepth(16);
  if (!spr_.createSprite(W, H)) {
    spr_.setColorDepth(8);
    spr_.createSprite(W, H);
  }
  inited_ = true;
}

void Display::setBrightness(uint8_t level) {
  if (!inited_ || level == brightness_) return;
  brightness_ = level;
  ledcWrite(PIN_BL, level);
}

void Display::off() {
  if (inited_) {
    ledcWrite(PIN_BL, 0);
    brightness_ = 0;
    tft_.writecommand(0x28);  // DISPOFF
    tft_.writecommand(0x10);  // SLPIN
    ledcDetach(PIN_BL);
  }
  // Keep the backlight transistor firmly off through deep sleep.
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, LOW);
  gpio_hold_en((gpio_num_t)PIN_BL);
  gpio_deep_sleep_hold_en();
  inited_ = false;
}

void Display::push() {
  spr_.pushSprite(0, 0);
}

// font: 2 = small, 4 = large (both smooth, with umlauts), 7 = built-in 7-segment digits,
// 40 = built-in font 4 (ASCII digits, used scaled for 8-digit codes).
void Display::text(const String& s, int x, int y, uint8_t font) {
  const uint8_t* want = font == 2 ? FONT_SMALL : font == 4 ? FONT_LARGE : nullptr;
  if (want != loaded_) {
    if (loaded_) spr_.unloadFont();
    if (want) spr_.loadFont(want);
    loaded_ = want;
  }
  spr_.drawString(s, x, y, font == 40 ? 4 : font);
}

int Display::textWidth(const String& s, uint8_t font) {
  text("", 0, 0, font);   // just selects the font
  return spr_.textWidth(s, font == 40 ? 4 : font);
}

int Display::power(int batteryPct) {
  String s = batteryPct < 0 ? String("USB") : String(batteryPct) + "%";
  spr_.setTextDatum(TR_DATUM);
  spr_.setTextColor(batteryPct >= 0 && batteryPct < 20 ? TFT_RED : TFT_LIGHTGREY, TFT_BLACK);
  text(s, W - 4, 3, 2);
  return textWidth(s, 2);
}

void Display::topRow(const String& name, int rssi, int batteryPct, bool offline) {
  constexpr int GAP = 6;
  int right = W - 4 - power(batteryPct) - GAP;   // right edge of the signal slot

  // Signal slot, left of the power text: red "offline", bars, or nothing (not connected yet).
  if (offline) {
    spr_.setTextDatum(TR_DATUM);
    spr_.setTextColor(TFT_RED, TFT_BLACK);
    text(tr(S_OFFLINE), right, 3, 2);
    right -= textWidth(tr(S_OFFLINE), 2) + GAP;
  } else if (rssi != 0) {
    const int thresholds[4] = {-85, -75, -65, -55};
    const int bx = right - 4 * 6 + 2, by = 3 + 14;   // bottom on the text baseline (small font ascent 14)
    for (int i = 0; i < 4; i++) {
      int h = 4 + i * 4;
      uint16_t c = rssi >= thresholds[i] ? TFT_GREEN : TFT_DARKGREY;
      spr_.fillRect(bx + i * 6, by - h, 4, h, c);
    }
    right = bx - GAP;
  }

  spr_.setTextDatum(TL_DATUM);
  spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String ssid = name;
  const int maxW = right - 4;
  if (textWidth(ssid, 2) > maxW) {
    while (ssid.length() > 1 && textWidth(ssid + "…", 2) > maxW) {
      unsigned n = ssid.length() - 1;
      while (n > 0 && (ssid[n] & 0xC0) == 0x80) n--;   // drop whole UTF-8 characters
      ssid.remove(n);
    }
    ssid += "…";
  }
  text(ssid, 4, 3, 2);
}

void Display::name(const String& s, int y) {
  spr_.setTextColor(TFT_WHITE, TFT_BLACK);
  text(s, W / 2, y, textWidth(s, 4) <= W - 8 ? 4 : 2);
}

void Display::showStatus(const String& title, const String& line1, const String& line2, uint16_t titleColor,
                         bool nameLine) {
  begin();
  spr_.fillSprite(TFT_BLACK);
  topRow(ssid_, rssi_, batteryPercent());
  spr_.setTextDatum(MC_DATUM);
  spr_.setTextColor(titleColor, TFT_BLACK);
  text(title, W / 2, 50, 4);
  if (nameLine) {
    name(line1, 81);
    spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    text(line2, W / 2, 110, 2);
  } else {
    spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    text(line1, W / 2, 80, 2);
    text(line2, W / 2, 104, 2);
  }
  push();
  setBrightness(BRIGHT_FULL);
}

// SI unit symbols with a space: "58 min", "2 h 5 min", "1 d 3 h" ("m" would be metres).
static String fmtDuration(long s) {
  if (s < 0) s = -s;
  char b[24];
  if (s < 3600) snprintf(b, sizeof b, "%ld min", s / 60);
  else if (s < 86400) snprintf(b, sizeof b, "%ld h %ld min", s / 3600, (s % 3600) / 60);
  else snprintf(b, sizeof b, "%ld d %ld h", s / 86400, (s % 86400) / 3600);
  return b;
}

void Display::showInspection(const Inspection& ins, int pendingCount, time_t now, bool offline, int batteryPct) {
  begin();
  spr_.fillSprite(TFT_BLACK);
  topRow(ssid_, offline ? 0 : rssi_, batteryPct, offline);

  const bool haveDeadline = ins.deadline > 0 && now > TIME_VALID_AFTER;
  const long left = haveDeadline ? (long)(ins.deadline - now) : 0;

  // Label row: "KONTROLLE  +1 · bis 15:30" — the date too when it's not within the next 20 h,
  // only the day if that doesn't fit.
  spr_.setTextDatum(MC_DATUM);
  String label = tr(S_INSPECTION);
  if (pendingCount > 1) label += "  +" + String(pendingCount - 1);
  if (haveDeadline) {
    struct tm lt;
    localtime_r(&ins.deadline, &lt);
    char when[24];
    strftime(when, sizeof when, left > 20 * 3600 ? tr(S_DUE_DATE_FMT) : "%H:%M", &lt);
    String full = label + tr(S_DUE_BY) + when;
    if (textWidth(full, 2) > W - 8) {
      strftime(when, sizeof when, tr(S_DUE_DAY_FMT), &lt);
      full = label + tr(S_DUE_BY) + when;
    }
    label = full;
  }
  spr_.setTextColor(COL_INSPECT, TFT_BLACK);
  text(label, W / 2, 30, 2);

  spr_.setTextColor(TFT_WHITE, TFT_BLACK);
  if (ins.code.isEmpty()) {
    text(tr(S_INSPECTION_OPEN), W / 2, 66, 4);
    spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    text(tr(S_NO_CODE_SEE_APP), W / 2, 90, 2);
  } else if (ins.code.length() <= 7) {
    text(ins.code, W / 2, 72, 7);          // 7-segment, 48 px high
  } else {
    spr_.setTextSize(2);
    text(ins.code, W / 2, 72, 40);         // 8 digits don't fit font 7
    spr_.setTextSize(1);
  }

  // Bottom, below the code (ends at about y = 98): time left, large; small if it doesn't fit.
  if (haveDeadline) {
    char line[40];
    snprintf(line, sizeof line, tr(left >= 0 ? S_TIME_LEFT_FMT : S_OVERDUE_FMT), fmtDuration(left).c_str());
    spr_.setTextColor(left < 0 ? TFT_RED : left < 1800 ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
    uint8_t font = textWidth(line, 4) > W - 8 ? 2 : 4;
    text(line, W / 2, 116, font);
  }
  push();
}

void Display::showDeviceStatus(const DeviceStatus& st) {
  begin();
  spr_.fillSprite(TFT_BLACK);

  topRow(st.ssid, st.rssi, st.battery);

  // Middle: lock state, username, inspection state.
  spr_.setTextDatum(MC_DATUM);
  if (st.locked == 1) { spr_.setTextColor(TFT_GREEN, TFT_BLACK); text(tr(S_LOCKED), W / 2, 50, 4); }
  else if (st.locked == 0) { spr_.setTextColor(COL_UNLOCKED, TFT_BLACK); text(tr(S_UNLOCKED), W / 2, 50, 4); }
  else { spr_.setTextColor(TFT_DARKGREY, TFT_BLACK); text(tr(S_STATUS_UNKNOWN), W / 2, 50, 4); }
  name(st.username, 81);
  spr_.setTextColor(TFT_GREEN, TFT_BLACK);
  text(tr(S_NO_INSPECTION), W / 2, 110, 2);

  push();
  setBrightness(BRIGHT_FULL);
}

void Display::showPortal(const String& ap, const String& apPass, bool cancelHint) {
  begin();
  spr_.fillSprite(TFT_BLACK);
  // Only power on top: the Wi-Fi below is the badge's own access point, not a joined network.
  power(batteryPercent());
  // Left-aligned, so it reads as the start of the instructions below rather than a screen title.
  spr_.setTextDatum(ML_DATUM);
  spr_.setTextColor(COL_INSPECT, TFT_BLACK);
  text(tr(S_AP_USE_PHONE), 4, 16, 4);

  // Label left (small, grey), value right of it (large, white).
  const int valueX = 110;
  const int rows[2] = {54, 88};
  spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  text(tr(S_AP_WIFI), 4, rows[0], 2);
  text(tr(S_AP_PASSWORD), 4, rows[1], 2);
  spr_.setTextColor(TFT_WHITE, TFT_BLACK);
  text(ap, valueX, rows[0], 4);
  text(apPass, valueX, rows[1], 4);

  if (cancelHint) {
    spr_.setTextDatum(BC_DATUM);
    spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    text(tr(S_AP_CANCEL), W / 2, H - 4, 2);
  }
  push();
  setBrightness(BRIGHT_FULL);
}

void Display::showPortalJoined(const String& url, bool cancelHint) {
  begin();
  spr_.fillSprite(TFT_BLACK);
  power(batteryPercent());
  spr_.setTextDatum(ML_DATUM);
  spr_.setTextColor(COL_INSPECT, TFT_BLACK);
  text(tr(S_AP_JOINED), 4, 16, 4);

  spr_.setTextDatum(MC_DATUM);
  spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  text(tr(S_AP_NO_PAGE), W / 2, 46, 2);
  spr_.setTextColor(TFT_WHITE, TFT_BLACK);
  text(url, W / 2, 74, 4);
  spr_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  text(tr(S_AP_OR_WAIT), W / 2, 102, 2);

  if (cancelHint) {
    spr_.setTextDatum(BC_DATUM);
    text(tr(S_AP_CANCEL), W / 2, H - 4, 2);
  }
  push();
  setBrightness(BRIGHT_FULL);
}

int batteryPercent() {
  pinMode(PIN_ADC_EN, OUTPUT);
  digitalWrite(PIN_ADC_EN, HIGH);
  delay(2);
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  mv = mv / 8 * 2;
  if (mv > 4350) return -1;  // USB / charging
  int pct = ((int)mv - 3300) * 100 / (4150 - 3300);
  return constrain(pct, 0, 100);
}
