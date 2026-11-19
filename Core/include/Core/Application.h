
#pragma once

#include "Renderer.h"
#include "Platform/Win32/Window.h"

#include <string>

namespace Core
{
    struct ApplicationConfig
    {
        std::wstring title = L"Kjarn Application";
        int width = 1280;
        int height = 720;
    };

    class Application
    {
    public:
        explicit Application(const ApplicationConfig& config = {});
        virtual ~Application() = default;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();

    protected:
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnRender() = 0;

        Renderer& GetRenderer() { return m_renderer; }
        Window& GetWindow() { return m_window; }

    private:
        void RunFrame();

        Window m_window;
        Renderer m_renderer;

        bool m_isRunning = false;
    };
}
