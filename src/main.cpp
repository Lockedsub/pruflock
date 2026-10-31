// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Main program: wake, check for inspections, show the code, sleep.

// Idle: deep sleep, wake every POLL_IDLE_S, join Wi-Fi, ask /api/heartbeat whether an inspection
// is pending, sleep again (display stays dark). Pending: fetch its code and deadline from
// /api/kontrollen/open, show them, stay awake and re-check every POLL_ACTIVE_MS until the photo
// has been submitted (the inspection's id leaves the heartbeat), then go dark again.
//
// Buttons: A (upper) or B (lower) — wake, check now; with nothing pending a status screen
//          (Wi-Fi, signal, username, locked) shows until the next press or STATUS_SCREEN_MS.
//          Hold B for PORTAL_HOLD_MS (on wake or while lit) — setup portal.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <time.h>
#include "config.h"
#include "settings.h"
#include "display.h"
#include "tracker.h"
#include "portal.h"
#include "i18n.h"
#include "vibra.h"
#include "tz.h"

// ── State kept across deep sleep ─────────────────────────────────────────────
static constexpr uint32_t RTC_MAGIC = 0x504C434B;  // "PLCK"
RTC_DATA_ATTR static uint32_t rtcMagic;
RTC_DATA_ATTR static char rtcCookie[2048];
RTC_DATA_ATTR static time_t rtcCookieSavedAt;
RTC_DATA_ATTR static time_t rtcLastNtp;
RTC_DATA_ATTR static time_t rtcLoginRetryAt;
RTC_DATA_ATTR static uint32_t rtcNoWifiSleep;
RTC_DATA_ATTR static int8_t rtcNet = -1;           // index of the last network that worked
RTC_DATA_ATTR static uint8_t rtcBssid[6];
RTC_DATA_ATTR static int32_t rtcChannel;
RTC_DATA_ATTR static char rtcUsername[64];          // real username from /api/auth/session
RTC_DATA_ATTR static char rtcTimezone[48];          // the account's IANA zone, from /api/auth/session

static Settings settings;
static Display display;
static TrackerClient client;
static bool interactive = false;  // a human just pressed a button / powered on → show status

// ── helpers ──────────────────────────────────────────────────────────────────

static time_t now() { return time(nullptr); }

/** Username for display: the app's real one once known, else what was entered at setup. */
static String displayName() { return rtcUsername[0] ? String(rtcUsername) : settings.user; }
static bool timeValid() { return now() > TIME_VALID_AFTER; }

/** POSIX rule for the account's zone; UTC until the app has told us (only "due 15:30" needs it,
 *  the countdown is computed in UTC). */
static const char* posixTz() {
  const char* tz = tzPosix(rtcTimezone);
  return tz ? tz : "UTC0";
}

static void applyTz() {
  setenv("TZ", posixTz(), 1);
  tzset();
}

/** Caches what /api/auth/session reported: the username for the screens, the zone for the clock. */
static void rememberSession(const SessionInfo& s) {
  if (s.name.length() && s.name != rtcUsername) {
    strlcpy(rtcUsername, s.name.c_str(), sizeof rtcUsername);
    Settings::saveUsername(s.name);
  }
  if (s.timezone.length() && s.timezone != rtcTimezone) {
    if (!tzPosix(s.timezone.c_str())) Serial.printf("[tz] unknown zone %s, using UTC\n", s.timezone.c_str());
    strlcpy(rtcTimezone, s.timezone.c_str(), sizeof rtcTimezone);
    Settings::saveTimezone(s.timezone);
    applyTz();
    Serial.printf("[tz] %s = %s\n", rtcTimezone, posixTz());
  }
}

/** Asks /api/auth/session for the username and zone, and caches them. */
static TrackerResult refreshSession() {
  SessionInfo si;
  TrackerResult r = client.session(si);
  if (r == TrackerResult::Ok) rememberSession(si);
  return r;
}

static bool pressed(int pin) { return digitalRead(pin) == LOW; }

