#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/Renderer.h"
#include <windows.h> // Для функции Sleep

int main() {
    Core::Window window(L"Kjarn Engine — Render Test", 800, 600);
    Core::Renderer renderer(window.GetNativeHandle());

    // Позиция нашего "игрока"
    float playerX = 400.0f;
    float playerY = 300.0f;
    float playerSpeed = 5.0f;

    // Главный игровой цикл
    while (!window.ShouldClose()) {
        window.PollEvents();

        // --- 1. Логика (Обновление) ---
        if (Core::Input::IsKeyDown('W')) playerY -= playerSpeed;
        if (Core::Input::IsKeyDown('S')) playerY += playerSpeed;
        if (Core::Input::IsKeyDown('A')) playerX -= playerSpeed;
        if (Core::Input::IsKeyDown('D')) playerX += playerSpeed;

        // --- 2. Рендеринг (Отрисовка) ---
        renderer.BeginFrame();

        // Рисуем красный квадрат (нашего игрока)
        renderer.DrawRect(playerX, playerY, 50.0f, 50.0f, Core::Color(200, 50, 50));

        renderer.EndFrame();

        // --- 3. Конец кадра ---
        Core::Input::Update();
        Sleep(16); // Задержка ~16мс = ~60 FPS (чтобы не грузить процессор на 100%)
    }

    return 0;
}