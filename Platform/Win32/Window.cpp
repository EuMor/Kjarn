#include "Window.h"
#include "Input.h"

#include <stdexcept>
#include <system_error>

int Core::Window::GetWidth() const
{
    if (m_hwnd == nullptr)
        return m_width;

    RECT rc{};

    if (!GetClientRect(m_hwnd, &rc))
        return m_width;

    return rc.right - rc.left;
}

int Core::Window::GetHeight() const
{
    if (m_hwnd == nullptr)
        return m_height;

    RECT rc{};

    if (!GetClientRect(m_hwnd, &rc))
        return m_height;

    return rc.bottom - rc.top;
}

namespace Core {

    namespace {
        constexpr wchar_t WINDOW_CLASS_NAME[] =
            L"KjarnWindowClass";
    }

    Window::Window(
        const std::wstring& title,
        int width,
        int height
    )
        : m_width(width),
        m_height(height)
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument(
                "Window dimensions must be positive."
            );
        }

        HINSTANCE hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = WINDOW_CLASS_NAME;

        if (!RegisterClassExW(&wc)) {
            const DWORD error = GetLastError();

            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                throw std::system_error(
                    error,
                    std::system_category(),
                    "Failed to register window class."
                );
            }
        }

        m_hwnd = CreateWindowExW(
            0,
            WINDOW_CLASS_NAME,
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            width,
            height,
            nullptr,
            nullptr,
            hInstance,
            this
        );

        if (m_hwnd == nullptr) {
            throw std::system_error(
                GetLastError(),
                std::system_category(),
                "Failed to create window."
            );
        }

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }

    Window::~Window() {
        if (m_hwnd != nullptr) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool Window::ShouldClose() const {
        return m_shouldClose;
    }

    void Window::PollEvents() {
        MSG msg{};

        while (PeekMessageW(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE
        )) {
            if (msg.message == WM_QUIT) {
                m_shouldClose = true;
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    LRESULT CALLBACK Window::WindowProc(
        HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam
    ) {
        Window* window = reinterpret_cast<Window*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA)
            );

        if (uMsg == WM_NCCREATE) {
            const auto* createInfo =
                reinterpret_cast<CREATESTRUCTW*>(lParam);

            window = static_cast<Window*>(
                createInfo->lpCreateParams
                );

            if (window == nullptr) {
                return FALSE;
            }

            SetWindowLongPtrW(
                hWnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(window)
            );
        }

        if (window != nullptr) {
            Input::ProcessMessage(uMsg, wParam, lParam);
        }

        switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (window != nullptr) {
                window->m_shouldClose = true;
            }
            return 0;

        case WM_NCDESTROY: {
            const LRESULT result =
                DefWindowProcW(hWnd, uMsg, wParam, lParam);

            if (window != nullptr) {
                window->m_hwnd = nullptr;
            }

            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);

            return result;
        }
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

}