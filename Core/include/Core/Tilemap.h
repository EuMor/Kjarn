#pragma once
#include "Core/Color.h"
#include <unordered_map>
#include <cstdint>
#include <cmath>

namespace Core {

    enum class TilePattern {
        Solid,  // Просто цвет
        Grass,  // Трава (добавим темные травинки)
        Stone,  // Камень (добавим серые трещины)
        Water   // Вода (добавим светлые блики)
    };
    // Структура одного тайла (клетки)
    struct Tile {
        Color color;
        TilePattern pattern = TilePattern::Solid;
        // В будущем сюда можно добавить ID текстуры, тип коллизии и т.д.
    };

    class Tilemap {
    public:
        void SetTile(int x, int y, const Tile& tile) {
            m_tiles[PackKey(x, y)] = tile;
        }

        void EraseTile(int x, int y) {
            m_tiles.erase(PackKey(x, y));
        }

        const Tile* GetTile(int x, int y) const {
            auto it = m_tiles.find(PackKey(x, y));
            if (it != m_tiles.end()) {
                return &it->second;
            }
            return nullptr;
        }

        // Проверка, занята ли клетка
        bool IsOccupied(int x, int y) const {
            return m_tiles.find(PackKey(x, y)) != m_tiles.end();
        }

        void Clear() {
            m_tiles.clear();
        }

        const std::unordered_map<int64_t, Tile>& GetTiles() const {
            return m_tiles;
        }

        // Функция для упаковки двух 32-битных int в один 64-битный ключ
        static int64_t PackKey(int x, int y) {
            return (static_cast<int64_t>(y) << 32) | static_cast<uint32_t>(x);
        }

        static void UnpackKey(int64_t key, int& outX, int& outY) {
            outY = static_cast<int32_t>(key >> 32);
            outX = static_cast<int32_t>(key & 0xFFFFFFFF);
        }

    private:
        // Используем hash map для бесконечного разреженного мира
        std::unordered_map<int64_t, Tile> m_tiles;
    };
}