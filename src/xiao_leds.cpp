// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "xiao_leds.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#include <initializer_list>

volatile uint8_t xiaoReactiveRgb[3] = {0, 0, 0};
volatile int xiaoNeoPixelStateMachine = -1;
volatile uint8_t xiao_neopixel_last_rgb[3] = {0, 0, 0};
volatile uint32_t xiao_neopixel_frames = 0;
volatile bool xiao_neopixel_ready = false;

namespace {
bool pixelEnabled = false;
bool pixelPowered = false;
bool clearPending = false;
uint32_t lastPixel = 0;
uint64_t nextFrame = 0;
}

void xiaoBoardLedSet(bool red, bool green, bool blue) {
    gpio_put(XIAO_LED_RED_PIN, !red);
    gpio_put(XIAO_LED_GREEN_PIN, !green);
    gpio_put(XIAO_LED_BLUE_PIN, !blue);
}

void xiaoBoardLedStartup() {
    // Storageのロードより先に出力ラッチを設定する。基板RGBはactive-low。
    for (uint8_t pin : {XIAO_LED_RED_PIN, XIAO_LED_GREEN_PIN, XIAO_LED_BLUE_PIN}) {
        gpio_init(pin);
        gpio_put(pin, pin != XIAO_LED_RED_PIN);
        gpio_set_dir(pin, GPIO_OUT);
    }
}

void xiaoNeoPixelInitialize(bool enabled) {
    xiao_neopixel_ready = false;
    pixelEnabled = enabled;
    xiaoNeoPixelSet(0, 0, 0);
    if (!pixelPowered) {
        // 電源を先に設定する。無効時はデータもLowにして逆給電を防ぐ。
        gpio_init(XIAO_PIXEL_ENABLE_PIN);
        gpio_put(XIAO_PIXEL_ENABLE_PIN, enabled);
        gpio_set_dir(XIAO_PIXEL_ENABLE_PIN, GPIO_OUT);
        if (xiaoNeoPixelStateMachine < 0) {
            gpio_init(XIAO_PIXEL_DATA_PIN);
            gpio_put(XIAO_PIXEL_DATA_PIN, false);
            gpio_set_dir(XIAO_PIXEL_DATA_PIN, GPIO_OUT);
        }
        if (!enabled) return;
        pixelPowered = true;
    }

    if (enabled && xiaoNeoPixelStateMachine < 0) {
        // USBのPIO0 SM1とPIO1には触れず、空きSMと命令領域だけを使う。
        if (!pio_can_add_program(pio0, &ws2812_program)) {
            gpio_put(XIAO_PIXEL_ENABLE_PIN, false);
            pixelPowered = false;
            return;
        }
        // USBが未初期化でもSM1を選ばない。予約はclaim管理だけでハードには触れない。
        const bool reserveUsbSm = !pio_sm_is_claimed(pio0, 1);
        if (reserveUsbSm) pio_sm_claim(pio0, 1);
        const int sm = pio_claim_unused_sm(pio0, false);
        if (reserveUsbSm) pio_sm_unclaim(pio0, 1);
        if (sm < 0) {
            gpio_put(XIAO_PIXEL_ENABLE_PIN, false);
            pixelPowered = false;
            return;
        }
        const uint offset = pio_add_program(pio0, &ws2812_program);
        ws2812_program_init(pio0, sm, offset, XIAO_PIXEL_DATA_PIN, 800000, false);
        xiaoNeoPixelStateMachine = sm;
    }
    xiao_neopixel_ready = enabled && pixelPowered && xiaoNeoPixelStateMachine >= 0;
    clearPending = true;
    // 24bit転送は30us。初回も300us以上Lowを保ち、待機はしない。
    const uint64_t earliest = time_us_64() + 330;
    if (nextFrame < earliest) nextFrame = earliest;
}

void xiaoNeoPixelSet(uint8_t red, uint8_t green, uint8_t blue) {
    xiaoReactiveRgb[0] = red;
    xiaoReactiveRgb[1] = green;
    xiaoReactiveRgb[2] = blue;
}

void xiaoNeoPixelProcess() {
    if (!pixelPowered || xiaoNeoPixelStateMachine < 0) return;
    const uint64_t now = time_us_64();
    if (now < nextFrame) return;
    const uint32_t pixel = pixelEnabled && !clearPending
        ? (uint32_t(xiaoReactiveRgb[1]) << 16) | (uint32_t(xiaoReactiveRgb[0]) << 8) | xiaoReactiveRgb[2]
        : 0;
    if (clearPending || pixel != lastPixel) {
        if (pio_sm_is_tx_fifo_full(pio0, xiaoNeoPixelStateMachine)) return;
        // GRBをMSBから24bit送信。FIFO投入だけなのでUSB処理を止めない。
        pio_sm_put(pio0, xiaoNeoPixelStateMachine, pixel << 8);
        // 実際のFIFO投入時だけ更新し、初期クリア/無効化の黒も数える。
        xiao_neopixel_last_rgb[0] = (pixel >> 8) & 0xff;
        xiao_neopixel_last_rgb[1] = (pixel >> 16) & 0xff;
        xiao_neopixel_last_rgb[2] = pixel & 0xff;
        xiao_neopixel_frames++;
        lastPixel = pixel;
        clearPending = false;
        nextFrame = now + 1000;
        return;
    }
    if (!pixelEnabled) {
        // 黒の転送とラッチ完了後に電源を落とす。
        gpio_put(XIAO_PIXEL_ENABLE_PIN, false);
        pixelPowered = false;
    }
}
