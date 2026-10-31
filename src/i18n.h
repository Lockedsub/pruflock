// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// All user-visible strings, in English and German.

#pragma once
#include <Arduino.h>

// UI language. English is the default; the setup page switches it (stored in NVS as "lang").
enum class Lang : uint8_t { EN = 0, DE = 1 };

// One line per string: id, English, German.
// The display fonts (src/fonts.h) cover ASCII plus ÄÖÜäöüß°·–… — add a character there before
// using it here.
#define I18N_STRINGS(X)                                                                       \
  /* display: inspection */                                                                   \
  X(INSPECTION,        "INSPECTION",              "KONTROLLE")                                \
  X(INSPECTION_OPEN,   "Inspection pending",      "Kontrolle offen")                          \
  X(NO_CODE_SEE_APP,   "no code – see app",       "kein Code – siehe App")                    \
  X(DUE_BY,            " · due ",                 " · bis ")                                  \
  X(DUE_DATE_FMT,      "%d %b %H:%M",             "%d.%m. %H:%M")                             \
  X(DUE_DAY_FMT,       "%d %b",                   "%d.%m.")                                   \
  X(TIME_LEFT_FMT,     "%s left",                 "noch %s")                                  \
  X(OVERDUE_FMT,       "overdue %s",              "%s zu spät")                               \
  X(OFFLINE,           "offline",                 "offline")                                  \
  /* display: status screen */                                                                \
  X(LOCKED,            "LOCKED",                  "VERSCHLOSSEN")                             \
  X(UNLOCKED,          "UNLOCKED",                "OFFEN")                                    \
  X(NO_INSPECTION,     "No inspection pending",   "Keine Kontrolle offen")                    \
  X(STATUS_UNKNOWN,    "Status ?",                "Status ?")                                 \
  /* display: progress / errors */                                                            \
  X(CONNECTING,        "Connecting Wi-Fi …",      "Verbinde WLAN …")                          \
  X(SCANNING,          "scanning networks",       "suche Netze")                              \
  X(CHECKING,          "Checking",                "Prüfe")                                    \
  X(SIGNING_IN,        "Signing in …",            "Anmelden …")                               \
  X(LOGIN_REJECTED,    "Login rejected",          "Login abgelehnt")                          \
  X(CHECK_PASSWORD,    "Check password:",         "Passwort prüfen:")                         \
  X(HOLD_B_SETUP,      "hold B 3 s = setup",      "B 3 s halten = Einrichtung")               \
  X(LOGIN_BLOCKED,     "Login blocked",           "Login gesperrt")                           \
  X(TOO_MANY,          "too many attempts,",      "zu viele Versuche,")                       \
  X(RETRY_IN,          "retry in ",               "neuer Versuch in ")                        \
  X(SETUP_Q,           "Setup?",                  "Einrichtung?")                             \
  X(KEEP_HOLDING_B,    "keep holding B …",        "B weiter halten …")                        \
  X(DONE,              "Done",                    "Erledigt")                                 \
  X(NO_LONGER_PENDING, "no inspection pending",   "keine Kontrolle mehr offen")               \
  X(NO_WIFI,           "No Wi-Fi",                "Kein WLAN")                                \
  X(NEXT_TRY_IN,       "next try in ",            "nächster Versuch in ")                     \
  X(HOTSPOT_HINT,      "iPhone: open Hotspot page", "iPhone: Hotspot öffnen")                 \
  X(SERVER_ERROR,      "Server error",            "Server-Fehler")                            \
  /* display: setup */                                                                        \
  X(SETUP,             "Setup",                   "Einrichtung")                              \
  X(AP_USE_PHONE,      "Use your phone",          "Handy verwenden")                          \
  X(STARTING_SETUP,    "Starting setup …",        "Einrichtung startet …")                    \
  X(RESTARTING,        "Restarting",              "Neustart")                                 \
  X(AP_WIFI,           "Wi-Fi:",                  "WLAN:")                                    \
  X(AP_PASSWORD,       "Password:",               "Passwort:")                                \
  X(AP_CANCEL,         "Press any button to cancel", "Zum Abbrechen Taste drücken")          \
  X(AP_JOINED,         "Connected",               "Verbunden")                                \
  X(AP_NO_PAGE,        "No page? Open in browser:", "Keine Seite? Im Browser:")               \
  X(AP_OR_WAIT,        "or wait about 45 s",      "oder ca. 45 s warten")                     \
  /* setup web page */                                                                        \
  X(WEB_LANGUAGE,      "Language",                "Sprache")                                  \
  X(WEB_WIFI,          "Wi-Fi",                   "WLAN")                                     \
  X(WEB_WIFI_HINT1,    " (e.g. home)",            " (z.B. Zuhause)")                          \
  X(WEB_WIFI_HINT2,    " (e.g. iPhone hotspot)",  " (z.B. iPhone-Hotspot)")                   \
  X(WEB_PASSWORD,      "Password",                "Passwort")                                 \
  X(WEB_UNCHANGED,     "(unchanged)",             "(unverändert)")                            \
  X(WEB_SERVER,        "Server",                  "Server")                                   \
  X(WEB_SERVER_HINT,   "Host name of your chastitytracker instance",                          \
                       "Host-Name deiner Chastitytracker-Instanz")                            \
  X(WEB_ACCOUNT,       "Wearer account",          "Träger-Konto")                             \
  X(WEB_USER,          "Username or e-mail",      "Benutzername oder E-Mail")                 \
  X(WEB_HINT,          "Empty SSID = remove network. Empty password = keep the current one.", \
                       "Leeres SSID-Feld = Netz entfernen. Leeres Passwortfeld = bisheriges behalten.") \
  X(WEB_SAVE,          "Save &amp; restart",      "Speichern &amp; neu starten")              \
  X(WEB_MISSING,       "Please enter at least one Wi-Fi network, the server and the login.", \
                       "Bitte mindestens ein WLAN, den Server und die Anmeldedaten angeben.") \
  X(WEB_SAVED,         "Saved.",                  "Gespeichert.")                             \
  X(WEB_SAVED_TEXT,    "The device restarts and signs in.", "Das Gerät startet neu und meldet sich an.") \
  X(WEB_RESET,         "Erase all settings",      "Alle Einstellungen löschen")               \
  X(WEB_RESET_CONFIRM, "Erase all networks and the login? The device restarts in setup.",    \
                       "Alle Netze und die Anmeldedaten löschen? Das Gerät startet danach die Einrichtung.") \
  X(WEB_RESET_DONE,    "Erased.",                 "Gelöscht.")                                \
  X(WEB_RESET_DONE_TEXT, "The device restarts in setup. Join its Wi-Fi again.",              \
                       "Das Gerät startet die Einrichtung. Verbinde dich erneut mit seinem WLAN.")

enum Str : uint8_t {
#define I18N_ENUM(id, en, de) S_##id,
  I18N_STRINGS(I18N_ENUM)
#undef I18N_ENUM
  S_COUNT
};

extern Lang gLang;

const char* tr(Str id);
const char* tr(Str id, Lang lang);

Lang langFromCode(const String& code);   // "de" → DE, anything else → EN
const char* langCode(Lang lang);
