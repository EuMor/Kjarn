#pragma once
#include <windows.h>

namespace Core {

    class Input {
    public:
        // Вызывается в начале каждого кадра
        static void Update();

        // Проверка, зажата ли клавиша (например, 'W' или VK_UP)
        static bool IsKeyDown(int key);

        // Проверка, была ли клавиша нажата именно в этом кадре (одиночное нажатие)
        static bool IsKeyPressed(int key);

        static void GetMousePosition(float& x, float& y);

        // Внутренняя функция для обработки сообщений Windows
        static void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);

        static int GetMouseWheelDelta();
        static bool IsLeftMouseButtonPressed();

    private:
        static bool s_Keys[256];      // Текущее состояние клавиш
        static bool s_PrevKeys[256];  // Состояние клавиш в прошлом кадре
        static float s_MouseX;
        static float s_MouseY;
        static bool s_LeftMousePressedThisFrame;
    };

} // namespace Core