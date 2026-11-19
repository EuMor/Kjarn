
#include "Core/Application.h"

#include "Core/Camera.h"
#include "Core/Color.h"
#include "Core/Entity.h"
#include "Core/Input.h"

#include <vector>

class CameraTestApplication final : public Core::Application
{
public:
    CameraTestApplication()
        : Core::Application({
            L"Kjarn Engine — Application Test",
            800,
            600
            })
    {
        CreateTestScene();
    }

protected:
    void OnUpdate(float deltaTime) override
    {
        constexpr float cameraSpeed = 500.0f;

        if (Core::Input::IsKeyDown('W'))
            m_camera.y -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('S'))
            m_camera.y += cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('A'))
            m_camera.x -= cameraSpeed * deltaTime / m_camera.zoom;

        if (Core::Input::IsKeyDown('D'))
            m_camera.x += cameraSpeed * deltaTime / m_camera.zoom;

        const int wheel = Core::Input::GetMouseWheelDelta();

        if (wheel > 0)
            m_camera.zoom *= 1.1f;

        if (wheel < 0)
            m_camera.zoom /= 1.1f;

        if (m_camera.zoom < 0.1f)
            m_camera.zoom = 0.1f;

        if (m_camera.zoom > 10.0f)
            m_camera.zoom = 10.0f;
    }

    void OnRender() override
    {
        auto& renderer = GetRenderer();

        for (const auto& entity : m_scene)
        {
            renderer.DrawRect(
                m_camera,
                entity.x,
                entity.y,
                entity.width,
                entity.height,
                entity.color
            );
        }
    }

private:
    void CreateTestScene()
    {
        for (int i = -50; i <= 50; ++i)
        {
            for (int j = -50; j <= 50; ++j)
            {
                const Core::Color color =
                    (i + j) % 2 == 0
                    ? Core::Color(80, 80, 80)
                    : Core::Color(100, 100, 100);

                m_scene.emplace_back(
                    i * 100.0f,
                    j * 100.0f,
                    90.0f,
                    90.0f,
                    color
                );
            }
        }
    }

    Core::Camera m_camera;
    std::vector<Core::Entity> m_scene;
};

int main()
{
    CameraTestApplication application;
    application.Run();

    return 0;
}
