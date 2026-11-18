#pragma once
#include <string>
#include <windows.h>

namespace Core {

    class Window {
    public:
        HWND GetNativeHandle() const { return m_hwnd; }
        Window(const std::wstring& title, int width, int height);
        ~Window();

        // Убираем старый Run() и добавляем:
        bool ShouldClose() const;
        void PollEvents(); // Обрабатывает сообщения Windows

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

    private:
        static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        HWND m_hwnd = nullptr;
        int  m_width;
        int  m_height;
        bool m_shouldClose = false; // Флаг: пора ли закрывать окно
    };

} // namespace Core