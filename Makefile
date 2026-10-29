SHELL := /bin/sh
.DEFAULT_GOAL := all
CMAKE ?= cmake
PYTHON ?= python3
POWERSHELL ?= pwsh
BUILD_DIR := build-release
TEST_BUILD_DIR := build-native-tests
JOBS ?= 1
GENERATOR ?= Ninja
RELEASE_VERSION := 0.7.12-neGcon
RELEASE_DATE ?= $(shell date +%Y%m%d)

.PHONY: all build web test clean release
all: build

build:
	@test ! -L $(BUILD_DIR) || { echo 'Refusing linked build output'; exit 1; }
	@test -f lib/ArduinoJson/CMakeLists.txt -a -f lib/httpd/fsdata.c -a -f lib/tinyusb/src/tusb.h -a -f lib/pico_pio_usb/CMakeLists.txt || { echo 'Complete vendored source and prebuilt web filesystem required'; exit 1; }
	@test -n "$$picotool_DIR" && test -f "$$picotool_DIR/picotoolConfig.cmake" || { echo 'Set picotool_DIR to a local picotool 2.1.1 CMake package'; exit 1; }
	SKIP_SUBMODULES=ON SKIP_WEBBUILD=ON PICOTOOL_FORCE_FETCH_FROM_GIT=OFF $(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=seeed_xiao_rp2040 -DGP2040_BOARDCONFIG=SeeedXIAORP2040 -DSKIP_SUBMODULES=ON -DSKIP_WEBBUILD=ON -DGP2040_USE_SYSTEM_PYTHON=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DPICO_SDK_FETCH_FROM_GIT=OFF -Dpicotool_DIR="$$picotool_DIR" -DPython3_EXECUTABLE="$(PYTHON)" -DPROJECT_RELEASE_VERSION=$(RELEASE_VERSION) -DGIT_REPO_BUILD_ID=$(RELEASE_DATE)
	$(CMAKE) --build $(BUILD_DIR) --config Release --parallel $(JOBS)

# Network access and web source regeneration are explicit, not prerequisites of build.
web:
	cd www && npm ci && npm run build

test:
	@test ! -L $(TEST_BUILD_DIR) || { echo 'Refusing linked native test output'; exit 1; }
	$(CMAKE) -S pc-profile-editor -B $(TEST_BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(TEST_BUILD_DIR) --config Release --parallel $(JOBS)
	ctest --test-dir $(TEST_BUILD_DIR) -C Release --output-on-failure
	$(PYTHON) -B -m unittest discover -s scripts -p test_psx_negcon_additive.py

clean:
	$(POWERSHELL) -NoProfile -File ./build-release.ps1 -Clean
	@test ! -L $(TEST_BUILD_DIR) || { echo 'Refusing linked native test output'; exit 1; }
	$(CMAKE) -E rm -rf $(TEST_BUILD_DIR)

release: build
	$(CMAKE) -E copy "$(BUILD_DIR)/GP2040-CE_0.7.12_SeeedXIAORP2040.uf2" "$(BUILD_DIR)/GP2040-CE_$(RELEASE_VERSION)_SeeedXIAORP2040_$(RELEASE_DATE).uf2"
