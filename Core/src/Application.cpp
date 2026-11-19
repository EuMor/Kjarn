
#include "Core/Application.h"

#include "Core/Input.h"
#include "Core/Time.h"

#include <chrono>
#include <thread>

namespace Core
{
    Application::Application(const ApplicationConfig& config)
        : m_window(config.title, config.width, config.height)
        , m_renderer(m_window.GetNativeHandle())
    {
    }

    void Application::Run()
    {
        if (m_isRunning)
            return;

        m_isRunning = true;

        while (m_isRunning && !m_window.ShouldClose())
        {
            RunFrame();
        }

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

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
