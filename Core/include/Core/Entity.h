#pragma once
#include "Core/Color.h"

namespace Core {
    struct Entity {
        float x, y;
        float width, height;
        Color color;

        Entity(float x, float y, float w, float h, Color c)
            : x(x), y(y), width(w), height(h), color(c) {}
    };
}