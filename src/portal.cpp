// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Setup portal: Wi-Fi access point, captive DNS and the settings web page.

#include "portal.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>
#include <esp_netif.h>
#include "config.h"
#include "i18n.h"

static String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;";   // attributes are single-quoted; SSIDs come from the neighbours
    else o += c;
  }
  return o;
}

/** "https://App.Example.ch/dashboard" → "app.example.ch". */
static String normalizeHost(String h) {
  h.trim();
  h.toLowerCase();
  if (h.startsWith("https://")) h.remove(0, 8);
  else if (h.startsWith("http://")) h.remove(0, 7);
  int slash = h.indexOf('/');
  if (slash >= 0) h.remove(slash);
  return h;
}

/** The page shown after Save or reset, while the badge restarts. */
static String donePage(const char* title, const char* text) {
  return String("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
                "<body style='font-family:system-ui;background:#111;color:#eee;padding:1rem'><h3>") +
         title + "</h3><p>" + text + "</p></body>";
}

static String page(const Settings& s, const String& scanned, const String& notice, Lang L) {
  auto t = [L](Str id) { return String(tr(id, L)); };
  String h;
  h.reserve(4096);
  h += F("<!doctype html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>PrufLock</title><style>"
         "body{font-family:-apple-system,system-ui,sans-serif;max-width:28rem;margin:1rem auto;padding:0 1rem;background:#111;color:#eee}"
         "fieldset{border:1px solid #444;border-radius:8px;margin:1rem 0}"
         "label{display:block;margin:.5rem 0 .2rem;font-size:.9rem;color:#aaa}"
         "input{width:100%;box-sizing:border-box;padding:.55rem;font-size:1rem;border-radius:6px;border:1px solid #555;background:#222;color:#eee}"
         "button{width:100%;padding:.8rem;font-size:1.05rem;border:0;border-radius:8px;background:#f80;color:#000;margin-top:1rem}"
         "small{color:#888}.n{color:#fc6}"
         ".lang{display:flex;gap:.5rem;align-items:center;justify-content:flex-end;font-size:.9rem;color:#aaa}"
         ".lang a{color:#eee;text-decoration:none;padding:.3rem .7rem;border:1px solid #555;border-radius:6px}"
         ".lang a.on{background:#f80;color:#000;border-color:#f80}"
         "button.r{background:none;border:1px solid #a33;color:#f66;font-size:.9rem;padding:.5rem;margin-top:2rem}"
         "</style></head><body>");
  // Language switch: re-renders the form; the choice is stored with "Save".
  h += "<div class='lang'>" + t(S_WEB_LANGUAGE) + ":"
       "<a href='/?lang=en'" + String(L == Lang::EN ? " class='on'" : "") + ">English</a>"
       "<a href='/?lang=de'" + String(L == Lang::DE ? " class='on'" : "") + ">Deutsch</a></div>";
  h += F("<h2>PrufLock</h2>");
  if (notice.length()) h += "<p class='n'>" + esc(notice) + "</p>";
  h += "<form method='post' action='/save'><input type='hidden' name='lang' value='" + String(langCode(L)) + "'>";
  h += F("<datalist id='nets'>");
  h += scanned;
  h += F("</datalist>");
  for (int i = 0; i < Settings::NETS; i++) {
    h += "<fieldset><legend>" + t(S_WEB_WIFI) + " " + String(i + 1) +
         (i == 0 ? t(S_WEB_WIFI_HINT1) : i == 1 ? t(S_WEB_WIFI_HINT2) : String()) + "</legend>";
    h += "<label>SSID</label><input name='s" + String(i) + "' list='nets' autocapitalize='off' value='" + esc(s.ssid[i]) + "'>";
    h += "<label>" + t(S_WEB_PASSWORD) + "</label><input type='password' name='p" + String(i) + "' placeholder='" +
         (s.pass[i].length() ? t(S_WEB_UNCHANGED) : String()) + "'></fieldset>";
  }
  h += "<fieldset><legend>" + t(S_WEB_SERVER) + "</legend><input name='host' autocapitalize='off' "
       "autocorrect='off' inputmode='url' placeholder='tracker.example.com' value='" + esc(s.host) + "'><small>" + t(S_WEB_SERVER_HINT) +
       "</small></fieldset>";
  h += "<fieldset><legend>" + t(S_WEB_ACCOUNT) + "</legend><label>" + t(S_WEB_USER) +
       "</label><input name='user' autocapitalize='off' autocomplete='username' value='" + esc(s.user) + "'>";
  h += "<label>" + t(S_WEB_PASSWORD) + "</label><input type='password' name='pw' autocomplete='current-password' placeholder='" +
       (s.password.length() ? t(S_WEB_UNCHANGED) : String()) + "'></fieldset>";
  h += "<small>" + t(S_WEB_HINT) + "</small><button>" + t(S_WEB_SAVE) + "</button></form>";
  // Factory reset, only once there is something to erase. The confirm text has no quotes.
  if (s.configured())
    h += "<form method='post' action='/reset' onsubmit=\"return confirm('" + t(S_WEB_RESET_CONFIRM) +
         "')\"><input type='hidden' name='lang' value='" + String(langCode(L)) + "'><button class='r'>" +
         t(S_WEB_RESET) + "</button></form>";
  h += F("<p><small>Firmware " FW_VERSION "</small></p></body></html>");
  return h;
}

