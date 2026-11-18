#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/Renderer.h"
#include "Core/Time.h"
#include "Core/Entity.h"
#include <vector>
#include <string>

int main() {
    Core::Window window(L"Kjarn Engine — Core Architecture", 800, 600);
    Core::Renderer renderer(window.GetNativeHandle());

    // Наша "сцена" (пока просто вектор объектов)
    std::vector<Core::Entity> scene;

    // Создаем игрока
    scene.push_back(Core::Entity(400.0f, 300.0f, 50.0f, 50.0f, Core::Color(200, 50, 50)));
    // Создаем статичную платформу
    scene.push_back(Core::Entity(100.0f, 500.0f, 200.0f, 50.0f, Core::Color(50, 200, 50)));

    float playerSpeed = 300.0f; // Теперь скорость в пикселях в СЕКУНДУ

    while (!window.ShouldClose()) {
        Core::Time::Update(); // Замеряем время
        float dt = Core::Time::GetDeltaTime(); // Дельта времени

        window.PollEvents();

        // --- Логика ---
        // Управляем первым объектом (игроком)
        if (Core::Input::IsKeyDown('W')) scene[0].y -= playerSpeed * dt;
        if (Core::Input::IsKeyDown('S')) scene[0].y += playerSpeed * dt;
        if (Core::Input::IsKeyDown('A')) scene[0].x -= playerSpeed * dt;
        if (Core::Input::IsKeyDown('D')) scene[0].x += playerSpeed * dt;

        // Обновляем заголовок окна, показывая FPS
        if (Core::Input::IsKeyPressed(VK_F1)) { // По нажатию F1 меняем заголовок
            std::wstring title = L"Kjarn Engine - FPS: " + std::to_wstring((int)Core::Time::GetFPS());
            SetWindowTextW(window.GetNativeHandle(), title.c_str());
        }

        // --- Рендеринг ---
        renderer.BeginFrame();

        // Рисуем ВСЕ объекты из сцены
        for (const auto& entity : scene) {
            renderer.DrawRect(entity.x, entity.y, entity.width, entity.height, entity.color);
        }

        renderer.EndFrame();

        Core::Input::Update();

        // Убираем жесткий Sleep. Даем процессору немного отдохнуть, 
        // но точность времени теперь обеспечивает Time::Update()
        Sleep(1);
    }

    return 0;
}