[[noreturn]] static void sleepFor(uint32_t seconds) {
  Serial.printf("[sleep] %lu s\n", (unsigned long)seconds);
  vibraSleep();
  display.off();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  digitalWrite(PIN_ADC_EN, LOW);
  // A held button would wake us again at once.
  while (pressed(PIN_BTN_A) || pressed(PIN_BTN_B)) delay(10);
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BTN_A);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_A, 0);
  // B: ext1 with a single-pin mask — ALL_LOW then simply means "B pressed". GPIO35 has an
  // external pull-up on the board (it has no internal one).
  esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_B, ESP_EXT1_WAKEUP_ALL_LOW);
  Serial.flush();
  esp_deep_sleep_start();
  for (;;) {}
}

static void status(const String& title, const String& l1 = "", const String& l2 = "", uint16_t c = TFT_WHITE,
                   bool nameLine = false) {
  Serial.printf("[status] %s | %s | %s\n", title.c_str(), l1.c_str(), l2.c_str());
  if (interactive) display.showStatus(title, l1, l2, c, nameLine);
}

// ── Wi-Fi ────────────────────────────────────────────────────────────────────

static uint32_t wifiT0;                  // start of the current attempt, for the log
static volatile bool wifiAssociated;     // associated, waiting for DHCP (slow on an iPhone)
static volatile uint8_t wifiAuthFails;   // wrong-password-like disconnects in this attempt

static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  unsigned long ms = millis() - wifiT0;
  if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    wifiAssociated = true;
    Serial.printf("[wifi] associated after %lu ms\n", ms);
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("[wifi] got IP after %lu ms\n", ms);
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP6) {
    Serial.printf("[wifi] got IPv6 %s after %lu ms\n", IPAddress(IPv6, (const uint8_t*)info.got_ip6.ip6_info.ip.addr).toString().c_str(), ms);
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiAssociated = false;
    uint8_t r = info.wifi_sta_disconnected.reason;
    Serial.printf("[wifi] disconnect %u %s after %lu ms\n", r, WiFi.disconnectReasonName((wifi_err_reason_t)r), ms);
    if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) wifiAuthFails = wifiAuthFails + 1;
  }
}

/** One attempt at network `net`, waiting up to `ms` for an IP — or up to WIFI_TIMEOUT_MS once
 *  associated, so DHCP can finish. Without `bssid` the core sends probe requests naming the SSID
 *  and, through auto-reconnect, repeats them after every NO_AP_FOUND for as long as we wait. */
static bool attempt(int net, uint32_t ms, int32_t channel = 0, const uint8_t* bssid = nullptr) {
  WiFi.disconnect();
  display.setNetwork(settings.ssid[net], 0);
  status(tr(S_CONNECTING));
  Serial.printf("[wifi] trying %s%s, %lu ms\n", settings.ssid[net].c_str(), bssid ? " (last AP)" : "", (unsigned long)ms);
  wifiAssociated = false;
  wifiAuthFails = 0;
  wifiT0 = millis();
  WiFi.begin(settings.ssid[net].c_str(), settings.pass[net].c_str(), channel, bssid);
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) return true;
    uint32_t limit = wifiAssociated ? max<uint32_t>(ms, WIFI_TIMEOUT_MS) : ms;
    if (millis() - wifiT0 >= limit) break;
    if (wifiAuthFails >= 2) { Serial.println("[wifi] wrong password?"); break; }
    delay(50);
  }
  WiFi.disconnect();
  return false;
}

static bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  static bool hooked = false;
  if (!hooked) { WiFi.onEvent(onWifiEvent); hooked = true; }
  WiFi.mode(WIFI_STA);
  WiFi.enableIPv6();   // link-local + SLAAC, for servers that are IPv6-only
  WiFi.setAutoReconnect(true);
  bool ok = false;

  // 1. Fast path: the network/AP/channel that worked last time (skips the scan). Not after a
  //    wake without Wi-Fi: we have probably moved.
  if (!rtcNoWifiSleep && rtcNet >= 0 && rtcNet < Settings::NETS && settings.ssid[rtcNet].length())
    ok = attempt(rtcNet, WIFI_FAST_MS, rtcChannel, rtcBssid);

  // 2. Scan: the strongest configured network that is beaconing.
  bool seen[Settings::NETS] = {};
  if (!ok) {
    display.setNetwork("", 0);
    status(tr(S_CONNECTING), tr(S_SCANNING));
    int n = WiFi.scanNetworks();
    int best = -1;
    int32_t bestRssi = INT32_MIN, bestCh = 0;
    uint8_t bestBssid[6];
    for (int i = 0; i < n; i++) {
      for (int k = 0; k < Settings::NETS; k++) {
        if (!settings.ssid[k].length() || WiFi.SSID(i) != settings.ssid[k]) continue;
        seen[k] = true;
        if (WiFi.RSSI(i) > bestRssi) {
          best = k; bestRssi = WiFi.RSSI(i); bestCh = WiFi.channel(i);
          memcpy(bestBssid, WiFi.BSSID(i), 6);
        }
      }
    }
    WiFi.scanDelete();
    Serial.printf("[wifi] scan: %d networks, best known: %s\n", n, best >= 0 ? settings.ssid[best].c_str() : "none");
    if (best >= 0) ok = attempt(best, WIFI_TIMEOUT_MS, bestCh, bestBssid);
  }

  // 3. Wake by name: an idle iPhone hotspot stops beaconing and comes back only for probe
  //    requests that name it. Take turns between the networks the scan didn't see (a slice
  //    each, so an unseen home network can't use up the budget), until the budget is used.
  //    Once woken, the hotspot stays up for a minute or so, so its next slice finds it.
  int unseen[Settings::NETS], nUnseen = 0;
  for (int k = 0; k < Settings::NETS; k++) if (settings.ssid[k].length() && !seen[k]) unseen[nUnseen++] = k;
  uint32_t budget = interactive ? WIFI_WAKE_INTERACTIVE_MS : WIFI_WAKE_MS;
  uint32_t slice = nUnseen == 1 ? budget : WIFI_WAKE_SLICE_MS;
  uint32_t t0 = millis();
  for (int i = 0; !ok && nUnseen && millis() - t0 < budget; i = (i + 1) % nUnseen)
    ok = attempt(unseen[i], min<uint32_t>(slice, budget - (millis() - t0)));
  if (!ok) return false;

  String ssid = WiFi.SSID();
  for (int i = 0; i < Settings::NETS; i++) if (settings.ssid[i] == ssid) rtcNet = i;
  memcpy(rtcBssid, WiFi.BSSID(), 6);
  rtcChannel = WiFi.channel();
  return true;
}

static void syncTimeIfNeeded() {
  if (timeValid() && now() - rtcLastNtp < (time_t)NTP_RESYNC_S) return;
  configTzTime(posixTz(), "pool.ntp.org", "time.google.com");
  uint32_t t0 = millis();
  while (!timeValid() && millis() - t0 < 5000) delay(100);
  if (timeValid()) rtcLastNtp = now();
}

/** Keeps the RTC copy of the session cookie current; writes NVS only now and then. */
static void persistCookie(bool force) {
  String c = client.jar.sessionCookies();
  bool fitsRtc = c.length() < sizeof(rtcCookie);
  if (fitsRtc) strlcpy(rtcCookie, c.c_str(), sizeof(rtcCookie));
  else rtcCookie[0] = 0;  // too long: setup() then reloads it from NVS on every wake
  if (force || (!fitsRtc && c != Settings::loadCookie()) ||
      (timeValid() && now() - rtcCookieSavedAt > (time_t)COOKIE_NVS_SAVE_S)) {
    Settings::saveCookie(c);
    rtcCookieSavedAt = timeValid() ? now() : 0;
    Serial.println("[cookie] saved to NVS");
  }
}

static bool loginRejected;   // this wake: the server already rejected the password once

static TrackerResult doLogin() {
  // A known-bad password is retried only when a person woke the badge, and once per wake:
  // every failed login counts towards the app's account lockout.
  if (settings.authBad && (!interactive || loginRejected)) return TrackerResult::BadCredentials;
  if (!interactive && rtcLoginRetryAt && timeValid() && now() < rtcLoginRetryAt) return TrackerResult::RateLimited;
  status(tr(S_SIGNING_IN), displayName(), "", TFT_WHITE, true);
  TrackerResult r = client.login(settings.user, settings.password);
  Serial.printf("[login] %s\n", resultName(r));
  if (r == TrackerResult::Ok) {
    settings.setAuthBad(false);
    rtcLoginRetryAt = 0;
    persistCookie(true);
    refreshSession();
  } else if (r == TrackerResult::BadCredentials) {
    settings.setAuthBad(true);
    loginRejected = true;
  } else {
    rtcLoginRetryAt = (timeValid() ? now() : 0) + LOGIN_RETRY_S;
  }
  return r;
}

