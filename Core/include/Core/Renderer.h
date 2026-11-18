#pragma once
#include <windows.h>
#include "Core/Color.h"
#include "Core/Camera.h"

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
        //void DrawRect(float x, float y, float width, float height, Color color);
        void DrawRect(const Camera& camera, float x, float y, float width, float height, Color color);

        // Геттеры для размеров экрана (нужны для камеры)
        int GetWidth() const { return m_width; }
        int GetHeight() const { return m_height; }

    private:
        HWND m_hwnd;
        HDC m_memDC;
        HBITMAP m_memBitmap;
        HBITMAP m_oldBitmap;
        int m_width = 0;
        int m_height = 0;
    };

}