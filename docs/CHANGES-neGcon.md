# neGcon Changes

Source release identifier: `0.7.12-neGcon`. Numeric CMake project version: `0.7.12`. The produced date/build ID is supplied by the release script, not a fixed document timestamp.

## Current Source Behavior

- neGcon I defaults to mode `12` (RY negative), II to mode `11` (RY positive), and L to mode `0` (no analog assignment).
- I, II, and L each have independent digital enable (`0`/`1`, default **ON**), inner deadzone (default **0%**), and outer endpoint (default **100%**). Valid values require inner < outer. Digital gating uses raw pressure and its threshold, independently of the analog assignment.
- These nine settings are carried through firmware configuration, web editing, TXT presets, native PC editing, and the shared analog-preview model.
- Pressure assignments may overlap twist axes and one another. Contributions are summed before final output saturation. Full-axis contribution is `(u - 0.5) * 65535`, where `u` is the normalized value after the pressure endpoint/deadzone transform; it is not a half-range contribution. Directional assignments and triggers use their corresponding signed/unipolar contributions.
- TXT exports use preset format version **3**. Supported older preset inputs are normalized by the current parser; preserve the samples, specification, and parser tests. Shared preview ABI remains version **3** with updated pressure endpoint fields, so use the matching rebuilt DLL and Python interface together.
- The settings footer magic is `0x50535833`. Settings using incompatible older magic are rejected and replaced with defaults. **Back up needed profiles before flashing; do not assume all previous saved settings survive this change.**

## Release Build

Portable source-root build wrappers support the external Pico SDK 2.1.1 and Arm GNU Toolchain 14_2_Rel1. They do not embed personal paths, fetch missing libraries, regenerate the web UI, flash hardware, or copy to a separate distribution destination by default. Python package installation and npm web regeneration require explicit opt-in. See [BUILD-neGcon.md](BUILD-neGcon.md).

## Validation Boundary

PC parser, shared-math, API contract, and web simulation tests are hardware-free. The latest neGcon source changes must not be described as flashed or hardware-verified solely because those tests pass. Historical `0x3bc` / `0x6b` hardware logs belong to earlier firmware runs and do not verify this release. This document does not claim a new hardware run or a new firmware build.