/** Heartbeat, logging in again once if the session has expired. */
static TrackerResult poll(String& ids, int* locked = nullptr) {
  TrackerResult r = client.heartbeat(ids, locked);
  if (r == TrackerResult::NeedLogin) {
    r = doLogin();
    if (r == TrackerResult::Ok) r = client.heartbeat(ids, locked);
  }
  if (r == TrackerResult::Ok) persistCookie(false);
  Serial.printf("[poll] %s, pending: \"%s\", locked %d\n", resultName(r), ids.c_str(), locked ? *locked : -1);
  return r;
}

static TrackerResult fetch(Inspection& ins) {
  TrackerResult r = client.fetchInspection(ins);
  if (r == TrackerResult::NeedLogin && doLogin() == TrackerResult::Ok) r = client.fetchInspection(ins);
  return r;
}

static bool containsId(const String& list, const String& id) {
  return id.length() && ("," + list + ",").indexOf("," + id + ",") >= 0;
}

/** Does `ids` contain an inspection that `seen` doesn't (i.e. one not announced yet)? */
static bool hasNewId(const String& ids, const String& seen) {
  int start = 0;
  while (start < (int)ids.length()) {
    int end = ids.indexOf(',', start);
    if (end < 0) end = ids.length();
    if (!containsId(seen, ids.substring(start, end))) return true;
    start = end + 1;
  }
  return false;
}

static int countIds(const String& ids) {
  if (ids.isEmpty()) return 0;
  int n = 1;
  for (size_t i = 0; i < ids.length(); i++) n += ids[i] == ',';
  return n;
}

/** Shown even on a timer wake (the wearer has to act), for 10 s. */
static void showLoginError(TrackerResult r) {
  Serial.printf("[status] login error: %s\n", resultName(r));
  if (r == TrackerResult::BadCredentials)
    display.showStatus(tr(S_LOGIN_REJECTED), tr(S_CHECK_PASSWORD), tr(S_HOLD_B_SETUP), TFT_RED);
  else
    display.showStatus(tr(S_LOGIN_BLOCKED), tr(S_TOO_MANY), tr(S_RETRY_IN) + String(LOGIN_RETRY_S / 60) + " min", TFT_RED);
  delay(10000);
}

/** B is down right now (e.g. it woke us): opens the portal if it stays down for PORTAL_HOLD_MS,
 *  otherwise returns once it is released. */
static void checkPortalHold() {
  if (!pressed(PIN_BTN_B)) return;
  uint32_t t0 = millis();
  bool hinted = false;
  while (pressed(PIN_BTN_B)) {
    if (!hinted && millis() - t0 > 600) {   // only a deliberate hold gets the hint
      display.showStatus(tr(S_SETUP_Q), tr(S_KEEP_HOLDING_B));
      hinted = true;
    }
    if (millis() - t0 >= PORTAL_HOLD_MS) runPortal(settings, display);
    delay(10);
  }
}

// ── status screen after a button wake, nothing pending ───────────────────────

[[noreturn]] static void statusLoop(int locked) {
  DeviceStatus st;
  st.locked = locked;
  TrackerResult nr = refreshSession();
  st.username = displayName();
  Serial.printf("[status] session %s, user %s, locked %d\n", resultName(nr), st.username.c_str(), locked);

  // The press that woke us (or a still-held one) must not end the screen right away.
  while (pressed(PIN_BTN_A) || pressed(PIN_BTN_B)) delay(10);

  const uint32_t until = millis() + STATUS_SCREEN_MS;
  uint32_t nextDraw = 0;
  bool lastA = false, lastB = false;
  uint32_t bDown = 0;
  while ((int32_t)(until - millis()) > 0) {
    bool a = pressed(PIN_BTN_A), b = pressed(PIN_BTN_B);
    if (a && !lastA) break;                               // A: done → sleep
    if (b && !lastB) bDown = millis();
    if (b && bDown && millis() - bDown >= PORTAL_HOLD_MS) runPortal(settings, display);
    if (!b && lastB) break;                               // B released before the hold → sleep
    lastA = a; lastB = b;

    if ((int32_t)(millis() - nextDraw) >= 0) {
      st.ssid = WiFi.SSID();
      st.rssi = WiFi.RSSI();
      st.battery = batteryPercent();
      display.showDeviceStatus(st);
      nextDraw = millis() + 5000;
    }
    delay(20);
  }
  sleepFor(POLL_IDLE_S);
}

