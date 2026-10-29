# GP2040-CE neGcon Source

This source fork is based on GP2040-CE `0.7.12`, with fixed Seeed XIAO RP2040 PSX controller input, neGcon pressure mapping, JOGCON support, a web configurator, shared PC analog simulation, and an optional native TXT profile editor. The source release identifier is **0.7.12-neGcon**, not an unrelated application version. The original upstream overview remains in [README.md](README.md).

## Start Here

- [Build the full source distribution](docs/BUILD-neGcon.md), including external dependencies, opt-in web regeneration, native tests, and dated UF2 naming.
- [Current neGcon changes and settings compatibility warning](docs/CHANGES-neGcon.md).
- [TXT preset specification and samples](preset-format/SPECIFICATION.md).
- [Optional native Windows profile editor](pc-profile-editor/README.md).

The archive must retain the complete CMake sources, `headers`, `src`, `configs`, `proto`, `modules`, required `lib` trees including local ArduinoJson/nanopb/TinyUSB/Pico-PIO-USB, prebuilt `lib/httpd/fsdata.c`, web source/lockfile, preset samples, native editor, and required test sources. The Pico SDK and compiler are external. Do not distribute `.git`, `.kilo`, local settings/authentication backups, private test results, dependency caches, virtual environments, `node_modules`, or build output as firmware source prerequisites. Preserve the original `README.md`, root `LICENSE`, and all applicable third-party notices. Source cleanup must not remove required library code or its license notices.

## Wiring

PSX SPI is **250 kHz**, not the RP2040 CPU frequency. XIAO D labels differ from RP2040 GPIO numbers:

| Signal | XIAO Label | RP2040 GPIO |
| --- | --- | --- |
| PSX DATA | D9 | 4 |
| PSX CMD | D10 | 3 |
| PSX CLK | D8 | 2 |
| PSX ATT | D7 | 1 |
| UART0 diagnostic TX | D6 | 0 |
| USB host D+ | fixed | 6 |
| USB host D- | fixed | 7 |

PSX ACK is not connected in this implementation. Check the electrical design, common ground, voltage compatibility, and controller power requirements before wiring; JOGCON motor power is not supplied by a logic signal. PSX input virtual slots 08..23 are not physical GPIO connections. The onboard active-low RGB uses GPIO R=17/G=16/B=25; the external WS2813 data/enable pins are fixed at 12/11.

## neGcon Defaults

I is assigned to RY negative, II to RY positive, L to no analog destination. Their digital outputs remain independently enabled by default. Each pressure input has inner 0% / outer 100% defaults, with independent analog assignments and raw-pressure digital thresholds. Axis contributions can overlap and are added before saturation. Draft monitor values are computed previews, not proof of actual USB output or firmware timing.

Old settings magic is intentionally incompatible with `0x50535833`; back up required profiles before installing firmware. The latest source changes are not declared hardware-tested merely on the basis of prior logs or successful PC tests.

## Origins And Licenses

Upstream GP2040-CE: [OpenStickCommunity/GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE). PSX fork reference: [v9938/GP2040-CE](https://github.com/v9938/GP2040-CE). These URLs identify source origins, not dependencies to fetch during a normal release build.

The root [LICENSE](LICENSE) is MIT and must be preserved verbatim. **Not every bundled file is covered by that root license.** Keep per-directory license files, SPDX identifiers, and source-header notices, including those retained from upstream libraries:

- TinyUSB 0.17 source includes its MIT license and additional vendor/platform notices under `lib/tinyusb`; preserve those notices rather than relabeling all contents MIT.
- Pico-PIO-USB has its own MIT notice under `lib/pico_pio_usb`.
- ArduinoJson retains its local library license; nanopb retains `lib/nanopb/LICENSE.txt`.
- Raspberry Pi-derived PIO code retains the applicable BSD notice, including extracted code even when its former enclosing library is removed.
- Other retained libraries and vendored code may carry BSD, zlib, GPL, or other terms. Review the notices for the actual files shipped; do not claim the entire archive is MIT or remove notices with unused documentation.

No authentication secrets, personal filesystem paths, debug ELF, or private hardware logs are required to build this source distribution. Release wrappers create local build output only and never copy into an external distribution destination.
