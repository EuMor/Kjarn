
#include "Core/Renderer.h"

namespace Core {

    Renderer::Renderer(HWND hwnd)
        : m_hwnd(hwnd) {
    }

    Renderer::~Renderer() {
        ReleaseBackBuffer();
    }

    void Renderer::ReleaseBackBuffer() {
        m_frameReady = false;

        if (m_memDC != nullptr) {
            if (m_oldBitmap != nullptr) {
                SelectObject(m_memDC, m_oldBitmap);
            }

            if (m_memBitmap != nullptr) {
                DeleteObject(m_memBitmap);
            }

            DeleteDC(m_memDC);
        }

        m_memDC = nullptr;
        m_memBitmap = nullptr;
        m_oldBitmap = nullptr;
    }

    void Renderer::BeginFrame() {
        m_frameReady = false;

        if (m_hwnd == nullptr) {
            return;
        }

        RECT rc{};

        if (!GetClientRect(m_hwnd, &rc)) {
            return;
        }

        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;

        // Окно может быть свёрнуто или иметь нулевой размер.
        if (width <= 0 || height <= 0) {
            m_width = width;
            m_height = height;
            ReleaseBackBuffer();
            return;
        }

        const bool resizeRequired =
            width != m_width ||
            height != m_height ||
            m_memDC == nullptr ||
            m_memBitmap == nullptr;

        if (resizeRequired) {
            ReleaseBackBuffer();

            HDC windowDC = GetDC(m_hwnd);

            if (windowDC == nullptr) {
                return;
            }

            HDC newDC = CreateCompatibleDC(windowDC);

            if (newDC == nullptr) {
                ReleaseDC(m_hwnd, windowDC);
                return;
            }

            HBITMAP newBitmap =
                CreateCompatibleBitmap(windowDC, width, height);

            if (newBitmap == nullptr) {
                DeleteDC(newDC);
                ReleaseDC(m_hwnd, windowDC);
                return;
            }

            HGDIOBJ previousObject =
                SelectObject(newDC, newBitmap);

            if (previousObject == nullptr ||
                previousObject == HGDI_ERROR) {

                DeleteObject(newBitmap);
                DeleteDC(newDC);
                ReleaseDC(m_hwnd, windowDC);
                return;
            }

            m_memDC = newDC;
            m_memBitmap = newBitmap;
            m_oldBitmap = static_cast<HBITMAP>(previousObject);

            m_width = width;
            m_height = height;

            ReleaseDC(m_hwnd, windowDC);
        }

        HBRUSH backgroundBrush =
            CreateSolidBrush(RGB(40, 44, 52));

        if (backgroundBrush == nullptr) {
            return;
        }

        RECT fullRect{ 0, 0, m_width, m_height };

        const int fillResult =
            FillRect(m_memDC, &fullRect, backgroundBrush);

        DeleteObject(backgroundBrush);

        if (fillResult == 0) {
            return;
        }

        m_frameReady = true;
    }

    void Renderer::DrawRect(
        const Camera& camera,
        float x,
        float y,
        float width,
        float height,
        Color color
    ) {
        if (!m_frameReady || width <= 0.0f || height <= 0.0f) {
            return;
        }

        float screenX = 0.0f;
        float screenY = 0.0f;

        camera.WorldToScreen(
            x,
            y,
            static_cast<float>(m_width),
            static_cast<float>(m_height),
            screenX,
            screenY
        );

        const float screenW = width * camera.zoom;
        const float screenH = height * camera.zoom;

        HBRUSH brush =
            CreateSolidBrush(RGB(color.r, color.g, color.b));

        if (brush == nullptr) {
            return;
        }

        RECT rect{
            static_cast<LONG>(screenX),
            static_cast<LONG>(screenY),
            static_cast<LONG>(screenX + screenW),
            static_cast<LONG>(screenY + screenH)
        };

        FillRect(m_memDC, &rect, brush);

        DeleteObject(brush);
    }

    void Renderer::EndFrame() {
        if (!m_frameReady || m_hwnd == nullptr || m_memDC == nullptr) {
            return;
        }

        HDC windowDC = GetDC(m_hwnd);

        if (windowDC == nullptr) {
            m_frameReady = false;
            return;
        }

        BitBlt(
            windowDC,
            0,
            0,
            m_width,
            m_height,
            m_memDC,
            0,
            0,
            SRCCOPY
        );

        ReleaseDC(m_hwnd, windowDC);

        m_frameReady = false;
    }

}
