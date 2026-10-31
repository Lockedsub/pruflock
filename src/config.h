// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Build-time configuration: version, timings, pins.

#pragma once
#include <Arduino.h>

// Release builds get the version from the git tag (-DFW_VERSION, .github/workflows/build.yml).
#ifndef FW_VERSION
#define FW_VERSION "1.0.0"
#endif

// ── Timing ───────────────────────────────────────────────────────────────────
constexpr uint32_t POLL_IDLE_S        = 60;            // deep sleep between polls, nothing pending
constexpr uint32_t POLL_ACTIVE_MS     = 30 * 1000;     // heartbeat while a code is on screen
constexpr uint32_t NO_WIFI_SLEEP_S    = 60;            // first back-off without Wi-Fi ...
constexpr uint32_t NO_WIFI_SLEEP_MAX_S = 10 * 60;      // ... doubling up to this
constexpr uint32_t LOGIN_RETRY_S      = 30 * 60;       // after a transient login failure
constexpr uint32_t AUTH_BAD_SLEEP_S   = 15 * 60;       // wrong password: only nag occasionally
constexpr uint32_t STATUS_SCREEN_MS   = 30 * 1000;     // status screen after a button wake
constexpr uint32_t PORTAL_HOLD_MS     = 3000;          // hold B this long → setup portal
constexpr uint32_t DIM_AFTER_MS       = 5 * 60 * 1000; // full brightness, then dimmed
constexpr uint32_t NTP_RESYNC_S       = 6 * 3600;
constexpr uint32_t COOKIE_NVS_SAVE_S  = 12 * 3600;     // limit flash writes for the rolling JWT
constexpr uint32_t WIFI_FAST_MS       = 5 * 1000;      // last network/AP; longer once associated
constexpr uint32_t WIFI_TIMEOUT_MS    = 15 * 1000;     // a network seen in the scan; also DHCP wait
constexpr uint32_t WIFI_WAKE_MS       = 20 * 1000;     // probing unseen networks by name (hotspot)
constexpr uint32_t WIFI_WAKE_INTERACTIVE_MS = 45 * 1000;  // ... after a button press
constexpr uint32_t WIFI_WAKE_SLICE_MS = 6 * 1000;      // per turn when several are unseen
constexpr uint32_t IPV6_WAIT_MS       = 5 * 1000;      // SLAAC address, only once a connect has failed
constexpr uint32_t HTTP_TIMEOUT_MS    = 45 * 1000;
constexpr uint32_t PORTAL_TIMEOUT_MS  = 10 * 60 * 1000;
constexpr uint32_t PORTAL_HINT_AFTER_MS = 5 * 1000;   // phone joined, page not opened → show how to open it
constexpr bool     PORTAL_DHCP_114    = true;    // DHCP option 114 (RFC 8910); see setCaptivePortalUri()

constexpr time_t TIME_VALID_AFTER = 1700000000;   // clock set by NTP (Nov 2023), not 1970

// ── TTGO T-Display pins ──────────────────────────────────────────────────────
constexpr int PIN_BTN_A   = 0;    // upper button (BOOT); also deep-sleep wake-up
constexpr int PIN_BTN_B   = 35;   // lower button
constexpr int PIN_BL      = 4;    // backlight
constexpr int PIN_BAT_ADC = 34;   // battery voltage / 2
constexpr int PIN_ADC_EN  = 14;   // enables the battery divider

constexpr int PIN_VIBRA   = 27;   // vibration motor driver (NPN base via 1 kΩ), HIGH = on

// ── Vibration alarm ──────────────────────────────────────────────────────────
constexpr uint32_t VIBRA_REMIND_MS = 5 * 60 * 1000;  // repeat until a button is pressed
constexpr int      VIBRA_PULSES    = 3;
constexpr uint32_t VIBRA_ON_MS     = 400;
constexpr uint32_t VIBRA_OFF_MS    = 250;

constexpr uint8_t BRIGHT_FULL = 255;
constexpr uint8_t BRIGHT_DIM  = 40;
