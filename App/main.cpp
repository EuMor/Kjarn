#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/Renderer.h"
#include "Core/Time.h"
#include "Core/Entity.h"
#include "Core/Camera.h"
#include <vector>
#include <string>

int main() {
    Core::Window window(L"Kjarn Engine — Camera Test", 800, 600);
    Core::Renderer renderer(window.GetNativeHandle());
    Core::Camera camera;

    std::vector<Core::Entity> scene;

    // Создаем сетку из квадратов, чтобы видеть, как работает камера
    for (int i = -5; i <= 5; ++i) {
        for (int j = -5; j <= 5; ++j) {
            Core::Color c = (i + j) % 2 == 0 ? Core::Color(80, 80, 80) : Core::Color(100, 100, 100);
            scene.push_back(Core::Entity(i * 100.0f, j * 100.0f, 90.0f, 90.0f, c));
        }
    }

    float cameraSpeed = 500.0f;

    while (!window.ShouldClose()) {
        Core::Time::Update();
        float dt = Core::Time::GetDeltaTime();

        window.PollEvents();

        // --- Логика Камеры ---
        // Движение камеры на WASD
        if (Core::Input::IsKeyDown('W')) camera.y -= cameraSpeed * dt / camera.zoom;
        if (Core::Input::IsKeyDown('S')) camera.y += cameraSpeed * dt / camera.zoom;
        if (Core::Input::IsKeyDown('A')) camera.x -= cameraSpeed * dt / camera.zoom;
        if (Core::Input::IsKeyDown('D')) camera.x += cameraSpeed * dt / camera.zoom;

        // Зум колесиком
        int wheel = Core::Input::GetMouseWheelDelta();
        if (wheel > 0) camera.zoom *= 1.1f; // Приблизить
        if (wheel < 0) camera.zoom /= 1.1f; // Отдалить

        // Ограничим зум, чтобы не улететь в бесконечность
        if (camera.zoom < 0.1f) camera.zoom = 0.1f;
        if (camera.zoom > 10.0f) camera.zoom = 10.0f;

        // --- Рендеринг ---
        renderer.BeginFrame();

        // Передаем камеру в рендерер при отрисовке каждого объекта
        for (const auto& entity : scene) {
            renderer.DrawRect(camera, entity.x, entity.y, entity.width, entity.height, entity.color);
        }

        renderer.EndFrame();

        Core::Input::Update();
        Sleep(1);
    }

    return 0;
}