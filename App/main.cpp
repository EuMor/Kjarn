#include "Core/Window.h"
#include "Core/Input.h"
#include <iostream>

int main() {
    Core::Window window(L"Kjarn Engine — Input Test", 800, 600);

    // Главный игровой цикл (Game Loop)
    while (!window.ShouldClose()) {
        // 1. Обрабатываем события Windows (мышь, клавиатура, закрытие)
        window.PollEvents();

        // 2. Проверяем ввод (например, если нажата клавиша W)
        if (Core::Input::IsKeyDown('W')) {
            std::cout << "W is held down\n";
        }

        // Проверяем одиночное нажатие (сработает один раз при нажатии)
        if (Core::Input::IsKeyPressed(VK_SPACE)) {
            std::cout << "Space pressed once!\n";
        }

        // Получаем позицию мыши
        float mx, my;
        Core::Input::GetMousePosition(mx, my);
        // std::cout << "Mouse: " << mx << ", " << my << "\n"; // Осторожно, будет спамить в консоль

        // 3. В конце кадра обновляем Input (запоминаем состояние для IsKeyPressed)
        Core::Input::Update();

        // Небольшая задержка, чтобы цикл не сжирал 100% процессора (пока нет рендера)
        Sleep(10);
    }

    return 0;
}