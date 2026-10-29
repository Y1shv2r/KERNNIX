# Third-party components

Kernix's project license is not selected yet. This document does not relicense
third-party material or assign ownership of pre-existing code.

- **Buildroot:** downloaded separately at `configs/buildroot-revision` from
  https://github.com/buildroot/buildroot. Its licensing is documented in its
  `COPYING`. The local checkout and downloads are not included in source exports.
- **Linux/Raspberry Pi codec patches:** patches under `board/kernix/patches/linux`
  modify the Linux device-tree binding and TLV320AIC31xx driver. Retain upstream
  notices and applicable kernel/binding licensing; a future project license
  does not override those terms.
- **Boot firmware, Realtek firmware, ALSA, BlueZ, BlueALSA, mpg123, SQLite and
  other system packages:** fetched by Buildroot and governed by their individual
  licenses. Before distributing binary images, generate Buildroot's legal-info
  output with `make -C output legal-info` and review its warnings, notices and
  source requirements. An image contains much more than Kernix's own code.
- **DejaVu Sans Mono font:** baked font masks are in `ui/assets/fonts/theme-font.h`.
  The upstream notice/license is preserved in `ui/assets/fonts/LICENSE`.
- **Artwork:** maintained vector sources, generated bitmaps, device headers and
  generators live in `ui/assets/` and `scripts/`. The root project license has
  not yet been assigned to this material. Reference mockups are not bundled.
- **Music:** no MP3s or personal catalog are included in source exports. Import
  your own files; the software license grants no rights to music recordings.
