# PocketBook regression checks

Run `sh pocketbook-tests/run.sh` with a host C compiler. Tests include the actual
application sources with a minimal InkView substitute and undefined-behavior
sanitizer. They cover movement bounds and update rectangles, journal validation
and save failures, invalid/single-hour weather data, timer hit testing, and
catalog refresh guards while audio or downloads are active.

These tests do not emulate the firmware, fonts, audio playback, power management
or the physical e-ink panel. GitHub Actions builds against the actual PocketBook
SDK; on-device checks are still required for touch, networking and rendering.

## September 2026 usability changes

- Weather: recoverable refresh errors, connection feedback, single-hour graph
  handling, wind units, and clothing panel bounded by screen height.
- Podcasts: visible download status and cancellation, protection against changing
  catalog indices during playback/download, and room for long download URLs.
- Journal: temporary-file saves preserve the previous CSV on failure; errors are
  displayed, M4A is selectable, and imported progress/rating are bounded.
- Timer: explicit Back button in custom-time mode; stray touches keep the value.
- Sokoban: only status and changed cells refresh after movement; out-of-board
  moves are guarded and tapping the player no longer causes a downward move.

Weather and podcasts now use the server at 5.10.251.95:8093. Previously installed
binaries still contain the former address and must be replaced.
