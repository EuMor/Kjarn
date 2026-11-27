//#include "Sandbox/Editor/Tools/BrushTool.h"
//
//#include <algorithm>
//#include <cstdlib>
//
//namespace Kjarn::Editor
//{
//    namespace
//    {
//        // Предел клеток на один мазок: защита от гигантских линий
//        // при телепорте мыши (например, после Alt+Tab).
//        constexpr int MaxCellsPerStroke = 8192;
//    }
//
//    void BrushTool::Begin(
//        Core::Tilemap& tilemap,
//        BrushMode mode,
//        const Core::Color& color
//    )
//    {
//        m_isActive = true;
//        m_mode = mode;
//        m_color = color;
//
//        m_lastCellX = 0;
//        m_lastCellY = 0;
//        m_hasLastCell = false;
//
//        m_edits.clear();
//    }
//
//    void BrushTool::Continue(Core::Tilemap& tilemap, int cellX, int cellY)
//    {
//        if (!m_isActive)
//            return;
//
//        if (!m_hasLastCell)
//        {
//            PaintCell(tilemap, cellX, cellY, true);
//
//            m_lastCellX = cellX;
//            m_lastCellY = cellY;
//            m_hasLastCell = true;
//            return;
//        }
//
//        if (cellX == m_lastCellX && cellY == m_lastCellY)
//            return;
//
//        PaintLine(tilemap, m_lastCellX, m_lastCellY, cellX, cellY, true);
//
//        m_lastCellX = cellX;
//        m_lastCellY = cellY;
//    }
//
//    void BrushTool::End()
//    {
//        m_isActive = false;
//        m_hasLastCell = false;
//    }
//
//    bool BrushTool::TryTakeStroke(Stroke& outStroke)
//    {
//        if (m_edits.empty())
//        {
//            m_edits.clear();
//            return false;
//        }
//
//        outStroke.id = m_nextStrokeId++;
//        outStroke.edits = std::move(m_edits);
//
//        m_edits.clear();
//        m_hasLastCell = false;
//
//        return true;
//    }
//
//    void BrushTool::DiscardStroke()
//    {
//        m_edits.clear();
//        m_hasLastCell = false;
//        m_isActive = false;
//    }
//
//    BrushTool::TileEdit* BrushTool::FindEdit(int cellX, int cellY)
//    {
//        const auto it = std::find_if(
//            m_edits.begin(),
//            m_edits.end(),
//            [cellX, cellY](const TileEdit& edit)
//            {
//                return edit.cell.x == cellX && edit.cell.y == cellY;
//            }
//        );
//
//        if (it == m_edits.end())
//            return nullptr;
//
//        return &(*it);
//    }
//
//    bool BrushTool::PaintCell(
//        Core::Tilemap& tilemap,
//        int cellX,
//        int cellY,
//        bool recordEdits
//    )
//    {
//        const std::uint8_t beforeTile = tilemap.GetTile(cellX, cellY);
//        const Core::Color beforeColor = tilemap.GetColor(cellX, cellY);
//
//        const std::uint8_t afterTile =
//            (m_mode == BrushMode::Paint)
//            ? Core::Tilemap::SolidTile
//            : Core::Tilemap::EmptyTile;
//
//        const Core::Color afterColor =
//            (m_mode == BrushMode::Paint)
//            ? m_color
//            : Core::Color{};
//
//        // Ничего не меняется — не трогаем ни карту, ни историю.
//        if (beforeTile == afterTile &&
//            (afterTile == Core::Tilemap::EmptyTile || beforeColor == afterColor))
//        {
//            return false;
//        }
//
//        tilemap.SetTile(cellX, cellY, afterTile, afterColor);
//
//        if (!recordEdits)
//            return true;
//
//        // Клетка могла уже быть изменена в этом мазке:
//        // обновляем конечное состояние, сохраняя исходное.
//        if (TileEdit* existing = FindEdit(cellX, cellY))
//        {
//            existing->afterTile = afterTile;
//            existing->afterColor = afterColor;
//            return true;
//        }
//
//        if (static_cast<int>(m_edits.size()) >= MaxCellsPerStroke)
//            return false;
//
//        TileEdit edit;
//        edit.cell.x = cellX;
//        edit.cell.y = cellY;
//        edit.beforeTile = beforeTile;
//        edit.beforeColor = beforeColor;
//        edit.afterTile = afterTile;
//        edit.afterColor = afterColor;
//
//        m_edits.push_back(edit);
//        return true;
//    }
//
//    void BrushTool::PaintLine(
//        Core::Tilemap& tilemap,
//        int fromX,
//        int fromY,
//        int toX,
//        int toY,
//        bool recordEdits
//    )
//    {
//        // Алгоритм Брезенхэма: непрерывная линия между соседними кадрами.
//        int x = fromX;
//        int y = fromY;
//
//        const int dx = std::abs(toX - fromX);
//        const int dy = std::abs(toY - fromY);
//
//        const int stepX = (fromX < toX) ? 1 : -1;
//        const int stepY = (fromY < toY) ? 1 : -1;
//
//        int error = dx - dy;
//
//        for (;;)
//        {
//            PaintCell(tilemap, x, y, recordEdits);
//
//            if (x == toX && y == toY)
//                break;
//
//            const int doubledError = error * 2;
//
//            if (doubledError > -dy)
//            {
//                error -= dy;
//                x += stepX;
//            }
//
//            if (doubledError < dx)
//            {
//                error += dx;
//                y += stepY;
//            }
//        }
//    }
//}
