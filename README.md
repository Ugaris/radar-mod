# Radar & Nameplates

A native client mod for [Ugaris](https://ugaris.com): combat awareness.

- **Health bars** — readable HP bars over the characters around you, colored
  by health (with a shield bar when a magic shield is up). Bars of characters
  at full health are hidden by default to keep the screen clean.
- **Level badges** — the level next to each character, colored by how
  dangerous they are relative to you (gray = harmless, green = your league,
  orange/red = above you).
- **Players in view** — a compact list of players currently on your screen
  with level, distance, and their PK sign; clan mates are tinted green,
  hostile PKs red.
- **Enter-view alert** — the moment another player walks into your view you
  get a chat line and a soft ping. Players already present at login or after
  an area change do not alert; only genuine approaches do.

## Commands

| Command | Effect |
|---------|--------|
| `#radar` | Show status and help |
| `#radar bars` | Toggle health/shield bars |
| `#radar full` | Toggle hiding bars of full-health characters |
| `#radar levels` | Toggle level badges |
| `#radar list` | Toggle the players-in-view list |
| `#radar alert` | Toggle enter-view alerts |
| `#radar sound` | Toggle the alert ping |

Settings persist in `<client config dir>/radar_mod.cfg`.

## Notes

- Player detection uses the client's own rule (base player sprites) plus the
  system mod's `amod_is_playersprite`, resolved at runtime — so Ugaris'
  custom player sprites are recognized when the system mod is present.
- Names can lag a moment behind a character's appearance (the server sends
  them separately); alerts wait briefly for the name before falling back to
  "Someone".
- Everything is read from data the server already sends; the mod sends
  nothing.

## Installing

Install from the Ugaris Launcher: **Mods → Browse → Radar & Nameplates**,
or via *Install from URL* with `Ugaris/radar-mod`.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, first copy `lib/moac.a` / `lib/moac.lib` from the client
release's `mod-sdk.zip` into `lib/` — mods link against the client's import
library there. Released binaries are built by GitHub Actions from tags.

## License

MIT
