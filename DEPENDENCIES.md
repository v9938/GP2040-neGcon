# Dependencies

The project license is retained in LICENSE. Third-party sources keep their original notices; this package does not relicense them.
Vendored snapshots are included without Git metadata. A local commit identifies a snapshot, not proof that it is available from its public upstream.

| Source directory | Public upstream | Local snapshot commit |
| --- | --- | --- |
| lib/tinyusb | https://github.com/OpenStickCommunity/tinyusb | 5aede3143927f6d75a47eb720f4c188bdefc0a0c |
| lib/pico_pio_usb | https://github.com/OpenStickCommunity/Pico-PIO-USB | 37965f8895fffb4ffacace86e1f731f908dc18e0 |
| lib/ArduinoJson | https://github.com/bblanchon/ArduinoJson/tree/v6.21.2 | 420221f0785fe8321fb7f449453afecaf73af7b0 |

Other included library sources: CRC32, FlashPROM, httpd, lwip-port, nanopb, PicoPeripherals and rndis. Original license/copyright notices remain in their respective files.
External build dependency: Raspberry Pi Pico SDK 2.1.1 or later (https://github.com/raspberrypi/pico-sdk), including its SDK dependencies. The SDK, compilers, CMake, Python and Node.js are not bundled.

Included license/notice files:
- `headers/drivers/xboxog/xid/LICENSE`
- `lib/ArduinoJson/LICENSE.txt`
- `lib/nanopb/LICENSE.txt`
- `lib/pico_pio_usb/LICENSE`
- `lib/tinyusb/hw/bsp/espressif/components/led_strip/LICENSE`
- `lib/tinyusb/lib/fatfs/LICENSE.txt`
- `lib/tinyusb/lib/SEGGER_RTT/LICENSE.md`
- `lib/tinyusb/LICENSE`
- `LICENSE`
