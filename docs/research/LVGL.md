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
- Original MAZ labels, the context strip, the ring mark, and the eight-cell
  paged app table, all built from LVGL objects and styles.
- `lv_timer_handler()` while the LVGL Home screen is active.

## Why the Home table is hand-built rather than `lv_table`

`lv_table` was the obvious candidate for "lay the apps out as a table" and was
rejected on this hardware:

- Its cells are drawn, not objects, so a cell cannot own a badge chip and a
  leading selection bar as children. Both are needed here, because selection
  has to stay legible by shape once the backlight dims to 12/255 and the amber
  fill converges with the panel fill.
- `lv_table`'s keyboard selection arrives through an `lv_indev` of type
  `LV_INDEV_TYPE_KEYPAD` bound to an `lv_group`, reporting via
  `LV_EVENT_VALUE_CHANGED` and `lv_table_get_selected_cell`. Adopting it would
  mean a second input path running alongside the existing TCA8418 driver and
  the `App::onKey` contract — exactly the parallel implementation this
  integration set out not to create.
- A fixed eight-cell object tree is allocated once in `buildHome()` and only
  re-labelled on a page turn, so paging costs no allocation at all.

`lv_tileview` was likewise considered for the page transition and rejected: an
animated slide is a full-screen redraw per frame against a 14-row partial
buffer, and `lv_refr_now` on the shell's 100 ms repaint cadence cannot carry
that without tearing. Paging is therefore instant, and orientation is carried
by the page counter and dots rather than by motion.

Upstream reference for both, at the pinned version:

- https://docs.lvgl.io/9.5/widgets/table.html
- https://docs.lvgl.io/9.5/widgets/tileview.html
- https://docs.lvgl.io/9.5/main-modules/indev/keypad.html

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
