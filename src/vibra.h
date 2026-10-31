// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Vibration motor interface.

#pragma once
#include <Arduino.h>

/** Vibration motor on PIN_VIBRA (through a transistor driver — never drive a motor from a pin
 *  directly). Without a motor fitted all of this is harmless. */
void vibraInit();
/** Blocking pattern: `pulses` × (VIBRA_ON_MS on, VIBRA_OFF_MS off). */
void vibrate(int pulses, uint32_t onMs = 0);
/** Pin LOW and held through deep sleep, so the motor can't hum while the board sleeps. */
void vibraSleep();
