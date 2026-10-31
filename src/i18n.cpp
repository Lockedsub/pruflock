// SPDX-License-Identifier: MIT
// PrufLock (https://github.com/Lockedsub/pruflock)
// Author: LockedSub, Zürich, Switzerland
// String table lookup for the UI languages.

#include "i18n.h"

Lang gLang = Lang::EN;

static const char* const TABLE[S_COUNT][2] = {
#define I18N_ROW(id, en, de) {en, de},
  I18N_STRINGS(I18N_ROW)
#undef I18N_ROW
};

const char* tr(Str id, Lang lang) {
  return id < S_COUNT ? TABLE[id][(uint8_t)lang] : "";
}

const char* tr(Str id) { return tr(id, gLang); }

Lang langFromCode(const String& code) { return code == "de" ? Lang::DE : Lang::EN; }

const char* langCode(Lang lang) { return lang == Lang::DE ? "de" : "en"; }
