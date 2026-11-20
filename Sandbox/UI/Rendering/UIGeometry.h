#pragma once

#include <cstdint>

namespace Kjarn::UI
{
    struct UIVec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct UIRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct UIColor
    {
        std::uint8_t r = 255;
        std::uint8_t g = 255;
        std::uint8_t b = 255;
        std::uint8_t a = 255;
    };
}