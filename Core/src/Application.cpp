
#include "Core/Application.h"

#include "Platform/Win32/Input.h"
#include "Core/Time.h"

#include <chrono>
#include <thread>

namespace Core
{
    Application::Application(
        IWindow& window,
        IRenderer& renderer
    )
        : m_window(window)
        , m_renderer(renderer)
    {
    }

    void Application::Run()
    {
        if (m_isRunning)
            return;

        m_isRunning = true;

        while (m_isRunning && !m_window.ShouldClose())
            RunFrame();

        m_isRunning = false;
    }

    void Application::RunFrame()
    {
        Time::Update();

        const float deltaTime = Time::GetDeltaTime();

        m_window.PollEvents();

        if (m_window.ShouldClose())
            return;

        OnUpdate(deltaTime);

        m_renderer.BeginFrame();
        OnRender();
        m_renderer.EndFrame();

        Input::Update();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }
}
