
#pragma once

#include "Core/IRenderer.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Renderer::Win32
{
    class GdiRenderer final : public Core::IRenderer
    {
    public:
        explicit GdiRenderer(HWND windowHandle);
        ~GdiRenderer() override;

        GdiRenderer(const GdiRenderer&) = delete;
        GdiRenderer& operator=(const GdiRenderer&) = delete;

        void BeginFrame() override;
        void EndFrame() override;

        void DrawRect(
            const Core::Camera& camera,
            float x,
            float y,
            float width,
            float height,
            Core::Color color
        ) override;

        int GetWidth() const override;
        int GetHeight() const override;

        HDC GetFrameDeviceContext() const;

    private:
        void ReleaseBackBuffer();

        HWND m_hwnd = nullptr;

        HDC m_memDC = nullptr;
        HBITMAP m_memBitmap = nullptr;
        HBITMAP m_oldBitmap = nullptr;

        int m_width = 0;
        int m_height = 0;

        bool m_frameReady = false;
    };
}
