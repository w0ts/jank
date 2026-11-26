# Jank

> ## [→ w0ts.github.io/jank](https://w0ts.github.io/jank/)
> **What Jank is, how to install it from the browser, and the device's control page: on the site.**
> This README is for developers.

Jank brings computers, MIDI gear and modular gear into one Ableton Link session with no router: the group mode of
[Optimist](https://github.com/w0ts/optimist), the open firmware for the FM-1 synth. Link is Ableton's trademark; Jank is
not an Ableton product.

## Build and flash

```
scripts/install_idf.sh       # once: ESP-IDF v5.5.6 into the scratch folder (needs Python 3.13)
make build                   # scripts/build.sh auto: the release image, XIAO or jank mini 3, told apart at the start
make flash                   # build, then install on the running device: the JANK drive, else USB MIDI (scripts/ota.py)
bash tests/run.sh            # the host tests (C and the pages' JS)
```

Build files stay in `$JANK_SCRATCH` (default `worktrees/jank-scratch/`, git-ignored). Boards, builds, partitions and
updates: [docs/firmware.md](docs/firmware.md), [docs/boards.md](docs/boards.md), [docs/usb.md](docs/usb.md).

## Site and releases

- `make page` expands `page/src/*.html` (+ `ui/`) into `page/`. The site is built from `main` by the Pages workflow
  (`.github/workflows/pages.yml`, the shared w0ts site builder with `site.json`) whenever `page/` changes; `make publish-page` runs it by hand.
  No branch holds the site.
- A `v*` tag runs `.github/workflows/release.yml`: it builds the release image, `scripts/publish_firmware.py` writes its
  assets (every flash part and the manifest `jank-auto-TAG.json`), the release carries them, and the Pages workflow lays
  them out under the site's `firmware/` (with `firmware/index.json`) for the Install page's browser flasher.

## Hardware

- [jank mini 3](hardware/jank-mini-3/README.md): our board around the Waveshare ESP32-S3-LCD-2, and its printed case
  (`hardware/jank-mini-3/case/`). The bare device is a Seeed XIAO ESP32-S3 alone.

## Docs

[On the air](docs/air.md) · [USB](docs/usb.md) · [MIDI clock](docs/midi.md) · [boards and pins](docs/boards.md) ·
[LCD-2](docs/lcd2.md) · [LINE IN](docs/linein.md) · [beat follow](docs/beat.md)

**Beat follow's network weights are not in the firmware or in this repository.** The firmware reads them from its
`beatw` partition; the Install page adds madmom's model (CC BY-NC-SA 4.0, non-commercial) at the user's click, from
madmom's own repository, in the user's browser ([docs/beat.md](docs/beat.md)).

## Licensing

The firmware (`firmware/`, `core/`, `tests/`, `scripts/`) is GPL-3.0-only, as it links Optimist's `link.c`
(`third_party/link`, copied from fm1-base, see its README). The control page and its test are MIT. The hardware
scripts carry GPL-3.0-only SPDX tags (`hardware/pcb/lib/stitch.py` CC0-1.0); the KiCad footprint of the XIAO is Seeed's,
CC BY-SA 4.0. See [LICENSE](LICENSE).
