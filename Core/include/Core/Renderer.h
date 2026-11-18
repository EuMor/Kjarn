#pragma once
#include <windows.h>
#include "Core/Color.h"

namespace Core {

    class Renderer {
    public:
        Renderer(HWND hwnd);
        ~Renderer();

        // Вызывается в начале кадра: очищает экран цветом
        void BeginFrame();

        // Вызывается в конце кадра: переносит нарисованное на экран
        void EndFrame();

        // Рисует прямоугольник
        void DrawRect(float x, float y, float width, float height, Color color);

    private:
        HWND m_hwnd;
        HDC m_memDC;
        HBITMAP m_memBitmap;
        HBITMAP m_oldBitmap;
        int m_width = 0;
        int m_height = 0;
    };

}