#include "Sandbox/Editor/History/EditorHistory.h"

#include <utility>

namespace Kjarn::Editor
{
    void EditorHistory::PushTileStroke(BrushTool::Stroke stroke)
    {
        if (stroke.edits.empty())
            return;

        Entry entry;
        entry.label = "Brush stroke";
        entry.stroke = std::move(stroke);

        m_undo.push_back(std::move(entry));

        // Новое действие разрушает ветку повтора.
        m_redo.clear();

        while (m_undo.size() > MaxEntries)
            m_undo.pop_front();
    }

    bool EditorHistory::CanUndo() const
    {
        return !m_undo.empty();
    }

    bool EditorHistory::CanRedo() const
    {
        return !m_redo.empty();
    }

    bool EditorHistory::Undo(Core::Tilemap& tilemap)
    {
        if (m_undo.empty())
            return false;

        Entry entry = std::move(m_undo.back());
        m_undo.pop_back();

        ApplyEdits(tilemap, entry.stroke, true);

        m_redo.push_back(std::move(entry));

        return true;
    }

    bool EditorHistory::Redo(Core::Tilemap& tilemap)
    {
        if (m_redo.empty())
            return false;

        Entry entry = std::move(m_redo.back());
        m_redo.pop_back();

        ApplyEdits(tilemap, entry.stroke, false);

        m_undo.push_back(std::move(entry));

        return true;
    }

    void EditorHistory::Clear()
    {
        m_undo.clear();
        m_redo.clear();
    }

    std::string EditorHistory::GetUndoLabel() const
    {
        if (m_undo.empty())
            return {};

        return m_undo.back().label;
    }

    std::string EditorHistory::GetRedoLabel() const
    {
        if (m_redo.empty())
            return {};

        return m_redo.back().label;
    }

    void EditorHistory::ApplyEdits(
        Core::Tilemap& tilemap,
        const BrushTool::Stroke& stroke,
        bool undo
    )
    {
        // Откат идёт в обратном порядке — так корректно разбираются
        // клетки, которые мазок менял несколько раз.
        if (undo)
        {
            for (auto it = stroke.edits.rbegin();
                it != stroke.edits.rend();
                ++it)
            {
                tilemap.SetTile(
                    it->cell.x,
                    it->cell.y,
                    it->beforeTile,
                    it->beforeColor
                );
            }
        }
        else
        {
            for (const TileEdit& edit : stroke.edits)
            {
                tilemap.SetTile(
                    edit.cell.x,
                    edit.cell.y,
                    edit.afterTile,
                    edit.afterColor
                );
            }
        }
    }
}
