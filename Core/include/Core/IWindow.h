#pragma once

#include <string>

namespace Core
{
    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        virtual bool ShouldClose() const = 0;
        virtual void PollEvents() = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
    };
}
