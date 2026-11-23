#pragma once

#include "Core/Scene/Scene.h"

#include <string>

namespace Core
{
    class SceneSerializer
    {
    public:
        static bool Save(
            const Scene& scene,
            const std::string& filename
        );

        static bool Load(
            const std::string& filename,
            Scene& scene
        );
    };
}
