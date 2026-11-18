#include "Core/Time.h"
#include <chrono>

namespace Core {
    static std::chrono::steady_clock::time_point s_lastTime;
    static float s_deltaTime = 0.0f;
    static float s_fps = 0.0f;
    static float s_fpsTimer = 0.0f;
    static int s_fpsCounter = 0;

    void Time::Update() {
        auto currentTime = std::chrono::steady_clock::now();
        if (s_lastTime.time_since_epoch().count() == 0) {
            s_lastTime = currentTime;
            return;
        }
        std::chrono::duration<float> elapsed = currentTime - s_lastTime;
        s_deltaTime = elapsed.count();
        s_lastTime = currentTime;

        // —читаем FPS дл€ отладки
        s_fpsTimer += s_deltaTime;
        s_fpsCounter++;
        if (s_fpsTimer >= 1.0f) {
            s_fps = (float)s_fpsCounter / s_fpsTimer;
            s_fpsTimer = 0;
            s_fpsCounter = 0;
        }
    }

    float Time::GetDeltaTime() { return s_deltaTime; }
    float Time::GetFPS() { return s_fps; }
}