#pragma once

#include "UIGeometry.h"

#include <string>
#include <variant>

namespace Kjarn::UI
{
    struct UIFillRectCommand
    {
        UIRect rect;
        UIColor color;
    };

    struct UIStrokeRectCommand
    {
        UIRect rect;
        UIColor color;
        float thickness = 1.0f;
    };

    struct UILineCommand
    {
        UIVec2 start;
        UIVec2 end;
        UIColor color;
        float thickness = 1.0f;
    };

    struct UITextCommand
    {
        UIVec2 position;
        std::string text;
        UIColor color;
        float fontSize = 14.0f;
    };

    struct UIPushClipRectCommand
    {
        UIRect rect;
    };

    struct UIPopClipRectCommand
    {
    };

    using UIDrawCommand = std::variant<
        UIFillRectCommand,
        UIStrokeRectCommand,
        UILineCommand,
        UITextCommand,
        UIPushClipRectCommand,
        UIPopClipRectCommand
    >;
}