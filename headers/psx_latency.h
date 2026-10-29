// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#ifndef PSX_LATENCY_H
#define PSX_LATENCY_H
#ifdef __cplusplus
extern "C" {
#endif
void psx_latency_setup(void);
void psx_latency_received(void);
void psx_latency_queued(void);
void psx_latency_usb_complete(unsigned char endpoint);
#ifdef __cplusplus
}
#endif
#endif
