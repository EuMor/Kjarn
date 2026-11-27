#pragma once
#include "Core/Tilemap.h"
#include <vector>
#include <unordered_map>
#include <optional>

namespace Kjarn::Editor {

    struct Stroke {
        // Ключ -> Состояние ДО (std::nullopt = клетка была пуста)
        std::unordered_map<int64_t, std::optional<Core::Tile>> before;
        // Ключ -> Состояние ПОСЛЕ (std::nullopt = клетка стерта)
        std::unordered_map<int64_t, std::optional<Core::Tile>> after;
    };

    class EditorHistory {
    public:
        void Push(const Stroke& stroke) {
            if (stroke.before.empty() && stroke.after.empty()) return;
            m_undoStack.push_back(stroke);
            m_redoStack.clear();
        }

        void Undo(Core::Tilemap& tilemap) {
            if (m_undoStack.empty()) return;
            Stroke stroke = m_undoStack.back();
            m_undoStack.pop_back();

            for (const auto& [key, tileOpt] : stroke.before) {
                int x, y;
                Core::Tilemap::UnpackKey(key, x, y);
                if (tileOpt.has_value()) tilemap.SetTile(x, y, tileOpt.value());
                else tilemap.EraseTile(x, y);
            }
            m_redoStack.push_back(stroke);
        }

        void Redo(Core::Tilemap& tilemap) {
            if (m_redoStack.empty()) return;
            Stroke stroke = m_redoStack.back();
            m_redoStack.pop_back();

            for (const auto& [key, tileOpt] : stroke.after) {
                int x, y;
                Core::Tilemap::UnpackKey(key, x, y);
                if (tileOpt.has_value()) tilemap.SetTile(x, y, tileOpt.value());
                else tilemap.EraseTile(x, y);
            }
            m_undoStack.push_back(stroke);
        }

    private:
        std::vector<Stroke> m_undoStack;
        std::vector<Stroke> m_redoStack;
    };
}