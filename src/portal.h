// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Setup portal interface.

#pragma once
#include "display.h"
#include "settings.h"

/** Opens the SoftAP "PrufLock" (password shown on the display) with a setup form for the
 *  Wi-Fi networks, the server and the wearer's login. Never returns: always restarts. */
[[noreturn]] void runPortal(Settings& settings, Display& display);
