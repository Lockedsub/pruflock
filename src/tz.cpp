// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Maps the account's IANA timezone name to a POSIX TZ rule.

#include "tz.h"
#include "tz_db.h"

const char* tzPosix(const char* iana) {
  if (!iana || !*iana) return nullptr;
  int lo = 0, hi = sizeof TZ_ZONES / sizeof TZ_ZONES[0] - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = strcmp(iana, TZ_ZONES[mid].name);
    if (c == 0) return TZ_RULES[TZ_ZONES[mid].rule];
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
  return nullptr;
}
