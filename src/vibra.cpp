// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// Vibration motor: pulse patterns and a safe state during deep sleep.

#include "vibra.h"
#include "config.h"

void vibraInit() {
  gpio_hold_dis((gpio_num_t)PIN_VIBRA);
  pinMode(PIN_VIBRA, OUTPUT);
  digitalWrite(PIN_VIBRA, LOW);
}

void vibrate(int pulses, uint32_t onMs) {
  if (!onMs) onMs = VIBRA_ON_MS;
  Serial.printf("[vibra] %d × %lu ms\n", pulses, (unsigned long)onMs);
  for (int i = 0; i < pulses; i++) {
    digitalWrite(PIN_VIBRA, HIGH);
    delay(onMs);
    digitalWrite(PIN_VIBRA, LOW);
    if (i < pulses - 1) delay(VIBRA_OFF_MS);
  }
}

void vibraSleep() {
  digitalWrite(PIN_VIBRA, LOW);
  gpio_hold_en((gpio_num_t)PIN_VIBRA);   // gpio_deep_sleep_hold_en() is enabled in Display::off()
}