// ── awake while an inspection is pending ─────────────────────────────────────

[[noreturn]] static void activeLoop(String ids) {
  Inspection ins;
  TrackerResult fr = fetch(ins);
  Serial.printf("[active] fetch %s, code %s\n", resultName(fr), ins.code.c_str());
  WiFi.setSleep(true);  // modem sleep between polls

  uint32_t brightUntil = millis() + DIM_AFTER_MS;
  uint32_t nextPoll = millis() + POLL_ACTIVE_MS;
  uint32_t nextDraw = 0;
  uint32_t bDown = 0;
  bool offline = fr != TrackerResult::Ok;
  bool loginErrorShown = false;
  bool lastA = false, lastB = false;
  int battery = batteryPercent();

  // Vibration alarm: announce every new inspection id once, then remind every VIBRA_REMIND_MS
  // until a button is pressed (or everything is done, which leaves this loop).
  String alerted;            // ids already announced
  bool alertNow = true;      // the inspection(s) we were woken for are new
  bool unacked = false;
  uint32_t nextRemind = 0;

  for (;;) {
    bool a = pressed(PIN_BTN_A);
    if (a && !lastA) {                           // A: full brightness again + poll now + ack
      brightUntil = millis() + DIM_AFTER_MS;
      nextPoll = millis();
      unacked = false;
    }
    lastA = a;
    bool b = pressed(PIN_BTN_B);
    if (b) {
      if (!bDown) bDown = millis();
      if (millis() - bDown >= PORTAL_HOLD_MS) runPortal(settings, display);
    } else bDown = 0;
    if (b && !lastB) {                           // B: full brightness + ack too
      brightUntil = millis() + DIM_AFTER_MS;
      unacked = false;
    }
    lastB = b;

    display.setBrightness((int32_t)(brightUntil - millis()) > 0 ? BRIGHT_FULL : BRIGHT_DIM);

    if ((int32_t)(millis() - nextPoll) >= 0) {
      String newIds;
      TrackerResult r = connectWifi() ? poll(newIds) : TrackerResult::NetError;
      offline = r != TrackerResult::Ok;
      if (r == TrackerResult::Ok) {
        if (newIds.isEmpty()) {
          display.showStatus(tr(S_DONE), tr(S_NO_LONGER_PENDING), "", TFT_GREEN);
          delay(3000);
          sleepFor(POLL_IDLE_S);
        }
        if (hasNewId(newIds, alerted)) alertNow = true;
        if (newIds != ids || (ins.code.isEmpty() && fr != TrackerResult::Ok)) {
          bool isNew = newIds != ids;
          ids = newIds;
          fr = fetch(ins);
          offline = fr != TrackerResult::Ok;
          if (isNew) brightUntil = millis() + DIM_AFTER_MS;
        }
      } else if (r == TrackerResult::BadCredentials && !loginErrorShown) {
        showLoginError(r);   // once; after that the code screen shows "offline"
        loginErrorShown = true;
      }
      battery = batteryPercent();
      nextPoll = millis() + POLL_ACTIVE_MS;
      nextDraw = 0;
    }

    if ((int32_t)(millis() - nextDraw) >= 0) {
      if (WiFi.status() == WL_CONNECTED) display.setNetwork(WiFi.SSID(), WiFi.RSSI());
      display.showInspection(ins, ins.count > 0 ? ins.count : countIds(ids), now(), offline, battery);
      nextDraw = millis() + 5000;
    }

    // After drawing, so the code is already on screen when the wearer pulls the badge out.
    if (alertNow || (unacked && (int32_t)(millis() - nextRemind) >= 0)) {
      if (alertNow) {
        alertNow = false;
        alerted = ids;   // all pending ones, the new one included
        unacked = true;
      }
      brightUntil = millis() + DIM_AFTER_MS;
      display.setBrightness(BRIGHT_FULL);
      vibrate(VIBRA_PULSES);
      nextRemind = millis() + VIBRA_REMIND_MS;
    }
    delay(30);
  }
}

