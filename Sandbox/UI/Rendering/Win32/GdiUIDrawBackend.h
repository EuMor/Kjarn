#pragma once

#include "Sandbox/UI/Rendering/IUIDrawBackend.h"
#include "Renderer/Win32/GdiRenderer.h"

namespace Kjarn::UI::Win32
{
    class GdiUIDrawBackend final : public IUIDrawBackend
    {
    public:
        explicit GdiUIDrawBackend(
            Renderer::Win32::GdiRenderer& renderer
        );

        void Submit(const UIDrawList& drawList) override;

    private:
        Renderer::Win32::GdiRenderer& m_renderer;
    };
}