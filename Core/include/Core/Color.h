#pragma once
#include <cstdint>

namespace Core {
    struct Color {
        uint8_t r, g, b;

        Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    };
}