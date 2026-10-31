// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// ChastityTracker API client interface.

#pragma once
#include <Arduino.h>
#include <time.h>
#include "http.h"

enum class TrackerResult { Ok, NeedLogin, BadCredentials, RateLimited, NetError, ServerError };

const char* resultName(TrackerResult r);

/** From /api/auth/session. */
struct SessionInfo {
  String name;      // the wearer's username (also after an e-mail login)
  String timezone;  // IANA zone set on the account, e.g. "Europe/Zurich"; "" if the app has none
};

struct Inspection {
  String code;          // empty = pending, but no code required
  time_t deadline = 0;  // 0 = unknown
  int count = 0;        // open inspections in total (this one is the most urgent)
};

/** Talks to the chastitytracker instance (httpSetHost) as the logged-in wearer (same session cookie a browser gets). */
class TrackerClient {
public:
  CookieJar jar;

  /** GET /api/heartbeat. `ids` = comma-separated ids of the pending inspections ("" = none);
   *  `locked` (optional) = 1/0 from the `l:` part, -1 if absent. */
  TrackerResult heartbeat(String& ids, int* locked = nullptr);

  /** GET /api/auth/session → the wearer's username and the account's timezone. */
  TrackerResult session(SessionInfo& out);

  /** Auth.js credentials login (csrf + callback). Only call on purpose — failures count
   *  towards the app's per-account lockout and per-IP rate limit. */
  TrackerResult login(const String& user, const String& password);

  /** GET /api/kontrollen/open → the most urgent open inspection's code and deadline. */
  TrackerResult fetchInspection(Inspection& out);
};
