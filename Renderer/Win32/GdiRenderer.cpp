
#include "Renderer/Win32/GdiRenderer.h"

namespace Renderer::Win32
{
    GdiRenderer::GdiRenderer(HWND windowHandle)
        : m_hwnd(windowHandle)
    {
    }

    GdiRenderer::~GdiRenderer()
    {
        ReleaseBackBuffer();
    }

    void GdiRenderer::ReleaseBackBuffer()
    {
        m_frameReady = false;

        if (m_memDC != nullptr)
        {
            if (m_oldBitmap != nullptr)
                SelectObject(m_memDC, m_oldBitmap);

            if (m_memBitmap != nullptr)
                DeleteObject(m_memBitmap);

            DeleteDC(m_memDC);
        }

        m_memDC = nullptr;
        m_memBitmap = nullptr;
        m_oldBitmap = nullptr;
    }

    void GdiRenderer::BeginFrame()
    {
        m_frameReady = false;

        if (m_hwnd == nullptr)
            return;

        RECT rc{};

        if (!GetClientRect(m_hwnd, &rc))
            return;

        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;

        if (width <= 0 || height <= 0)
        {
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

        if (resizeRequired)
        {
            ReleaseBackBuffer();

            HDC windowDC = GetDC(m_hwnd);

            if (windowDC == nullptr)
                return;

            HDC newDC = CreateCompatibleDC(windowDC);

            if (newDC == nullptr)
            {
                ReleaseDC(m_hwnd, windowDC);
                return;
            }

            HBITMAP newBitmap =
                CreateCompatibleBitmap(windowDC, width, height);

            if (newBitmap == nullptr)
            {
                DeleteDC(newDC);
                ReleaseDC(m_hwnd, windowDC);
                return;
            }

            HGDIOBJ previousObject =
                SelectObject(newDC, newBitmap);

            if (previousObject == nullptr ||
                previousObject == HGDI_ERROR)
            {
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

        if (backgroundBrush == nullptr)
            return;

        RECT fullRect{ 0, 0, m_width, m_height };

        const int fillResult =
            FillRect(m_memDC, &fullRect, backgroundBrush);

        DeleteObject(backgroundBrush);

        if (fillResult == 0)
            return;

        m_frameReady = true;
    }

    void GdiRenderer::DrawRect(
        const Core::Camera& camera,
        float x,
        float y,
        float width,
        float height,
        Core::Color color
    )
    {
        if (!m_frameReady || width <= 0.0f || height <= 0.0f)
            return;

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

        if (brush == nullptr)
            return;

        RECT rect{
            static_cast<LONG>(screenX),
            static_cast<LONG>(screenY),
            static_cast<LONG>(screenX + screenW),
            static_cast<LONG>(screenY + screenH)
        };

        FillRect(m_memDC, &rect, brush);

        DeleteObject(brush);
    }

    void GdiRenderer::DrawCircle(const Core::Camera& camera, float x, float y, float width, float height, Core::Color color) {
        float screenX, screenY;
        camera.WorldToScreen(x, y, (float)m_width, (float)m_height, screenX, screenY);
        float w = width * camera.zoom;
        float h = height * camera.zoom;

        HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
        HBRUSH oldBrush = (HBRUSH)SelectObject(m_memDC, brush);

        // Рисуем круг без обводки (используем NULL_PEN для скорости)
        HPEN pen = (HPEN)GetStockObject(NULL_PEN);
        HPEN oldPen = (HPEN)SelectObject(m_memDC, pen);

        Ellipse(m_memDC, (int)screenX, (int)screenY, (int)(screenX + w), (int)(screenY + h));

        SelectObject(m_memDC, oldBrush);
        SelectObject(m_memDC, oldPen);
        DeleteObject(brush);
    }

    void GdiRenderer::DrawTriangle(const Core::Camera& camera, float x, float y, float width, float height, Core::Color color) {
        float screenX, screenY;
        camera.WorldToScreen(x, y, (float)m_width, (float)m_height, screenX, screenY);
        float w = width * camera.zoom;
        float h = height * camera.zoom;

        HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
        HBRUSH oldBrush = (HBRUSH)SelectObject(m_memDC, brush);
        HPEN pen = (HPEN)GetStockObject(NULL_PEN);
        HPEN oldPen = (HPEN)SelectObject(m_memDC, pen);

        // Треугольник смотрящий вверх: вершина сверху по центру, основание снизу
        POINT points[3];
        points[0] = { (LONG)(screenX + w / 2.0f), (LONG)screenY };               // Верхушка
        points[1] = { (LONG)screenX,              (LONG)(screenY + h) };          // Левый нижний
        points[2] = { (LONG)(screenX + w),        (LONG)(screenY + h) };          // Правый нижний

        Polygon(m_memDC, points, 3);

        SelectObject(m_memDC, oldBrush);
        SelectObject(m_memDC, oldPen);
        DeleteObject(brush);
    }

    void GdiRenderer::EndFrame()
    {
        if (!m_frameReady ||
            m_hwnd == nullptr ||
            m_memDC == nullptr)
        {
            return;
        }

        HDC windowDC = GetDC(m_hwnd);

        if (windowDC == nullptr)
        {
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

    void GdiRenderer::DrawLine(const Core::Camera& camera, float x1, float y1, float x2, float y2, Core::Color color) {
        float sx1, sy1, sx2, sy2;
        camera.WorldToScreen(x1, y1, (float)m_width, (float)m_height, sx1, sy1);
        camera.WorldToScreen(x2, y2, (float)m_width, (float)m_height, sx2, sy2);

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(color.r, color.g, color.b));
        HPEN oldPen = (HPEN)SelectObject(m_memDC, pen);

        MoveToEx(m_memDC, (int)sx1, (int)sy1, nullptr);
        LineTo(m_memDC, (int)sx2, (int)sy2);

        SelectObject(m_memDC, oldPen);
        DeleteObject(pen);
    }

    int GdiRenderer::GetWidth() const
    {
        return m_width;
    }

    int GdiRenderer::GetHeight() const
    {
        return m_height;
    }
    HDC GdiRenderer::GetFrameDeviceContext() const
    {
        if (!m_frameReady)
            return nullptr;

        return m_memDC;
    }

}
