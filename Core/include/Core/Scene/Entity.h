#pragma once

#include "EntityId.h"
#include "Transform.h"
#include "Core/Color.h"

namespace Core {

    struct Entity {
        EntityId id{};
        Transform transform{};

        // Временные параметры 2D-объекта.
        float width = 1.0f;
        float height = 1.0f;

        Color color{ 255, 255, 255 };

        Entity() = default;

        bool IsValid() const {
            return id.IsValid();
        }

        bool ContainsPoint(float worldX, float worldY) const {
            const float x = transform.position.x;
            const float y = transform.position.y;

            return worldX >= x && worldX <= x + width &&
                worldY >= y && worldY <= y + height;
        }
    };

}