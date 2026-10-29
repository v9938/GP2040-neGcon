/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "enums.pb.h"
#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "Seeed XIAO-RP2040"

// 08..23 は物理 GPIO ではなく PSX 入力の仮想 slot。
#define GPIO_PIN_08 GpioAction::BUTTON_PRESS_UP
#define GPIO_PIN_09 GpioAction::BUTTON_PRESS_DOWN
#define GPIO_PIN_10 GpioAction::BUTTON_PRESS_RIGHT
#define GPIO_PIN_11 GpioAction::BUTTON_PRESS_LEFT
#define GPIO_PIN_12 GpioAction::BUTTON_PRESS_B1
#define GPIO_PIN_13 GpioAction::BUTTON_PRESS_B2
#define GPIO_PIN_14 GpioAction::BUTTON_PRESS_R2
#define GPIO_PIN_15 GpioAction::BUTTON_PRESS_L2
#define GPIO_PIN_16 GpioAction::BUTTON_PRESS_B3
#define GPIO_PIN_17 GpioAction::BUTTON_PRESS_B4
#define GPIO_PIN_18 GpioAction::BUTTON_PRESS_R1
#define GPIO_PIN_19 GpioAction::BUTTON_PRESS_L1
#define GPIO_PIN_20 GpioAction::BUTTON_PRESS_S1
#define GPIO_PIN_21 GpioAction::BUTTON_PRESS_S2
#define GPIO_PIN_22 GpioAction::BUTTON_PRESS_L3
#define GPIO_PIN_23 GpioAction::BUTTON_PRESS_R3

// アナログ値は PSX から取得し、物理 ADC は使用しない。
#define ANALOG_INPUT_ENABLED 1
#define ANALOG_ADC_1_VRX -1
#define ANALOG_ADC_1_VRY -1
#define ANALOG_ADC_2_VRX -1
#define ANALOG_ADC_2_VRY -1
#define ANALOG_ADC_1_MODE DPAD_MODE_LEFT_ANALOG
#define ANALOG_ADC_2_MODE DPAD_MODE_RIGHT_ANALOG
#define TURBO_ENABLED 0
#define I2C0_ENABLED 0
#define I2C1_ENABLED 0
#define SPI0_ENABLED 0
#define SPI1_ENABLED 0

// 基板 RGB は R=17/G=16/B=25 の active-low、起動色は赤。
#define BOARD_LED_ENABLED 1
#define BOARD_LED_TYPE ON_BOARD_LED_MODE_MODE_INDICATOR
#define REACTIVE_LED_ENABLED 1
// WS2813 一個の R/G/B。data=12、電源 enable=11 はドライバで固定する。
#define REACTIVE_LED_COUNT 3

// USB ホストは D+=6、D-=7 に固定。UART0 TX=0 は親側で初期化する。
#define USB_PERIPHERAL_ENABLED 1
#define USB_PERIPHERAL_PIN_DPLUS 6
#define USB_PERIPHERAL_PIN_ORDER 0
#define USB_PERIPHERAL_PIN_5V -1

// Keyboard Mapping Configuration
#define GAMEPAD_USB_HOST_ENABLED 1
//                                            // GP2040 | Xinput | Switch  | PS3/4/5  | Dinput | Arcade |
#define KEY_DPAD_UP     HID_KEY_ARROW_UP      // UP     | UP     | UP      | UP       | UP     | UP     |
#define KEY_DPAD_DOWN   HID_KEY_ARROW_DOWN    // DOWN   | DOWN   | DOWN    | DOWN     | DOWN   | DOWN   |
#define KEY_DPAD_RIGHT  HID_KEY_ARROW_RIGHT   // RIGHT  | RIGHT  | RIGHT   | RIGHT    | RIGHT  | RIGHT  |
#define KEY_DPAD_LEFT   HID_KEY_ARROW_LEFT    // LEFT   | LEFT   | LEFT    | LEFT     | LEFT   | LEFT   |
#define KEY_BUTTON_B1   HID_KEY_SHIFT_LEFT    // B1     | A      | B       | Cross    | 2      | K1     |
#define KEY_BUTTON_B2   HID_KEY_Z             // B2     | B      | A       | Circle   | 3      | K2     |
#define KEY_BUTTON_R2   HID_KEY_X             // R2     | RT     | ZR      | R2       | 8      | K3     |
#define KEY_BUTTON_L2   HID_KEY_V             // L2     | LT     | ZL      | L2       | 7      | K4     |
#define KEY_BUTTON_B3   HID_KEY_CONTROL_LEFT  // B3     | X      | Y       | Square   | 1      | P1     |
#define KEY_BUTTON_B4   HID_KEY_ALT_LEFT      // B4     | Y      | X       | Triangle | 4      | P2     |
#define KEY_BUTTON_R1   HID_KEY_SPACE         // R1     | RB     | R       | R1       | 6      | P3     |
#define KEY_BUTTON_L1   HID_KEY_C             // L1     | LB     | L       | L1       | 5      | P4     |
#define KEY_BUTTON_S1   HID_KEY_5             // S1     | Back   | Minus   | Select   | 9      | Coin   |
#define KEY_BUTTON_S2   HID_KEY_1             // S2     | Start  | Plus    | Start    | 10     | Start  |
#define KEY_BUTTON_L3   HID_KEY_EQUAL         // L3     | LS     | LS      | L3       | 11     | LS     |
#define KEY_BUTTON_R3   HID_KEY_MINUS         // R3     | RS     | RS      | R3       | 12     | RS     |
#define KEY_BUTTON_A1   HID_KEY_9             // A1     | Guide  | Home    | PS       | 13     | ~      |
#define KEY_BUTTON_A2   HID_KEY_F2            // A2     | ~      | Capture | ~        | 14     | ~      |
#define KEY_BUTTON_FN   -1                    // Hotkey Function                                        |

#endif
