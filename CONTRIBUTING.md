# Contributing

Issues and pull requests are welcome. Every pull request and every push to `main` is built by GitHub Actions (`.github/workflows/build.yml`); please check that `pio run` builds without warnings first.

## Code

- One concern per file in `src/`: `main.cpp` is the wake/sleep state machine and Wi-Fi, `tracker.cpp` the ChastityTracker API, `http.cpp` the HTTPS client, `portal.cpp` the setup portal, `display.cpp` the screens. Timings and pins are in `config.h`.
- [docs/internals.md](docs/internals.md) explains the API calls, the setup portal and other details.
- Keep the serial log useful: every step prints a `[tag] …` line.
- Test on the real board, and with an iPhone for anything that touches the setup portal.

## Docs

- No hard line wraps in Markdown: one line per paragraph, list item or table row, so the reader's window sets the line width.

## Adding a language

Every user-visible string is defined once in `src/i18n.h` (`I18N_STRINGS`: id, English, German) and fetched with `tr(S_…)`. To add a language, add a column there and a `Lang` value, widen `TABLE` and extend `langFromCode()`/`langCode()` in `src/i18n.cpp`, and add a link to the language switch at the top of the setup page (`page()` in `src/portal.cpp`).

The display fonts contain only ASCII plus `ÄÖÜäöüß°·–…`. New characters must be added to the list at the top of `tools/make_fonts.swift`. Then regenerate `src/fonts.h` (on macOS):

```bash
swift tools/make_fonts.swift
```

## Generated files

| File | Generator | When to re-run |
|---|---|---|
| `src/fonts.h` | `swift tools/make_fonts.swift` | other characters, font or sizes |
| `src/tz_db.h` | `python3 tools/make_tz.py` | a new tzdata release changes a zone's rules |
| `case/stl/*.stl`, `case/*.3mf`, `case/preview/*.png` | `case/build.sh` | after changing a `.scad` file or `case/3mf_parts.py` |

## Releases

1. Bump `FW_VERSION` in `src/config.h` and add a section to `CHANGELOG.md`.
2. Tag and push: `git tag v1.2.3 && git push --tags`.
3. The workflow attaches `pruflock-1.2.3.bin` to a GitHub Release and updates the browser flasher on GitHub Pages.

One-time setup on GitHub before the first release:

- Settings → Pages → Source: **GitHub Actions**.
- Settings → Environments → `github-pages` → Deployment branches and tags: add a **tag** rule `v*`. By default only the default branch may deploy to Pages, so the deploy from a release tag would be rejected.
