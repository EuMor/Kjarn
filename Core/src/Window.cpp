#include "Core/Window.h"
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
        wc.lpszClassName = CLASS_NAME; // Просто передаем, никаких кастов!

        RegisterClassEx(&wc);

        m_hwnd = CreateWindowEx(
            0,
            CLASS_NAME,          // Просто передаем
            title.c_str(),       // Передаем широкую строку (wchar_t*)
            WS_OVERLAPPEDWINDOW,
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

    void Window::Run() {
        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_DESTROY:
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