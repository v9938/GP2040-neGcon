// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "psx_latency.h"
#include "hardware/gpio.h"

#if defined(PSX_LATENCY_TRACE)
static volatile bool pending = false;
static volatile bool queued = false;
#endif

void psx_latency_setup(void) {
#if defined(PSX_LATENCY_TRACE)
    // 計測ビルドのみD6(GPIO0)を使用。通常ビルドはUART TXを維持する。
    gpio_init(0);
    gpio_put(0, 0);
    gpio_set_dir(0, GPIO_OUT);
#endif
}
void psx_latency_received(void) {
#if defined(PSX_LATENCY_TRACE)
    if (!pending) { pending = true; queued = false; gpio_put(0, 1); }
#endif
}
void psx_latency_queued(void) {
#if defined(PSX_LATENCY_TRACE)
    if (pending) queued = true;
#endif
}
void psx_latency_usb_complete(unsigned char endpoint) {
#if defined(PSX_LATENCY_TRACE)
    // 送信予約ではなく、native USBのIN転送完了IRQで下げる。
    if (endpoint == 0x81 && pending && queued) {
        gpio_put(0, 0); queued = false; pending = false;
    }
#endif
}
