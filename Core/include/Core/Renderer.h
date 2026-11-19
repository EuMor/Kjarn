#pragma once
#include <windows.h>
#include "Color.h"
#include "Camera.h"

namespace Core {

    class Renderer {
    public:
        explicit Renderer(HWND hwnd);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

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