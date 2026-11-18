#include "Core/Window.h"
#include "Core/Input.h" // Подключаем наш Input
#include <windows.h>

namespace Core {

    static Window* g_windowInstance = nullptr;

    Window::Window(const std::wstring& title, int width, int height)
        : m_width(width), m_height(height)
    {
        g_windowInstance = this;
        HINSTANCE hInstance = GetModuleHandle(nullptr);

        const wchar_t CLASS_NAME[] = L"MyCoreWindowClass";

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = CLASS_NAME;

        RegisterClassEx(&wc);

        m_hwnd = CreateWindowEx(
            0, CLASS_NAME, title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, hInstance, nullptr
        );

        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
        }
    }

    Window::~Window() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
        g_windowInstance = nullptr;
    }

    // --- НОВЫЕ МЕТОДЫ ---
    bool Window::ShouldClose() const {
        return m_shouldClose;
    }

    void Window::PollEvents() {
        MSG msg = {};
        // PeekMessage не блокирует поток, а просто забирает сообщения из очереди
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_shouldClose = true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    // -------------------

    LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        Input::ProcessMessage(uMsg, wParam, lParam);

        switch (uMsg) {
        case WM_ERASEBKGND:
            return 1; // Говорим Windows: "Я сам всё закрашу, не вмешивайся" (убирает мерцание)

        case WM_DESTROY:
            if (g_windowInstance) g_windowInstance->m_shouldClose = true;
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
        {
            // Теперь мы рисуем в цикле main.cpp, поэтому здесь просто валидируем область
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

} // namespace Core