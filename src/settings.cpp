// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Persistent settings in NVS: networks, server, login, cached session data.

#include "settings.h"
#include <Preferences.h>

// NVS namespace. Always opened read-write: on a fresh badge that creates it, where a read-only
// open would log an error.
static const char* NS = "pruflock";

/** "" for a missing key. isKey first: reading a missing key would log an NVS error. */
static String getStr(Preferences& p, const char* key) {
  return p.isKey(key) ? p.getString(key, "") : String();
}

bool Settings::configured() const {
  bool anyNet = false;
  for (int i = 0; i < NETS; i++) anyNet |= ssid[i].length() > 0;
  return anyNet && host.length() && user.length() && password.length();
}

void Settings::load() {
  Preferences p;
  p.begin(NS, false);
  for (int i = 0; i < NETS; i++) {
    ssid[i] = getStr(p, ("s" + String(i)).c_str());
    pass[i] = getStr(p, ("p" + String(i)).c_str());
  }
  host = getStr(p, "host");
  user = getStr(p, "user");
  password = getStr(p, "pw");
  authBad = p.isKey("authBad") && p.getBool("authBad", false);
  lang = langFromCode(getStr(p, "lang"));
  p.end();
}

void Settings::save() const {
  Preferences p;
  p.begin(NS, false);
  for (int i = 0; i < NETS; i++) {
    p.putString(("s" + String(i)).c_str(), ssid[i]);
    p.putString(("p" + String(i)).c_str(), pass[i]);
  }
  p.putString("host", host);
  p.putString("user", user);
  p.putString("pw", password);
  p.putBool("authBad", authBad);
  p.putString("lang", langCode(lang));
  p.end();
}

void Settings::setAuthBad(bool bad) {
  if (authBad == bad) return;
  authBad = bad;
  Preferences p;
  p.begin(NS, false);
  p.putBool("authBad", bad);
  p.end();
}

void Settings::erase() {
  Preferences p;
  p.begin(NS, false);
  p.clear();
  p.end();
}

static String loadKey(const char* key) {
  Preferences p;
  p.begin(NS, false);
  String v = getStr(p, key);
  p.end();
  return v;
}

static void saveKey(const char* key, const String& v) {
  Preferences p;
  p.begin(NS, false);
  p.putString(key, v);
  p.end();
}

String Settings::loadUsername() { return loadKey("uname"); }
void Settings::saveUsername(const String& u) { saveKey("uname", u); }
String Settings::loadTimezone() { return loadKey("tz"); }
void Settings::saveTimezone(const String& tz) { saveKey("tz", tz); }
String Settings::loadCookie() { return loadKey("cookie"); }
void Settings::saveCookie(const String& c) { saveKey("cookie", c); }
