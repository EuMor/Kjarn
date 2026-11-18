#pragma once
namespace Core {
    class Time {
    public:
        static void Update(); // Вызывается в начале каждого кадра
        static float GetDeltaTime(); // Время между кадрами (в секундах)
        static float GetFPS();
    };
}