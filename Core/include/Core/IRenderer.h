#pragma once

#include "Core/Camera.h"
#include "Core/Color.h"

namespace Core
{
    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void DrawRect(
            const Camera& camera,
            float x,
            float y,
            float width,
            float height,
            Color color
        ) = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual void DrawLine(const Camera& camera, float x1, float y1, float x2, float y2, Color color) = 0;
    };
}
