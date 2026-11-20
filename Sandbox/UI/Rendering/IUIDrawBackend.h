#pragma once

#include "UIDrawList.h"

namespace Kjarn::UI
{
    class IUIDrawBackend
    {
    public:
        virtual ~IUIDrawBackend() = default;

        IUIDrawBackend(const IUIDrawBackend&) = delete;
        IUIDrawBackend& operator=(const IUIDrawBackend&) = delete;

        IUIDrawBackend() = default;

        virtual void Submit(const UIDrawList& drawList) = 0;
    };
}