// Log timestamps are ms since the AP came up, so a slow login sheet shows where the time went.
static uint32_t apUpAt;
static volatile uint32_t phoneJoinedAt;   // 0 = no phone on the AP; set from the Wi-Fi event task
static uint32_t sinceAp() { return millis() - apUpAt; }

static void onApEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    phoneJoinedAt = millis() | 1;
    Serial.printf("[portal] +%lu station joined\n", (unsigned long)sinceAp());
  } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    phoneJoinedAt = 0;
    Serial.printf("[portal] +%lu station left, reason %u\n", (unsigned long)sinceAp(), info.wifi_ap_stadisconnected.reason);
  } else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
    Serial.printf("[portal] +%lu lease %s\n", (unsigned long)sinceAp(), IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
}

// ── captive DNS ──────────────────────────────────────────────────────────────
// Every name resolves to the badge, but only for A queries: AAAA/HTTPS/… get an empty NOERROR
// answer (the stock DNSServer answers those with an A record too, which iOS may discard).

static void dnsServe(WiFiUDP& udp, IPAddress ip) {
  int n = udp.parsePacket();
  if (n <= 0) return;
  uint8_t q[512];
  if (n > (int)sizeof q || n < 12) { udp.clear(); return; }
  udp.read(q, n);
  if ((q[2] & 0x80) || (q[2] & 0x78) || q[4] != 0 || q[5] != 1) return;   // queries, 1 question only
  int p = 12;
  char name[64];
  size_t nl = 0;
  while (p < n && q[p] != 0) {
    if (q[p] & 0xC0) return;   // no compression in a question
    for (int i = 1; i <= q[p] && p + i < n && nl < sizeof name - 2; i++) name[nl++] = (char)q[p + i];
    if (nl < sizeof name - 2) name[nl++] = '.';
    p += q[p] + 1;
  }
  name[nl] = 0;
  if (p + 5 > n) return;
  uint16_t qtype = (q[p + 1] << 8) | q[p + 2];
  int qend = p + 5;            // end of the question; drops EDNS etc.
  bool answer = qtype == 1 || qtype == 255;
  Serial.printf("[portal] +%lu dns %s type %u%s\n", (unsigned long)sinceAp(), name, qtype, answer ? "" : " (empty)");

  q[2] = 0x84 | (q[2] & 0x01);  // response, authoritative, keep RD
  q[3] = 0x80;                  // RA, NOERROR
  q[6] = 0; q[7] = answer ? 1 : 0;
  q[8] = q[9] = q[10] = q[11] = 0;
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write(q, qend);
  if (answer) {
    const uint8_t rr[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 10, 0, 4,
                          ip[0], ip[1], ip[2], ip[3]};   // A, IN, TTL 10 s, the badge's IP
    udp.write(rr, sizeof rr);
  }
  udp.endPacket();
}

