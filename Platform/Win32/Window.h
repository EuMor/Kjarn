
#pragma once

#include "Core/IWindow.h"

#include <string>
#include <windows.h>

namespace Core
{
    class Window final : public IWindow
    {
    public:
        Window(
            const std::wstring& title,
            int width,
            int height
        );

        ~Window() override;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        HWND GetNativeHandle() const
        {
            return m_hwnd;
        }

        bool ShouldClose() const override;
        void PollEvents() override;

        int GetWidth() const override;
        int GetHeight() const override;

    private:
        static LRESULT CALLBACK WindowProc(
            HWND hWnd,
            UINT uMsg,
            WPARAM wParam,
            LPARAM lParam
        );

        HWND m_hwnd = nullptr;

        int m_width = 0;
        int m_height = 0;

        bool m_shouldClose = false;
    };
}
