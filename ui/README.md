# Native TFT player

`calibrate.c --player` is the main application, started by `S99calibrate`.
The name is retained for compatibility with the package/init scripts. Rendering
and input run on the UI thread; `backend.c` handles playback and blocking output
operations in a worker. `library.c` loads the synced SQLite library.

The ST7789 framebuffer is 320×240 landscape, rotate=90, SPI 27 MHz. XPT2046
input mapping is configured through `/etc/default/kernix-ui`. The animation
opens Controls (volume and configured-device Bluetooth pairing). Pairing remains
black until confirmed, then green. Touch coordinates and existing button actions
are shared by all themes. After 30 seconds without touch, an overlay shows the
song title and progress. One touch dismisses it without activating a button
underneath, returning to the previous page while playback continues.

## Themes and assets

`theme.h` selects `themes/{crimson,moonlit,ukiyo,eclipse}.h` at build time.
Use `./scripts/build-theme.sh crimson` from the project root to rebuild firmware
and save a checksummed image under `dist/theme-images/`. This does not flash a card.
The current inactivity background is Crimson for all theme variants.

Vector sources, host bitmaps and RGB565 device headers live in `assets/`.
Retain the embedded headers: the Pi uses them directly, without a runtime
SVG/image decoder. Baked DejaVu font masks include their notice in
`assets/fonts/LICENSE`. Host generators under `scripts/` need Pillow; the SVG
rasterizers also need Python GI/Rsvg and Cairo. These are asset-development
dependencies, not normal firmware build requirements.

Render a host preview:

```sh
gcc -std=c99 -Wall -Wextra -Werror -o /tmp/kernix-theme-preview \
  scripts/preview-ui-theme.c ui/library.c ui/backend.c -lsqlite3 -pthread
/tmp/kernix-theme-preview
```

This writes PPM frames under `/tmp/kernix-*-theme.ppm`. 

## Display diagnostics

The existing `--rgb`, `--blue`, `--corners` and default touch diagnostic modes,
plus `kernix-display-test` and `kernix-lcd-registers`, remain useful for hardware
bring-up. Stop the normal UI before running one:

```sh
/etc/init.d/S99calibrate stop
kernix-calibrate --rgb
# After finishing the diagnostic:
/etc/init.d/S99calibrate start
```

Logs: `/tmp/kernix-calibrate.log`. Do not alter SPI speed, polarity, rotation
or touch transforms to solve an unrelated visual-theme problem.