// ── tiny HTTP server ─────────────────────────────────────────────────────────
// The stock WebServer serves one connection at a time and keeps an idle one for 5 s — a phone's
// speculative pre-connection then stalls its captive-portal probe until the probe times out and
// the phone decides "no internet, no login sheet". This one serves a few at once and drops idle
// connections after a second.

struct HttpReq {
  String method, path, query, host, body;
};

static String urlDecode(const String& s) {
  String o;
  o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') o += ' ';
    else if (c == '%' && i + 2 < s.length()) {
      o += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16);
      i += 2;
    } else o += c;
  }
  return o;
}

/** Value of `name` in a urlencoded "a=1&b=2" string, "" if absent. */
static String formArg(const String& params, const String& name) {
  int start = 0;
  while (start < (int)params.length()) {
    int end = params.indexOf('&', start);
    if (end < 0) end = params.length();
    int eq = params.indexOf('=', start);
    if (eq > start && eq < end && params.substring(start, eq) == name) return urlDecode(params.substring(eq + 1, end));
    start = end + 1;
  }
  return "";
}

static String reqArg(const HttpReq& r, const String& name) {
  String v = formArg(r.body, name);
  return v.length() ? v : formArg(r.query, name);
}

static void respond(WiFiClient& c, int code, const String& body, const String& location = String()) {
  String h = "HTTP/1.1 " + String(code) + (code == 200 ? " OK" : code == 302 ? " Found" : " Not Found") +
             "\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n";
  if (location.length()) h += "Location: " + location + "\r\n";
  h += "Connection: close\r\nContent-Length: " + String(body.length()) + "\r\n\r\n";
  uint32_t t0 = millis();
  size_t sent = c.write((const uint8_t*)h.c_str(), h.length());
  for (size_t off = 0; off < body.length(); off += 1024)
    sent += c.write((const uint8_t*)body.c_str() + off, min((size_t)1024, body.length() - off));
  Serial.printf("[portal] +%lu sent %u/%u bytes in %lu ms\n", (unsigned long)sinceAp(), sent,
                h.length() + body.length(), (unsigned long)(millis() - t0));
  c.stop();
}

/** Parses a complete request out of `buf`; false while it's still incomplete. */
static bool parseRequest(const String& buf, HttpReq& r) {
  int hdrEnd = buf.indexOf("\r\n\r\n");
  if (hdrEnd < 0) return false;
  int sp1 = buf.indexOf(' '), sp2 = buf.indexOf(' ', sp1 + 1);
  if (sp1 < 0 || sp2 < 0) return false;
  r.method = buf.substring(0, sp1);
  String target = buf.substring(sp1 + 1, sp2);
  int qm = target.indexOf('?');
  r.path = qm < 0 ? target : target.substring(0, qm);
  r.query = qm < 0 ? String() : target.substring(qm + 1);
  String lower = buf.substring(0, hdrEnd);
  lower.toLowerCase();
  int cl = lower.indexOf("\r\ncontent-length:");
  size_t len = cl < 0 ? 0 : (size_t)lower.substring(cl + 17).toInt();
  int ho = lower.indexOf("\r\nhost:");
  if (ho >= 0) {
    int he = lower.indexOf("\r\n", ho + 7);
    r.host = buf.substring(ho + 7, he);
    r.host.trim();
  }
  if (buf.length() < hdrEnd + 4 + len) return false;
  r.body = buf.substring(hdrEnd + 4, hdrEnd + 4 + len);
  return true;
}

struct Conn {
  WiFiClient c;
  String buf;
  uint32_t since = 0;
  bool used = false;
};

