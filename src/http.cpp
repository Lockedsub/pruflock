// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Minimal HTTPS client with a cookie jar for the ChastityTracker server.

#include "http.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

// ── CookieJar ────────────────────────────────────────────────────────────────

void CookieJar::set(const String& name, const String& value) {
  for (int i = 0; i < n_; i++) {
    if (names_[i] == name) { values_[i] = value; return; }
  }
  if (n_ < MAX) { names_[n_] = name; values_[n_] = value; n_++; }
}

void CookieJar::remove(const String& name) {
  for (int i = 0; i < n_; i++) {
    if (names_[i] == name) {
      for (int j = i; j < n_ - 1; j++) { names_[j] = names_[j + 1]; values_[j] = values_[j + 1]; }
      n_--;
      return;
    }
  }
}

bool CookieJar::has(const String& prefix) const {
  for (int i = 0; i < n_; i++) if (names_[i].startsWith(prefix)) return true;
  return false;
}

String CookieJar::join(const char* prefix) const {
  String h;
  for (int i = 0; i < n_; i++) {
    if (!names_[i].startsWith(prefix)) continue;
    if (h.length()) h += "; ";
    h += names_[i] + "=" + values_[i];
  }
  return h;
}

void CookieJar::loadSessionCookies(const String& s) {
  int start = 0;
  while (start < (int)s.length()) {
    int end = s.indexOf("; ", start);
    if (end < 0) end = s.length();
    String pair = s.substring(start, end);
    int eq = pair.indexOf('=');
    if (eq > 0) set(pair.substring(0, eq), pair.substring(eq + 1));
    start = end + 2;
  }
}

void CookieJar::applySetCookie(const String& v) {
  int eq = v.indexOf('=');
  if (eq <= 0) return;
  int semi = v.indexOf(';');
  String name = v.substring(0, eq);
  String value = v.substring(eq + 1, semi < 0 ? v.length() : semi);
  String attrs = semi < 0 ? String() : v.substring(semi);
  attrs.toLowerCase();
  bool expired = value.isEmpty() || attrs.indexOf("max-age=0") >= 0 || attrs.indexOf("1970") >= 0;
  if (expired) remove(name);
  else set(name, value);
}

// ── HTTP ─────────────────────────────────────────────────────────────────────

String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

// Reads up to n bytes; 0 = EOF, -1 = timeout.
static int readSome(WiFiClientSecure& c, uint8_t* buf, size_t n, uint32_t deadline) {
  while ((int32_t)(deadline - millis()) > 0) {
    int a = c.available();
    if (a > 0) return c.read(buf, min((size_t)a, n));
    if (!c.connected()) return 0;
    delay(5);
  }
  return -1;
}

static bool readLine(WiFiClientSecure& c, String& line, uint32_t deadline) {
  line = "";
  while ((int32_t)(deadline - millis()) > 0) {
    int a = c.available();
    if (a > 0) {
      char ch = c.read();
      if (ch == '\n') { line.trim(); return true; }
      if (line.length() < 8192) line += ch;
      continue;
    }
    if (!c.connected()) return line.length() > 0;
    delay(5);
  }
  return false;
}

// Feeds exactly n bytes (or until EOF if n < 0) into the sink. False = error; a sink that asks
// to stop sets `stopped` (and returns true).
static bool pump(WiFiClientSecure& c, long n, const BodySink& sink, uint32_t deadline, bool& stopped) {
  uint8_t buf[1024];
  while (n != 0) {
    size_t want = n < 0 ? sizeof(buf) : min((long)sizeof(buf), n);
    int got = readSome(c, buf, want, deadline);
    if (got == 0) return n < 0;   // EOF is fine only without a known length
    if (got < 0) return false;
    if (n > 0) n -= got;
    if (sink && !sink(buf, got)) { stopped = true; return true; }
  }
  return true;
}

static String gHost;
static String gOrigin;

void httpSetHost(const String& host) {
  gHost = host;
  gOrigin = "https://" + host;
}

