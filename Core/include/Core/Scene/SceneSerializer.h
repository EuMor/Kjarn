#pragma once

#include "Core/Scene/Scene.h"

#include <string>
#include <string_view>

namespace Core
{
    // Если передан error, при неудаче в него пишется причина
    // (для ошибок разбора — с номером строки), при успехе он очищается.
    class SceneSerializer
    {
    public:
        // Пишет KJARN_SCENE_V4. Данные сначала попадают во временный файл
        // "<filename>.tmp" и заменяют целевой только после успешной записи,
        // поэтому неудачное сохранение не портит прежний файл.
        static bool Save(
            const Scene& scene,
            const std::string& filename,
            std::string* error = nullptr
        );

        // Читает V4, V3 и V2. Сцена scene заменяется только если файл
        // прочитан целиком и прошёл все проверки; при неудаче она не меняется.
        static bool Load(
            const std::string& filename,
            Scene& scene,
            std::string* error = nullptr
        );

        // То же без файла — текст в памяти.
        static bool Serialize(
            const Scene& scene,
            std::string& text,
            std::string* error = nullptr
        );

        static bool Deserialize(
            std::string_view text,
            Scene& scene,
            std::string* error = nullptr
        );
    };
}
