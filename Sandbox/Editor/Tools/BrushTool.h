#pragma once
#include "Core/Tilemap.h"
#include "Core/Color.h"
#include "Editor/History/EditorHistory.h"
#include <cmath>
#include <algorithm>
#include <optional>

namespace Kjarn::Editor {

    class BrushTool {
    public:
        void PaintLine(Core::Tilemap& tilemap, int x0, int y0, int x1, int y1, Core::Color color, Core::TilePattern pattern, bool isErasing, Stroke& stroke) {
            int dx = std::abs(x1 - x0);
            int dy = std::abs(y1 - y0);
            int sx = (x0 < x1) ? 1 : -1;
            int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;

            while (true) {
                ApplyBrush(tilemap, x0, y0, color, pattern, isErasing, stroke);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        }

    private:
        void ApplyBrush(Core::Tilemap& tilemap, int x, int y, Core::Color color, Core::TilePattern pattern, bool isErasing, Stroke& stroke) {
            int64_t key = Core::Tilemap::PackKey(x, y);

            if (stroke.after.find(key) != stroke.after.end()) return;

            const Core::Tile* existing = tilemap.GetTile(x, y);
            if (existing) stroke.before[key] = *existing;
            else stroke.before[key] = std::nullopt;

            if (isErasing) {
                tilemap.EraseTile(x, y);
                stroke.after[key] = std::nullopt;
            }
            else {
                Core::Tile newTile{ color, pattern }; // 👈 ТЕПЕРЬ ПЕРЕДАЕМ И ПАТТЕРН
                tilemap.SetTile(x, y, newTile);
                stroke.after[key] = newTile;
            }
        }
    };
}