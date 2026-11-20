#include "Core/Camera.h"

namespace Core {
    void Camera::WorldToScreen(float worldX, float worldY, float screenW, float screenH,
        float& outX, float& outY) const {
        // Формула: (Мировая координата - Позиция камеры) * Зум + Центр экрана
        outX = (worldX - x) * zoom + (screenW / 2.0f);
        outY = (worldY - y) * zoom + (screenH / 2.0f);
    }
}