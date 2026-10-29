# PSX Offline TXT Profile Editor

Windows x64 native Win32/MSVC file editor. No WebView2, browser, network, Python,
serial port, firmware build, or device write is used by this application.
Only this folder contains the editor implementation. The independent
`usb-host-tester` is not touched.

## Build

Install Visual Studio 2022 Community with Desktop development with C++ and a
Windows SDK, plus CMake. The tested installation is
`C:\Program Files\Microsoft Visual Studio\2022\Community`.
Run `pwsh -File pc-profile-editor/build.ps1` from the repository root.
The script selects `Visual Studio 17 2022`, x64, Release and one build job.
It does not guess a compiler executable or download packages.

Outputs:

- `pc-profile-editor/build/Release/psx_profile_editor.exe`
- `pc-profile-editor/build/Release/preset_tests.exe`
- `pc-profile-editor/build/Release/analog_preview.dll`

The DLL compiles the existing portable `../headers/analog_preview.h` and
`../src/analog_preview.cpp` and, when present, `../src/jogcon.cpp` with
`ANALOG_PREVIEW_DLL`. Its exported C ABI is for
the separate PC analog simulation, not actual USB output or a device connection.
The editor itself does not need the DLL to edit TXT files. Static MSVC runtime
is selected. The unsigned executable may show a Windows SmartScreen warning;
no security policy is changed by the application or build script.

## Use

The native Japanese UI has two tabs. The first provides 16 PSX input action
combos with all 131 non-reserved canonical `GpioAction` names. The second
provides a section/category tree, searchable multi-column property list and
typed string/number editor or bool/enum combo for the selected field.
Double-clicking a list row focuses its typed control. Enter a string without
TXT quotation marks. Click the value-apply button to stage a field change.

New creates a valid neutral example, not a dump of firmware defaults. Open
loads `.gp2040-psx.txt` / `.txt` only after full validation. Save As validates
the complete document before writing; there is no automatic save. Unsaved
changes prompt before close, New or Open. Editing source is metadata only;
the editor never selects a hardware profile. `Profile.label` is restricted by
the shared preset contract to ASCII letters/digits/spaces, at most 16 bytes.
Japanese text is supported in macro labels and Windows file paths.

All supported fields are editable, including shared ADDON settings, keyboard
HID mappings, three reactive LED channels, six macros and their per-input
`buttonMask`, `duration`, `waitDuration` tables. Durations are microseconds.
Set a macro's `inputCount` to create 0..30 input rows. Reducing it confirms
deletion of the trailing rows. Global conflicts may temporarily exist while
editing; full validation and Save reject them. Selecting a non-custom action
clears its custom masks so they cannot silently remain active.

Shared ADDON settings apply to every profile. GPIO/authentication/USB identity
fields are not in the allowlist and are rejected, not ignored. Use the separate
web configuration UI to import and explicitly apply the saved TXT to a device.

新規作成・保存はTXT v2です。v1読込は旧スキーマと完全一致する場合だけ許可し、
既存のマッピング・マクロ・設定を保持したままJOGCON設定を0/100/1で補完します。
v1にJOGCONキーがある場合や、v2に必須キーがない場合は拒否します。
「全設定」または「共有ADDON全項目」の汎用プロパティ一覧で
`jogconDeadzone`（整数0..32766、`jogconMax`未満）、`jogconMax`（整数1..32767）、
`jogconForcebackEnabled`（整数0/1、boolではない）を編集できます。
PC模擬モーターの一時状態はTXTに保存しません。

## Contract and Tests

Authoritative format: `../preset-format/SPECIFICATION.md` and
`../www/src/Services/PsxPreset.ts`. The native parser mirrors the v2 allowlist
and cross-field rules; it does not import TypeScript or require a browser at
build/runtime. New versions require explicitly updating the owned schema.

- UTF-8 with optional BOM, 64KiB maximum, `#` whole-line comments.
- Required sections/keys; duplicate, unknown, missing and malformed data reject.
- Quoted strings with JSON-style escapes (including valid surrogate pairs).
- Plain decimal and nonnegative `0x` hex numbers, no exponent syntax.
- Exact legacy v1 import with only three JOGCON defaults added; other versions reject.
- Decimal floats only for allowed fields; finite/range/exact integer checks.
- Canonical action names, reserved action rejection, mask and output conflicts.

Save emits normalized readable UTF-8 without BOM, decimal numbers, and a
standard explanatory comment. Input comment placement/order is not retained.
Writes use same-directory `.tmp`, `FlushFileBuffers`, then `MoveFileExW` with
replace/write-through. An existing `.tmp` is never overwritten. A failed write
leaves the original intact and reports an error.

`preset_tests.exe` runs portable parser/schema tests without hardware. CTest
also runs Windows atomic file tests (including Japanese paths and existing-temp
file protection) and a real-control GUI smoke. Portable CTest entries check both
the v2 shared sample and legacy v1 sample interoperability on every host.
`preset_tests.exe --roundtrip <TXT-path>` checks a shared sample without writes.
`preset_tests.exe --emit-default` prints a valid new document.
`psx_profile_editor.exe --smoke-ui` creates the real Win32 controls, checks action
combos and dynamic macro rows, validates a roundtrip, then closes with an exit
status. This is not a substitute for manual visual/accessibility testing.
