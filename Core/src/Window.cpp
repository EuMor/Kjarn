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
        // Передаем ВСЕ сообщения в наш Input
        Input::ProcessMessage(uMsg, wParam, lParam);

        switch (uMsg) {
        case WM_DESTROY:
            if (g_windowInstance) g_windowInstance->m_shouldClose = true;
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
        {
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            HBRUSH bgBrush = CreateSolidBrush(RGB(40, 44, 52));
            FillRect(memDC, &rc, bgBrush);
            DeleteObject(bgBrush);

            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

} // namespace Core