#pragma once

#include "Core/IWindow.h"
#include "Core/IRenderer.h"

namespace Core
{
    class Application
    {
    public:
        Application(IWindow& window, IRenderer& renderer);
        virtual ~Application() = default;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();

    protected:
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnRender() = 0;

        IRenderer& GetRenderer() { return m_renderer; }
        const IRenderer& GetRenderer() const { return m_renderer; }

        IWindow& GetWindow() { return m_window; }
        const IWindow& GetWindow() const { return m_window; }

    private:
        void RunFrame();

        IWindow& m_window;
        IRenderer& m_renderer;

        bool m_isRunning = false;
    };
}