// DHCP option 114 (RFC 8910): may make iOS open the sheet sooner; the DNS + HTTP catch-all is what
// reliably brings it up. Settable only while the AP's DHCP server is stopped.
static void setCaptivePortalUri(IPAddress ip) {
  static char uri[32];   // esp_netif keeps the pointer, not a copy
  snprintf(uri, sizeof(uri), "http://%s/", ip.toString().c_str());
  esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (!nif) { Serial.println("[portal] no AP netif, option 114 not set"); return; }
  esp_err_t err = esp_netif_dhcps_stop(nif);
  if (err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    err = esp_netif_dhcps_option(nif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, uri, strlen(uri));
  esp_err_t startErr = esp_netif_dhcps_start(nif);
  if (err == ESP_OK) err = startErr;
  Serial.printf("[portal] DHCP option 114 %s: %s\n", uri, esp_err_to_name(err));
}

void runPortal(Settings& s, Display& d) {
  char apPass[9];
  for (int i = 0; i < 8; i++) apPass[i] = '0' + esp_random() % 10;
  apPass[8] = 0;
  const char* apName = "PrufLock";

  d.showStatus(tr(S_SETUP), tr(S_STARTING_SETUP));
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  String scanned;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) scanned += "<option value='" + esc(WiFi.SSID(i)) + "'>";
  WiFi.scanDelete();

  // AP only from here on: a station side that is still around (auto-reconnect from the last
  // session, background scans) takes the shared radio off the AP's channel and drops the phone's
  // DHCP/DNS/probe packets. Power save would add latency to each one.
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_AP);
  network_event_handle_t evHandle = WiFi.onEvent(onApEvent);
  WiFi.softAP(apName, apPass);
  WiFi.setSleep(false);
  delay(200);
  apUpAt = millis();
  IPAddress ip = WiFi.softAPIP();
  if (PORTAL_DHCP_114) setCaptivePortalUri(ip);
  WiFiUDP dns;
  dns.begin(53);
  WiFiServer server(80);
  server.begin();
  server.setNoDelay(true);
  Conn conns[4];
  bool saved = false;
  bool pageOpened = false;   // the phone has loaded the form from the badge itself
  Lang pageLang = s.lang;   // what the form shows; stored only on "Save"

  auto handle = [&](WiFiClient& c, const HttpReq& r) {
    Serial.printf("[portal] +%lu %s %s%s\n", (unsigned long)sinceAp(), r.method.c_str(), r.host.c_str(), r.path.c_str());
    if (r.path == "/favicon.ico") { respond(c, 404, ""); return; }
    if (r.method == "POST" && r.path == "/save") {
      s.lang = langFromCode(reqArg(r, "lang"));
      pageLang = s.lang;
      for (int i = 0; i < Settings::NETS; i++) {
        String ssid = reqArg(r, "s" + String(i));
        String pass = reqArg(r, "p" + String(i));
        ssid.trim();
        if (ssid.isEmpty()) { s.ssid[i] = ""; s.pass[i] = ""; continue; }
        if (pass.length() || ssid != s.ssid[i]) s.pass[i] = pass;
        s.ssid[i] = ssid;
      }
      String user = reqArg(r, "user");
      user.trim();
      String pw = reqArg(r, "pw");
      String host = normalizeHost(reqArg(r, "host"));
      bool accountChanged = host != s.host || user != s.user || pw.length();
      s.host = host;
      s.user = user;
      if (pw.length()) s.password = pw;
      if (accountChanged) {
        s.authBad = false;
        Settings::saveCookie("");   // force a fresh login with the new account data
        Settings::saveUsername("");
      }
      s.save();
      if (!s.configured()) {
        respond(c, 200, page(s, scanned, tr(S_WEB_MISSING, pageLang), pageLang));
        return;
      }
      respond(c, 200, donePage(tr(S_WEB_SAVED, pageLang), tr(S_WEB_SAVED_TEXT, pageLang)));
      saved = true;
      return;
    }
    if (r.method == "POST" && r.path == "/reset") {
      Lang L = langFromCode(reqArg(r, "lang"));
      Settings::erase();
      Serial.println("[portal] factory reset");
      respond(c, 200, donePage(tr(S_WEB_RESET_DONE, L), tr(S_WEB_RESET_DONE_TEXT, L)));
      saved = true;   // restart: the badge comes back unconfigured, in setup
      return;
    }
    // Every captive-portal probe (Apple's hotspot-detect.html, Android's generate_204, Windows'
    // connecttest.txt …) asks for another host: redirect it to the badge. A redirect is the
    // clearest "login required" signal; answering the probe with the form itself makes iOS
    // hold the sheet back until the Wi-Fi settings are reopened.
    String host = r.host;
    int colon = host.indexOf(':');
    if (colon >= 0) host.remove(colon);
    if (host.length() && host != ip.toString()) {
      String url = "http://" + ip.toString() + "/";
      respond(c, 302, "<a href='" + url + "'>PrufLock</a>", url);
      return;
    }
    // "/" and the language switch: the form.
    pageOpened = true;
    if (reqArg(r, "lang").length()) pageLang = langFromCode(reqArg(r, "lang"));
    respond(c, 200, page(s, scanned, "", pageLang));
  };

  auto serveHttp = [&]() {
    for (WiFiClient nc = server.accept(); nc; nc = server.accept()) {
      Conn* slot = nullptr;
      for (Conn& k : conns) if (!k.used) { slot = &k; break; }
      if (!slot) {   // all busy: the oldest one goes
        slot = &conns[0];
        for (Conn& k : conns) if ((int32_t)(k.since - slot->since) < 0) slot = &k;
        slot->c.stop();
      }
      Serial.printf("[portal] +%lu tcp from %s:%u\n", (unsigned long)sinceAp(), nc.remoteIP().toString().c_str(), nc.remotePort());
      slot->c = nc;
      slot->buf = "";
      slot->since = millis();
      slot->used = true;
    }
    for (Conn& k : conns) {
      if (!k.used) continue;
      while (k.c.available() && k.buf.length() < 4096) k.buf += (char)k.c.read();
      HttpReq r;
      if (parseRequest(k.buf, r)) {
        handle(k.c, r);
        k.used = false;
      } else if (!k.c.connected() || k.buf.length() >= 4096 ||
                 millis() - k.since > (k.buf.isEmpty() ? 1000u : 5000u)) {
        Serial.printf("[portal] +%lu dropped connection after %lu ms, %u bytes\n", (unsigned long)sinceAp(),
                      (unsigned long)(millis() - k.since), k.buf.length());
        k.c.stop();   // idle pre-connection, gone, or junk
        k.used = false;
      }
    }
  };

  // Once set up, A or B cancels setup. On a fresh device it is mandatory (a restart would land
  // right back here), so the buttons are ignored there.
  const bool cancelable = s.configured();
  d.showPortal(apName, apPass, cancelable);
  Serial.printf("[portal] AP %s up at %s\n", apName, ip.toString().c_str());

  uint32_t started = millis();
  uint32_t savedAt = 0;
  bool armed = false;   // B is usually still held from opening setup: arm once both are up
  bool joinedShown = false;
  for (;;) {
    dnsServe(dns, ip);
    serveHttp();
    // iOS often shows its login sheet only ~45 s after joining, whatever the badge answers: once a
    // phone has been on the AP for a few seconds without opening the page, say how to open it.
    uint32_t joined = phoneJoinedAt;
    bool wantJoined = joined && !pageOpened && millis() - joined > PORTAL_HINT_AFTER_MS;
    if (wantJoined != joinedShown) {
      if (wantJoined) d.showPortalJoined(ip.toString(), cancelable);
      else d.showPortal(apName, apPass, cancelable);
      joinedShown = wantJoined;
    }
    bool down = digitalRead(PIN_BTN_A) == LOW || digitalRead(PIN_BTN_B) == LOW;
    if (!down) armed = true;
    else if (armed && cancelable && !saved) {
      Serial.println("[portal] cancelled by button");
      break;
    }
    if (saved && !savedAt) savedAt = millis();
    if (savedAt && millis() - savedAt > 1500) break;
    if (millis() - started > PORTAL_TIMEOUT_MS) break;
    delay(2);
  }
  WiFi.removeEvent(evHandle);
  d.showStatus(tr(S_RESTARTING, s.lang), "");
  // A is GPIO0 (BOOT strap): restarting while it is held would enter the ROM download mode.
  while (digitalRead(PIN_BTN_A) == LOW || digitalRead(PIN_BTN_B) == LOW) delay(10);
  delay(500);
  ESP.restart();
  for (;;) {}
}
