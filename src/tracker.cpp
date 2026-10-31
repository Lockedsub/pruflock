// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// ChastityTracker API client: login, heartbeat, open inspections, session.

#include "tracker.h"
#include <ArduinoJson.h>
#include <string>

const char* resultName(TrackerResult r) {
  switch (r) {
    case TrackerResult::Ok: return "ok";
    case TrackerResult::NeedLogin: return "need-login";
    case TrackerResult::BadCredentials: return "bad-credentials";
    case TrackerResult::RateLimited: return "rate-limited";
    case TrackerResult::NetError: return "net-error";
    case TrackerResult::ServerError: return "server-error";
  }
  return "?";
}

// A 429 is a server error here; only login() reports it as RateLimited ("login blocked").
static TrackerResult classify(int status) {
  if (status < 0) return TrackerResult::NetError;
  if (status == 401) return TrackerResult::NeedLogin;
  return TrackerResult::ServerError;
}

// Collects a small body (JSON) into a string, capped.
static BodySink collect(std::string& into, size_t cap) {
  return [&into, cap](const uint8_t* d, size_t n) {
    if (into.size() + n > cap) return false;
    into.append((const char*)d, n);
    return true;
  };
}

// ── heartbeat ────────────────────────────────────────────────────────────────

TrackerResult TrackerClient::heartbeat(String& ids, int* locked) {
  std::string body;
  HttpResponse r;
  int st = httpsRequest("GET", "/api/heartbeat", jar, nullptr, "", r, collect(body, 4096));
  if (st != 200) return classify(st);

  JsonDocument doc;
  if (deserializeJson(doc, body)) return TrackerResult::ServerError;
  if (doc["sessionUserId"].isNull()) return TrackerResult::NeedLogin;

  // pendingSig = "l:1|t:...|k:<sorted pending inspection ids>|v:...|..."
  String sig = doc["pendingSig"] | "";
  ids = "";
  if (locked) *locked = -1;
  int start = 0;
  while (start <= (int)sig.length()) {
    int end = sig.indexOf('|', start);
    if (end < 0) end = sig.length();
    String part = sig.substring(start, end);
    if (part.startsWith("k:")) ids = part.substring(2);
    else if (part.startsWith("l:") && locked) *locked = part.substring(2) == "1" ? 1 : 0;
    start = end + 1;
  }
  return TrackerResult::Ok;
}

TrackerResult TrackerClient::session(SessionInfo& out) {
  std::string body;
  HttpResponse r;
  int st = httpsRequest("GET", "/api/auth/session", jar, nullptr, "", r, collect(body, 4096));
  if (st != 200) return classify(st);
  JsonDocument doc;
  if (deserializeJson(doc, body)) return TrackerResult::ServerError;
  if (doc.isNull() || doc["user"].isNull()) return TrackerResult::NeedLogin;
  out.name = doc["user"]["name"] | "";
  out.timezone = doc["user"]["timezone"] | "";
  return TrackerResult::Ok;
}

// ── login ────────────────────────────────────────────────────────────────────

TrackerResult TrackerClient::login(const String& user, const String& password) {
  jar.clear();
  std::string body;
  HttpResponse r;
  int st = httpsRequest("GET", "/api/auth/csrf", jar, nullptr, "", r, collect(body, 1024));
  if (st == 429) return TrackerResult::RateLimited;
  if (st != 200) return classify(st);
  JsonDocument doc;
  if (deserializeJson(doc, body)) return TrackerResult::ServerError;
  String csrf = doc["csrfToken"] | "";
  if (csrf.isEmpty()) return TrackerResult::ServerError;

  String form = "username=" + urlEncode(user) + "&password=" + urlEncode(password) +
                "&csrfToken=" + urlEncode(csrf) +
                "&callbackUrl=" + urlEncode(httpOrigin() + "/dashboard");
  st = httpsRequest("POST", "/api/auth/callback/credentials", jar,
                    "application/x-www-form-urlencoded", form, r, nullptr);
  Serial.printf("[login] status %d, location %s\n", st, r.location.c_str());
  if (st < 0) return TrackerResult::NetError;
  if (st == 429) return TrackerResult::RateLimited;
  if (jar.has(SESSION_COOKIE)) return TrackerResult::Ok;
  if (r.location.indexOf("error=") >= 0) return TrackerResult::BadCredentials;
  return TrackerResult::ServerError;
}

// ── open inspections ─────────────────────────────────────────────────────────

static time_t parseIsoUtc(const String& s) {   // 2026-10-12T15:00:00.000Z
  if (s.length() < 19 || s[4] != '-' || s[10] != 'T') return 0;
  int y = s.substring(0, 4).toInt(), mo = s.substring(5, 7).toInt(), d = s.substring(8, 10).toInt();
  int h = s.substring(11, 13).toInt(), mi = s.substring(14, 16).toInt(), se = s.substring(17, 19).toInt();
  // days from civil (Howard Hinnant)
  y -= mo <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = (long)era * 146097 + (long)doe - 719468;
  return (time_t)days * 86400 + h * 3600 + mi * 60 + se;   // 64-bit: past 2038
}

TrackerResult TrackerClient::fetchInspection(Inspection& out) {
  out = Inspection();
  std::string body;
  HttpResponse r;
  int st = httpsRequest("GET", "/api/kontrollen/open", jar, nullptr, "", r, collect(body, 4096));
  if (st != 200) return classify(st);

  JsonDocument doc;
  if (deserializeJson(doc, body)) return TrackerResult::ServerError;
  JsonArrayConst list = doc["inspections"];
  if (list.isNull()) return TrackerResult::ServerError;

  // Sorted by deadline, most urgent first; `code` is null when the target needs none.
  out.count = list.size();
  if (out.count) {
    out.code = list[0]["code"] | "";
    out.deadline = parseIsoUtc(list[0]["deadline"] | "");
  }
  Serial.printf("[open] %d inspections, code %s, deadline %ld\n", out.count,
                out.code.isEmpty() ? "(none)" : out.code.c_str(), (long)out.deadline);
  return TrackerResult::Ok;
}
