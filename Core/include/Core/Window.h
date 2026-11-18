#pragma once
#include <string>
#include <windows.h>

namespace Core {

    class Window {
    public:
        // Изменили std::string на std::wstring
        Window(const std::wstring& title, int width, int height);
        ~Window();

        void Run();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

    private:
        static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        HWND m_hwnd = nullptr;
        int  m_width;
        int  m_height;
    };

} // namespace Core