# neGcon Source Build

This guide applies to the complete GP2040-CE `0.7.12-neGcon` source distribution, including vendored libraries and `lib/httpd/fsdata.c`. The CMake project version remains numeric `0.7.12`. A source archive must work without `.git`; Git is not required to build the distribution. Keep the original `README.md` and root `LICENSE` unchanged alongside `README-neGcon.md`.

## External Tools

- CMake on `PATH` (or `-CMakeExecutable`), Ninja recommended, and Python 3 with `venv` support.
- Raspberry Pi Pico SDK **exactly 2.1.1**, outside this source tree. Set `PICO_SDK_PATH` or pass `-PicoSdkPath`. The wrapper also checks `$HOME/.pico-sdk/sdk/2.1.1` (`$env:USERPROFILE` on Windows).
- Arm GNU Toolchain **14_2_Rel1 / 14.2.Rel1**. Put `arm-none-eabi-gcc` on `PATH`, set `SDK_TOOLCHAIN`/`PICO_TOOLCHAIN_PATH`, or pass `-ToolchainPath` (installation root or `bin`). Optional Pico extension tool directories are added after existing `PATH` tools; no individual user's path is embedded.
- Python packages from `lib/nanopb/extra/requirements.txt`: `protobuf>=3.19`, `grpcio-tools==1.71.0`, `setuptools<81`. Existing compatible environments work; a clean environment needs these installed before protobuf generation.
- SDK subdependencies must already be present, including its required submodules. Install picotool 2.1.1 locally and pass `-PicotoolDir` or set `picotool_DIR` to the directory containing `picotoolConfig.cmake`. The wrapper also checks `$HOME/.pico-sdk/picotool/2.1.1/picotool`. A binary on `PATH` alone is insufficient for the SDK's CMake package lookup; the wrapper refuses to invoke the SDK's automatic picotool download fallback.

The SDK is not bundled. Obtain it explicitly from the [official Pico SDK release](https://github.com/raspberrypi/pico-sdk/releases/tag/2.1.1). A recursive Git checkout is preferable because GitHub's SDK source ZIP does not contain submodule contents:

```sh
git clone --branch 2.1.1 --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git pico-sdk-2.1.1
```

This is a user-run dependency download, not an automatic release-build step. See the [official toolchain downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) for the pinned Arm release. Missing vendored libraries are an incomplete firmware source distribution, not a reason for the build to fetch newer dependencies.

## Windows

Run from the source root, using PowerShell 7 or Windows PowerShell via the CMD wrapper:

```powershell
$env:PICO_SDK_PATH = '<external-sdk-directory>'
$env:SDK_TOOLCHAIN = '<arm-gnu-toolchain-directory>'
.\build-release.cmd -PythonExecutable '<python-interpreter>'
```

`-PythonExecutable` takes precedence over `Python3_EXECUTABLE`, then `PYTHON_EXECUTABLE`, then `python3`/`python` on `PATH`. A cached `build-release/venv` takes precedence for protobuf generation after interpreter selection. If Python requirements are absent, prepare that local environment explicitly:

```powershell
.\build-release.cmd -InstallPythonRequirements
```

Only that opt-in invokes pip; normal builds do not install packages into the global Python environment. A fresh installation may require internet access. TLS failures must be fixed using a valid CA/proxy configuration or pre-downloaded wheels, not `--trusted-host`, disabled certificate verification, or private machine paths. Offline preparation can use:

```powershell
python -m venv build-release/venv
build-release/venv/Scripts/python.exe -m pip install --no-index --find-links '<wheel-directory>' -r lib/nanopb/extra/requirements.txt
```

The wrapper configures `-S <source-root> -B <source-root>/build-release`, `Release`, `SKIP_SUBMODULES=ON`, and `SKIP_WEBBUILD=ON`, with dependency fetching disabled. It uses Ninja when available, otherwise suitable Make tools, or accepts `-Generator`. Default compilation concurrency is one; `-Jobs` is explicit. SDK/toolchain/generator changes should use a fresh build directory.

## Web Regeneration

The full source release includes the prebuilt web filesystem. Node.js/npm are unnecessary for the default firmware build. Only regenerate the web content when explicitly requested:

```powershell
.\build-release.cmd -RebuildWeb
```

This runs `npm ci` and `npm run build` in `www`, changing generated web content. It can require internet access for locked dependencies. No release build or flashing is performed just by reading this guide.

## POSIX / GNU Make

GNU Make uses a POSIX shell, including Git Bash on Windows. Native `cmd.exe` users should use `build-release.cmd` instead. Set the SDK environment and put the pinned compiler on `PATH`:

```sh
export PICO_SDK_PATH='<external-sdk-directory>'
export PICO_TOOLCHAIN_PATH='<toolchain-bin-directory>'
export picotool_DIR='<directory-containing-picotoolConfig.cmake>'
export PATH="$PICO_TOOLCHAIN_PATH:$PATH"
make release CMAKE=cmake PYTHON='<prepared-python-interpreter>'
make web
make test
```

`all`/`build` do not regenerate the web UI. `web` explicitly uses npm. `test` builds native parser/shared-math tests separately in `build-native-tests` and runs hardware-free Python neGcon contracts; native DLL tests require the explicit opt-in described by their test file. `GENERATOR`, `JOBS`, and `RELEASE_DATE` are overridable. `make clean` requires PowerShell (`POWERSHELL=pwsh` by default), removes only named build output directories, and must not be used with linked output directories. The wrapper rejects linked `build-release` directories for both building and cleaning. Neither clean target removes source, vendored libraries, or any external distribution destination.

## Artifacts And Date

The wrapper places the release UF2 only in `build-release`:

```text
GP2040-CE_0.7.12-neGcon_SeeedXIAORP2040_yyyyMMdd.uf2
```

The release date defaults to the **actual local OS date at script execution**, using `Get-Date -Format yyyyMMdd`; it is not taken from a chat timestamp or written as a fixed source date. The OS date observed while preparing these scripts was `20261029`. Explicit dated builds use `-ReleaseDate 20261029` (or `make RELEASE_DATE=20261029`); this value also supplies `GIT_REPO_BUILD_ID`. Select a date truthfully for the release being produced.

Only the UF2 is copied to the dated filename. Intermediate ELF/map/protobuf/cache/venv files stay in the build directory and are not firmware distribution artifacts. No flashing, Git commit/push, archive creation, or copying to an external distribution destination is performed. `Release` avoids debug-build DWARF as the default, but no bit-for-bit reproducibility or comprehensive binary privacy audit is claimed.

## Optional Native Editor

The Windows native profile editor is a separate C++17 build, not a firmware prerequisite. From a suitable compiler environment:

```sh
cmake -S pc-profile-editor -B build-native-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-native-tests --config Release --parallel 1
ctest --test-dir build-native-tests -C Release --output-on-failure
```

Keep TXT preset version 3 and the matching analog-preview ABI version 3 together. Native tests and PC simulations do not establish hardware behavior, actual USB timing, or successful flashing of the latest firmware.
