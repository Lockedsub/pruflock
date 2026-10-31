// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Persistent settings interface: networks, server, login, cached session data.

#pragma once
#include <Arduino.h>
#include "i18n.h"

/** Persistent configuration in NVS. Entered through the setup portal, never compiled into the
 *  firmware. */
struct Settings {
  static constexpr int NETS = 3;
  String ssid[NETS], pass[NETS];
  String host;                  // chastitytracker server, host name only
  String user, password;
  Lang lang = Lang::EN;   // UI language; English unless chosen on the setup page
  bool authBad = false;   // last login was rejected → don't retry on its own

  bool configured() const;
  void load();
  void save() const;
  void setAuthBad(bool bad);
  /** Factory reset: wipes everything in NVS (networks, account, cookie, cached name and zone). */
  static void erase();

  /** The wearer's real username as the app reports it (cached for the "Checking" screen). */
  static String loadUsername();
  static void saveUsername(const String& u);

  /** The account's IANA timezone as the app reports it ("" = not known yet). */
  static String loadTimezone();
  static void saveTimezone(const String& tz);

  static String loadCookie();
  static void saveCookie(const String& c);
};
