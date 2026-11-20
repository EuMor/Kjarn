#pragma once
#include "Color.h"

namespace Core {
    struct Entity {
        float x, y;
        float width, height;
        Color color;

        Entity(float x, float y, float w, float h, Color c)
            : x(x), y(y), width(w), height(h), color(c) {}

        bool ContainsPoint(float worldX, float worldY) const {
            return worldX >= x && worldX <= x + width &&
                worldY >= y && worldY <= y + height;
        }
    };
}