const String& httpOrigin() { return gOrigin; }

int httpsRequest(const char* method, const String& path, CookieJar& jar,
                 const char* contentType, const String& body,
                 HttpResponse& resp, const BodySink& sink) {
  resp = HttpResponse();
  const uint32_t deadline = millis() + HTTP_TIMEOUT_MS;

  WiFiClientSecure client;
  client.useBuiltinCACertBundle();  // Mozilla's root CAs, compiled into the Arduino-ESP32 core
  client.setTimeout(15000);         // ms
  client.setHandshakeTimeout(20);
  bool connected = client.connect(gHost.c_str(), 443);
  // WL_CONNECTED comes with the IPv4 lease; the SLAAC address follows a moment later, and
  // until then DNS asks for IPv4 only. An IPv6-only server needs it, so wait and retry once.
  if (!connected && !WiFi.STA.hasGlobalIPv6()) {
    uint32_t t0 = millis();
    while (!WiFi.STA.hasGlobalIPv6() && millis() - t0 < IPV6_WAIT_MS) delay(50);
    if (WiFi.STA.hasGlobalIPv6()) {
      Serial.printf("[http] retrying over IPv6 after %lu ms\n", millis() - t0);
      connected = client.connect(gHost.c_str(), 443);
    }
  }
  if (!connected) {
    char err[100];
    client.lastError(err, sizeof(err));
    Serial.printf("[http] connect/TLS failed: %s\n", err);
    return -1;
  }

  String req;
  req.reserve(512 + body.length());
  req += method; req += ' '; req += path; req += " HTTP/1.1\r\n";
  req += "Host: "; req += gHost; req += "\r\n";
  req += "User-Agent: pruflock/" FW_VERSION "\r\n";
  req += "Accept-Encoding: identity\r\n";
  req += "Connection: close\r\n";
  String cookies = jar.header();
  if (cookies.length()) { req += "Cookie: "; req += cookies; req += "\r\n"; }
  if (body.length() || strcmp(method, "POST") == 0) {
    req += "Origin: "; req += gOrigin; req += "\r\n";
    if (contentType) { req += "Content-Type: "; req += contentType; req += "\r\n"; }
    req += "Content-Length: "; req += body.length(); req += "\r\n";
  }
  req += "\r\n";
  req += body;
  if (client.print(req) != req.length()) { client.stop(); return -2; }

  String line;
  if (!readLine(client, line, deadline) || !line.startsWith("HTTP/")) { client.stop(); return -3; }
  resp.status = line.substring(line.indexOf(' ') + 1).toInt();

  bool chunked = false;
  long contentLength = -1;
  for (;;) {
    if (!readLine(client, line, deadline)) { client.stop(); return -4; }
    if (line.isEmpty()) break;
    int colon = line.indexOf(':');
    if (colon <= 0) continue;
    String name = line.substring(0, colon);
    name.toLowerCase();
    String value = line.substring(colon + 1);
    value.trim();
    if (name == "set-cookie") jar.applySetCookie(value);
    else if (name == "location") resp.location = value;
    else if (name == "content-length") contentLength = value.toInt();
    else if (name == "transfer-encoding") { value.toLowerCase(); chunked = value.indexOf("chunked") >= 0; }
  }

  bool stopped = false;
  bool ok = true;
  if (strcmp(method, "HEAD") == 0 || resp.status == 204 || resp.status == 304) {
    // no body
  } else if (chunked) {
    for (;;) {
      if (!readLine(client, line, deadline)) { ok = false; break; }
      if (line.isEmpty()) continue;
      long size = strtol(line.c_str(), nullptr, 16);
      if (size <= 0) break;
      if (!pump(client, size, sink, deadline, stopped)) { ok = false; break; }
      if (stopped) break;
    }
  } else {
    ok = pump(client, contentLength, sink, deadline, stopped);
  }
  client.stop();
  return ok ? resp.status : -5;
}
