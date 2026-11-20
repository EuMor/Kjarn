#include "UIDrawList.h"

#include <utility>

namespace Kjarn::UI
{
    void UIDrawList::Clear()
    {
        m_commands.clear();
        m_clipDepth = 0;
    }

    void UIDrawList::AddFillRect(
        const UIRect& rect,
        const UIColor& color
    )
    {
        m_commands.emplace_back(
            UIFillRectCommand{ rect, color }
        );
    }

    void UIDrawList::AddStrokeRect(
        const UIRect& rect,
        const UIColor& color,
        float thickness
    )
    {
        if (thickness <= 0.0f)
            return;

        m_commands.emplace_back(
            UIStrokeRectCommand{
                rect,
                color,
                thickness
            }
        );
    }

    void UIDrawList::AddLine(
        const UIVec2& start,
        const UIVec2& end,
        const UIColor& color,
        float thickness
    )
    {
        if (thickness <= 0.0f)
            return;

        m_commands.emplace_back(
            UILineCommand{
                start,
                end,
                color,
                thickness
            }
        );
    }

    void UIDrawList::AddText(
        const UIVec2& position,
        std::string text,
        const UIColor& color,
        float fontSize
    )
    {
        if (text.empty() || fontSize <= 0.0f)
            return;

        m_commands.emplace_back(
            UITextCommand{
                position,
                std::move(text),
                color,
                fontSize
            }
        );
    }

    void UIDrawList::PushClipRect(const UIRect& rect)
    {
        m_commands.emplace_back(
            UIPushClipRectCommand{ rect }
        );

        ++m_clipDepth;
    }

    bool UIDrawList::PopClipRect()
    {
        if (m_clipDepth == 0)
            return false;

        m_commands.emplace_back(
            UIPopClipRectCommand{}
        );

        --m_clipDepth;
        return true;
    }

    const std::vector<UIDrawCommand>&
        UIDrawList::GetCommands() const
    {
        return m_commands;
    }

    bool UIDrawList::IsEmpty() const
    {
        return m_commands.empty();
    }

    std::size_t UIDrawList::GetCommandCount() const
    {
        return m_commands.size();
    }
}