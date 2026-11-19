#pragma once

namespace Core {
    class Camera {
    public:
        float x = 0.0f;      // Центр камеры по X
        float y = 0.0f;      // Центр камеры по Y
        float zoom = 1.0f;   // Масштаб (1.0 = 100%)

        // Пересчитывает мировые координаты в экранные
        void WorldToScreen(float worldX, float worldY, float screenW, float screenH,
            float& outX, float& outY) const;
    };
}