// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// HTTPS client interface: one request per call, with a cookie jar.

#pragma once
#include <Arduino.h>
#include <functional>

/** Name (prefix) of the Auth.js session cookie(s): what survives sleep and reboot. */
constexpr const char* SESSION_COOKIE = "__Secure-authjs.session-token";

/** Minimal cookie jar for a single host. A later Set-Cookie with the same name wins
 *  (the app's /api/auth/csrf sends the csrf cookie twice; the last one is valid). */
class CookieJar {
public:
  void set(const String& name, const String& value);
  void remove(const String& name);
  void clear() { n_ = 0; }
  bool has(const String& prefix) const;
  String header() const { return join(""); }   // "a=b; c=d"
  String sessionCookies() const { return join(SESSION_COOKIE); }
  void loadSessionCookies(const String& s);
  void applySetCookie(const String& headerValue);
private:
  String join(const char* prefix) const;
  static constexpr int MAX = 8;
  String names_[MAX], values_[MAX];
  int n_ = 0;
};

struct HttpResponse {
  int status = 0;
  String location;
};

/** Gets body bytes; return false to stop reading (connection is then closed). */
using BodySink = std::function<bool(const uint8_t* data, size_t len)>;

/** Server for all requests (host name only, port 443). Call once before the first request. */
void httpSetHost(const String& host);
const String& httpOrigin();

/** One HTTPS request to the host set by httpSetHost() over a fresh TLS connection (no redirects followed,
 *  chunked transfer decoded). Returns the HTTP status, or a negative value on network error. */
int httpsRequest(const char* method, const String& path, CookieJar& jar,
                 const char* contentType, const String& body,
                 HttpResponse& resp, const BodySink& sink);

String urlEncode(const String& s);
