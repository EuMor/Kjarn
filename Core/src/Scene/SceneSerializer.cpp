#include "Core/Scene/SceneSerializer.h"
#include "Core/Scene/Scene.h"
#include "Core/Tilemap.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace Core {

    bool SceneSerializer::Save(const Scene& scene, const std::string& filename)
    {
        std::ofstream file(filename);
        if (!file.is_open()) return false;

        // Новая версия формата V3
        file << "KJARN_SCENE_V3\n";

        // 1. Сохраняем Тайлмап (ландшафт)
        const auto& tilemap = scene.GetTilemap();
        for (const auto& [key, tile] : tilemap.GetTiles()) {
            int x, y;
            Tilemap::UnpackKey(key, x, y);

            // Формат: TILE [X] [Y] [R] [G] [B]
            file << "TILE " << x << " " << y << " "
                << (int)tile.color.r << " "
                << (int)tile.color.g << " "
                << (int)tile.color.b << "\n";
        }

        // 2. Сохраняем Сущности (игроки, враги)
        for (const auto& entity : scene.GetEntities()) {
            std::string typeStr = "RECT";
            if (entity.shape == ShapeType::Circle) typeStr = "CIRCLE";
            else if (entity.shape == ShapeType::Triangle) typeStr = "TRIANGLE";

            // Формат: ENTITY [ТИП] [X] [Y] [W] [H] [R] [G] [B]
            file << "ENTITY " << typeStr << " "
                << entity.transform.position.x << " "
                << entity.transform.position.y << " "
                << entity.width << " "
                << entity.height << " "
                << (int)entity.color.r << " "
                << (int)entity.color.g << " "
                << (int)entity.color.b << "\n";
        }

        return true;
    }

    bool SceneSerializer::Load(const std::string& filename, Scene& scene)
    {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        scene.Clear(); // Очищаем текущую сцену перед загрузкой

        std::string line;
        std::getline(file, line); // Читаем заголовок

        // Проверяем версию. (Если у тебя были старые сохранения V1/V2, они не загрузятся без адаптера)
        if (line != "KJARN_SCENE_V3") {
            std::cerr << "Unknown scene format: " << line << "\n";
            return false;
        }

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string type;
            iss >> type;

            if (type == "TILE") {
                int x, y, r, g, b;
                if (iss >> x >> y >> r >> g >> b) {
                    scene.GetTilemap().SetTile(x, y, Tile{ Color((uint8_t)r, (uint8_t)g, (uint8_t)b) });
                }
            }
            else if (type == "ENTITY") {
                std::string shapeStr;
                float x, y, w, h;
                int r, g, b;

                if (iss >> shapeStr >> x >> y >> w >> h >> r >> g >> b) {
                    EntityId id = scene.CreateEntity();
                    Entity* entity = scene.FindEntity(id);
                    if (entity) {
                        entity->transform.position.x = x;
                        entity->transform.position.y = y;
                        entity->width = w;
                        entity->height = h;
                        entity->color = Color((uint8_t)r, (uint8_t)g, (uint8_t)b);

                        if (shapeStr == "CIRCLE") entity->shape = ShapeType::Circle;
                        else if (shapeStr == "TRIANGLE") entity->shape = ShapeType::Triangle;
                        else entity->shape = ShapeType::Rectangle;
                    }
                }
            }
        }

        return true;
    }

} // namespace Core