// ── boot / wake ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT);       // external pull-up on the board
  pinMode(PIN_ADC_EN, OUTPUT);
  gpio_deep_sleep_hold_dis();
  vibraInit();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  // Only a deep-sleep wake keeps the RTC state. After a restart (e.g. from the setup portal,
  // which may have changed the server or account) the cookie must come fresh from NVS.
  bool coldBoot = rtcMagic != RTC_MAGIC || esp_reset_reason() != ESP_RST_DEEPSLEEP;
  interactive = coldBoot || cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1;
  Serial.printf("\n[boot] pruflock " FW_VERSION ", %s, wake cause %d\n", coldBoot ? "cold" : "warm", (int)cause);

  if (coldBoot) {
    vibrate(1, 150);   // wiring check: one short buzz on power-up / reset
    memset(rtcCookie, 0, sizeof rtcCookie);
    rtcCookieSavedAt = rtcLastNtp = rtcLoginRetryAt = 0;
    rtcNoWifiSleep = 0;
    rtcNet = -1;
    rtcMagic = RTC_MAGIC;
    strlcpy(rtcUsername, Settings::loadUsername().c_str(), sizeof rtcUsername);
    strlcpy(rtcTimezone, Settings::loadTimezone().c_str(), sizeof rtcTimezone);
  }
  applyTz();

  settings.load();
  gLang = settings.lang;
  httpSetHost(settings.host);
  if (!settings.configured()) runPortal(settings, display);
  checkPortalHold();

  if (coldBoot || !rtcCookie[0]) {
    String c = Settings::loadCookie();
    strlcpy(rtcCookie, c.c_str(), sizeof rtcCookie);
    rtcCookieSavedAt = 0;
  }
  client.jar.loadSessionCookies(rtcCookie);

  if (!connectWifi()) {
    rtcNoWifiSleep = rtcNoWifiSleep ? min(rtcNoWifiSleep * 2, NO_WIFI_SLEEP_MAX_S) : NO_WIFI_SLEEP_S;
    status(tr(S_NO_WIFI), tr(S_NEXT_TRY_IN) + String(rtcNoWifiSleep / 60) + " min", tr(S_HOTSPOT_HINT), TFT_RED);
    if (interactive) delay(4000);
    sleepFor(rtcNoWifiSleep);
  }
  rtcNoWifiSleep = 0;
  Serial.printf("[wifi] %s, RSSI %d\n", WiFi.SSID().c_str(), WiFi.RSSI());
  syncTimeIfNeeded();

  display.setNetwork(WiFi.SSID(), WiFi.RSSI());
  status(tr(S_CHECKING), displayName(), "", TFT_WHITE, true);
  String ids;
  int locked = -1;
  TrackerResult r = poll(ids, &locked);
  if (r == TrackerResult::Ok && !rtcTimezone[0]) refreshSession();   // zone not known yet

  if (r == TrackerResult::Ok && ids.length()) activeLoop(ids);

  if (r == TrackerResult::Ok) {
    if (interactive) statusLoop(locked);
    sleepFor(POLL_IDLE_S);
  }
  if (r == TrackerResult::BadCredentials) {   // nag every AUTH_BAD_SLEEP_S until setup fixes it
    showLoginError(r);
    sleepFor(AUTH_BAD_SLEEP_S);
  }
  if (r == TrackerResult::RateLimited) {      // quietly wait for the retry time, unless asked
    if (interactive) showLoginError(r);
    time_t wait = timeValid() && rtcLoginRetryAt > now() ? rtcLoginRetryAt - now() : 0;
    sleepFor(max<uint32_t>(POLL_IDLE_S, wait));
  }
  status(tr(S_SERVER_ERROR), resultName(r), "", TFT_RED);
  if (interactive) delay(4000);
  sleepFor(POLL_IDLE_S);
}

void loop() {}
