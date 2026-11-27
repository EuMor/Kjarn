#pragma once
#include "Core/Tilemap.h"
#include "Core/Color.h"
#include <cmath>
#include <algorithm>

namespace Kjarn::Editor {

    class BrushTool {
    public:
        void Paint(Core::Tilemap& tilemap, int cellX, int cellY, Core::Color color) {
            if (!tilemap.IsOccupied(cellX, cellY)) {
                tilemap.SetTile(cellX, cellY, Core::Tile{ color });
            }
        }

        void Erase(Core::Tilemap& tilemap, int cellX, int cellY) {
            tilemap.EraseTile(cellX, cellY);
        }

        // Линия Брезенхэма для плавного рисования при быстром движении мыши
        void PaintLine(Core::Tilemap& tilemap, int x0, int y0, int x1, int y1, Core::Color color, bool isErasing) {
            int dx = std::abs(x1 - x0);
            int dy = std::abs(y1 - y0);
            int sx = (x0 < x1) ? 1 : -1;
            int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;

            while (true) {
                if (isErasing) Erase(tilemap, x0, y0);
                else Paint(tilemap, x0, y0, color);

                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        }
    };
}