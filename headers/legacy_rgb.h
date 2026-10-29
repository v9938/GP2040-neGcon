#ifndef LEGACY_RGB_H_
#define LEGACY_RGB_H_

#include <stdint.h>

// Values and four-byte layout are required by pre-protobuf flash migration.
enum LEDFormat {
    LED_FORMAT_GRB = 0,
    LED_FORMAT_RGB = 1,
    LED_FORMAT_GRBW = 2,
    LED_FORMAT_RGBW = 3,
};

struct RGB {
    RGB() = default;
    constexpr RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b), w(0) {}
    RGB(uint32_t c) : r((c >> 16) & 255), g((c >> 8) & 255), b(c & 255), w(0) {}
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    uint32_t value(LEDFormat format) const {
        if (format == LED_FORMAT_RGB) return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
        if (format == LED_FORMAT_GRB) return (uint32_t(g) << 16) | (uint32_t(r) << 8) | b;
        if (r == g && r == b) return r;
        return format == LED_FORMAT_RGBW
            ? (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | w
            : (uint32_t(g) << 24) | (uint32_t(r) << 16) | (uint32_t(b) << 8) | w;
    }
};

constexpr RGB ColorWhite(255, 255, 255);
constexpr RGB ColorRed(255, 0, 0);
static_assert(sizeof(RGB) == 4, "Legacy flash RGB layout must remain four bytes");

#endif
