#pragma once

#include "UIDrawCommand.h"

#include <vector>

namespace Kjarn::UI
{
    class UIDrawList
    {
    public:
        void Clear();

        void AddFillRect(
            const UIRect& rect,
            const UIColor& color
        );

        void AddStrokeRect(
            const UIRect& rect,
            const UIColor& color,
            float thickness = 1.0f
        );

        void AddLine(
            const UIVec2& start,
            const UIVec2& end,
            const UIColor& color,
            float thickness = 1.0f
        );

        void AddText(
            const UIVec2& position,
            std::string text,
            const UIColor& color,
            float fontSize = 14.0f
        );

        void PushClipRect(const UIRect& rect);
        bool PopClipRect();

        const std::vector<UIDrawCommand>& GetCommands() const;

        bool IsEmpty() const;
        std::size_t GetCommandCount() const;

    private:
        std::vector<UIDrawCommand> m_commands;
        std::size_t m_clipDepth = 0;
    };
}