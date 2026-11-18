#include "Core/Renderer.h"

namespace Core {

    Renderer::Renderer(HWND hwnd) : m_hwnd(hwnd) {}

    Renderer::~Renderer() {
        if (m_memDC) {
            SelectObject(m_memDC, m_oldBitmap);
            DeleteObject(m_memBitmap);
            DeleteDC(m_memDC);
        }
    }

    void Renderer::BeginFrame() {
        HDC hdc = GetDC(m_hwnd);
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        // Если размер окна изменился (или буфер создается впервые), пересоздаем его
        if (width != m_width || height != m_height || !m_memDC) {
            if (m_memDC) {
                SelectObject(m_memDC, m_oldBitmap);
                DeleteObject(m_memBitmap);
                DeleteDC(m_memDC);
            }
            m_width = width;
            m_height = height;
            m_memDC = CreateCompatibleDC(hdc);
            m_memBitmap = CreateCompatibleBitmap(hdc, width, height);
            m_oldBitmap = (HBITMAP)SelectObject(m_memDC, m_memBitmap);
        }
        ReleaseDC(m_hwnd, hdc);

        // Заливаем фон (тёмно-серый цвет)
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 44, 52));
        RECT rcFull = { 0, 0, m_width, m_height };
        FillRect(m_memDC, &rcFull, bgBrush);
        DeleteObject(bgBrush);
    }

    void Renderer::DrawRect(const Camera& camera, float x, float y, float width, float height, Color color) {
        float screenX, screenY;
        // Пересчитываем позицию
        camera.WorldToScreen(x, y, (float)m_width, (float)m_height, screenX, screenY);

        // Пересчитываем размеры с учетом зума
        float screenW = width * camera.zoom;
        float screenH = height * camera.zoom;

        HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
        RECT rect = { (int)screenX, (int)screenY, (int)(screenX + screenW), (int)(screenY + screenH) };
        FillRect(m_memDC, &rect, brush);
        DeleteObject(brush);
    }

    void Renderer::EndFrame() {
        HDC hdc = GetDC(m_hwnd);
        BitBlt(hdc, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
        ReleaseDC(m_hwnd, hdc);
    }

}