// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#ifndef XIAO_LEDS_H
#define XIAO_LEDS_H

#include <cstdint>

constexpr uint8_t XIAO_LED_RED_PIN = 17;
constexpr uint8_t XIAO_LED_GREEN_PIN = 16;
constexpr uint8_t XIAO_LED_BLUE_PIN = 25;
constexpr uint8_t XIAO_PIXEL_DATA_PIN = 12;
constexpr uint8_t XIAO_PIXEL_ENABLE_PIN = 11;

// core1の出力値と確保したSMをGDBから確認できるようにする。
extern volatile uint8_t xiaoReactiveRgb[3];
extern volatile int xiaoNeoPixelStateMachine;
// 送信要求値とは別に、FIFOへ投入済みのRGBと累積フレーム数を公開する。
extern volatile uint8_t xiao_neopixel_last_rgb[3];
extern volatile uint32_t xiao_neopixel_frames;
extern volatile bool xiao_neopixel_ready;

void xiaoBoardLedStartup();
void xiaoBoardLedSet(bool red, bool green, bool blue);
void xiaoNeoPixelInitialize(bool enabled);
void xiaoNeoPixelSet(uint8_t red, uint8_t green, uint8_t blue);
void xiaoNeoPixelProcess();

#endif
