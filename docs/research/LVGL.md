# LVGL integration record

MAZ Pocket uses **LVGL 9.5.0**, pinned in `platformio.ini`. PlatformIO fetches
the library from the official `lvgl/lvgl` package; the upstream source tree is
not duplicated in this repository.

## Saved upstream references

- Introduction: https://lvgl.io/docs/open/introduction
- Source repository: https://github.com/lvgl/lvgl
- Pinned release: https://github.com/lvgl/lvgl/releases/tag/v9.5.0
- PlatformIO integration: https://docs.lvgl.io/9.5/integration/frameworks/platformio.html
- Display setup and partial buffers: https://docs.lvgl.io/9.5/main-modules/display/setup.html
- Licence at the pinned tag: https://github.com/lvgl/lvgl/blob/v9.5.0/LICENCE.txt

Release `v9.5.0` points to upstream commit `85aa60d` and was published on
2026-02-18. The exact MIT notice is saved at
`third_party/lvgl/LICENSE.txt`.

## What MAZ Pocket uses

- `lv_init()` and `lv_tick_set_cb(millis)` for the runtime.
- One 240 × 135 RGB565 display object.
- The standard 64 KiB LVGL internal memory pool.
- A 14-row (6,720-byte) partial draw buffer, slightly above one tenth of the
  screen, explicitly aligned to LVGL's 4-byte requirement and copied into the
  existing M5Canvas framebuffer by a flush callback.
- Original MAZ labels, panels, ring mark, connection state and eight app tiles
  built from LVGL objects and styles.
- `lv_timer_handler()` while the LVGL Home screen is active.

The existing Cardputer ADV TCA8418 driver remains the only keyboard source.
Navigation continues through the established MAZ app contract, so adopting
LVGL does not create a parallel input implementation or change shortcuts.

The physical ADV verification caught an important integration detail: a plain
`uint16_t` array was linked at a 2-byte boundary, causing LVGL's draw-buffer
assertion handler to halt. `alignas(LV_DRAW_BUF_ALIGN)` is required here; a
successful boot now reaches the completed widget tree and READY banner.

## Deliberate limits

- No LVGL demos/examples are compiled or shipped.
- No LVGL Pro/XML tooling or generated output is included.
- No upstream visual assets or branding are copied.
- Other proven MAZ screens remain on the existing M5Canvas primitives until a
  physical-device comparison shows that migrating one improves daily use.
