
#include "Core/Scene/SceneSerializer.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace Core
{
    namespace
    {
        constexpr std::uint64_t MaxEntityCount = 1'000'000;

        const char* ShapeToString(ShapeType shape)
        {
            switch (shape)
            {
            case ShapeType::Rectangle:
                return "RECT";

            case ShapeType::Circle:
                return "CIRCLE";

            case ShapeType::Triangle:
                return "TRIANGLE";

            default:
                return nullptr;
            }
        }

        bool StringToShape(
            const std::string& value,
            ShapeType& shape
        )
        {
            if (value == "RECT")
                shape = ShapeType::Rectangle;
            else if (value == "CIRCLE")
                shape = ShapeType::Circle;
            else if (value == "TRIANGLE")
                shape = ShapeType::Triangle;
            else
                return false;

            return true;
        }

        bool ReadBool(std::istream& stream, bool& value)
        {
            int number = 0;

            if (!(stream >> number))
                return false;

            if (number != 0 && number != 1)
                return false;

            value = number != 0;
            return true;
        }

        bool IsValidEntity(const Entity& entity)
        {
            const auto& position = entity.transform.position;
            const auto& rotation = entity.transform.rotation;
            const auto& scale = entity.transform.scale;

            if (!entity.id.IsValid())
                return false;

            if (!std::isfinite(position.x) ||
                !std::isfinite(position.y) ||
                !std::isfinite(position.z))
                return false;

            if (!std::isfinite(rotation.x) ||
                !std::isfinite(rotation.y) ||
                !std::isfinite(rotation.z))
                return false;

            if (!std::isfinite(scale.x) ||
                !std::isfinite(scale.y) ||
                !std::isfinite(scale.z))
                return false;

            if (!std::isfinite(entity.width) ||
                !std::isfinite(entity.height))
                return false;

            if (entity.width <= 0.0f ||
                entity.height <= 0.0f)
                return false;

            if (ShapeToString(entity.shape) == nullptr)
                return false;

            return true;
        }

        bool LoadV1(std::istream& file, Scene& destination)
        {
            Scene loadedScene;

            std::string line;

            // Пропускаем остаток строки с заголовком.
            std::getline(file, line);

            while (std::getline(file, line))
            {
                if (line.empty())
                    continue;

                std::istringstream stream(line);

                std::string shapeName;

                float x = 0.0f;
                float y = 0.0f;
                float width = 0.0f;
                float height = 0.0f;

                int r = 0;
                int g = 0;
                int b = 0;

                if (!(stream >> shapeName
                    >> x >> y
                    >> width >> height
                    >> r >> g >> b))
                {
                    return false;
                }

                std::string extra;

                if (stream >> extra)
                    return false;

                ShapeType shape;

                if (!StringToShape(shapeName, shape))
                    return false;

                if (!std::isfinite(x) ||
                    !std::isfinite(y) ||
                    !std::isfinite(width) ||
                    !std::isfinite(height))
                    return false;

                if (width <= 0.0f || height <= 0.0f)
                    return false;

                if (r < 0 || r > 255 ||
                    g < 0 || g > 255 ||
                    b < 0 || b > 255)
                    return false;

                EntityId id = loadedScene.CreateEntity();

                Entity* entity = loadedScene.FindEntity(id);

                if (entity == nullptr)
                    return false;

                entity->shape = shape;

                entity->transform.position.x = x;
                entity->transform.position.y = y;

                entity->width = width;
                entity->height = height;

                entity->color = Color(
                    static_cast<std::uint8_t>(r),
                    static_cast<std::uint8_t>(g),
                    static_cast<std::uint8_t>(b)
                );
            }

            if (file.bad())
                return false;

            destination = std::move(loadedScene);
            return true;
        }
    }

    bool SceneSerializer::Save(
        const Scene& scene,
        const std::string& filename
    )
    {
        std::ofstream file(filename, std::ios::trunc);

        if (!file.is_open())
            return false;

        file << std::setprecision(
            std::numeric_limits<float>::max_digits10
        );

        file << "KJARN_SCENE_V2\n";
        file << "ENTITY_COUNT "
            << scene.GetEntityCount() << "\n";

        for (const Entity& entity : scene.GetEntities())
        {
            if (!IsValidEntity(entity))
                return false;

            const char* shape = ShapeToString(entity.shape);

            file << "ENTITY "
                << entity.id.value << " "
                << shape << " "

                // Position
                << entity.transform.position.x << " "
                << entity.transform.position.y << " "
                << entity.transform.position.z << " "

                // Rotation
                << entity.transform.rotation.x << " "
                << entity.transform.rotation.y << " "
                << entity.transform.rotation.z << " "

                // Scale
                << entity.transform.scale.x << " "
                << entity.transform.scale.y << " "
                << entity.transform.scale.z << " "

                // Dimensions
                << entity.width << " "
                << entity.height << " "

                // Color
                << static_cast<int>(entity.color.r) << " "
                << static_cast<int>(entity.color.g) << " "
                << static_cast<int>(entity.color.b) << " "

                // Persistent physics settings
                << static_cast<int>(entity.isStatic) << " "
                << static_cast<int>(entity.useGravity) << " "
                << static_cast<int>(entity.isPlayer)

                << "\n";
        }

        file.flush();

        return file.good();
    }

    bool SceneSerializer::Load(
        const std::string& filename,
        Scene& destination
    )
    {
        std::ifstream file(filename);

        if (!file.is_open())
            return false;

        std::string version;

        if (!(file >> version))
            return false;

        // Поддержка старого формата.
        if (version == "KJARN_SCENE_V1")
            return LoadV1(file, destination);

        if (version != "KJARN_SCENE_V2")
            return false;

        std::string countLabel;
        std::uint64_t entityCount = 0;

        if (!(file >> countLabel >> entityCount))
            return false;

        if (countLabel != "ENTITY_COUNT")
            return false;

        if (entityCount > MaxEntityCount)
            return false;

        Scene loadedScene;

        for (std::uint64_t i = 0; i < entityCount; ++i)
        {
            std::string recordType;
            std::string shapeName;

            std::uint64_t idValue = 0;

            if (!(file >> recordType >> idValue >> shapeName))
                return false;

            if (recordType != "ENTITY")
                return false;

            ShapeType shape;

            if (!StringToShape(shapeName, shape))
                return false;

            EntityId id{};
            id.value = idValue;

            Entity* entity = loadedScene.CreateEntityWithId(id);

            if (entity == nullptr)
                return false;

            auto& position = entity->transform.position;
            auto& rotation = entity->transform.rotation;
            auto& scale = entity->transform.scale;

            int r = 0;
            int g = 0;
            int b = 0;

            if (!(file
                >> position.x >> position.y >> position.z
                >> rotation.x >> rotation.y >> rotation.z
                >> scale.x >> scale.y >> scale.z
                >> entity->width >> entity->height
                >> r >> g >> b))
            {
                return false;
            }

            if (!ReadBool(file, entity->isStatic))
                return false;

            if (!ReadBool(file, entity->useGravity))
                return false;

            if (!ReadBool(file, entity->isPlayer))
                return false;

            entity->shape = shape;

            if (r < 0 || r > 255 ||
                g < 0 || g > 255 ||
                b < 0 || b > 255)
                return false;

            entity->color = Color(
                static_cast<std::uint8_t>(r),
                static_cast<std::uint8_t>(g),
                static_cast<std::uint8_t>(b)
            );

            // Скорость и состояние контакта не сохраняются:
            // это временные параметры игрового процесса.
            entity->velocityX = 0.0f;
            entity->velocityY = 0.0f;
            entity->isGrounded = false;

            if (!IsValidEntity(*entity))
                return false;
        }

        // Не допускаем посторонние данные после последнего объекта.
        std::string extra;

        if (file >> extra)
            return false;

        if (file.bad())
            return false;

        // Заменяем исходную сцену только после успешной загрузки.
        destination = std::move(loadedScene);

        return true;
    }
}
