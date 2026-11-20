#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "Sandbox/UI/Rendering/Win32/GdiUIDrawBackend.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
    RECT ToNativeRect(const Kjarn::UI::UIRect& rect)
    {
        const int left = static_cast<int>(std::lround(rect.x));
        const int top = static_cast<int>(std::lround(rect.y));

        const int right = static_cast<int>(
            std::lround(rect.x + rect.width)
            );

        const int bottom = static_cast<int>(
            std::lround(rect.y + rect.height)
            );

        return RECT{
            std::min(left, right),
            std::min(top, bottom),
            std::max(left, right),
            std::max(top, bottom)
        };
    }

    COLORREF ToNativeColor(const Kjarn::UI::UIColor& color)
    {
        return RGB(color.r, color.g, color.b);
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return {};

        const int requiredSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );

        if (requiredSize <= 0)
            return {};

        std::wstring result(
            static_cast<std::size_t>(requiredSize),
            L'\0'
        );

        const int convertedSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            requiredSize
        );

        if (convertedSize != requiredSize)
            return {};

        return result;
    }

    void DrawFillRect(
        HDC dc,
        const Kjarn::UI::UIFillRectCommand& command
    )
    {
        HBRUSH brush = CreateSolidBrush(
            ToNativeColor(command.color)
        );

        if (brush == nullptr)
            return;

        const RECT rect = ToNativeRect(command.rect);

        FillRect(dc, &rect, brush);

        DeleteObject(brush);
    }

    void DrawStrokeRect(
        HDC dc,
        const Kjarn::UI::UIStrokeRectCommand& command
    )
    {
        const int thickness = std::max(
            1,
            static_cast<int>(std::lround(command.thickness))
        );

        HPEN pen = CreatePen(
            PS_SOLID,
            thickness,
            ToNativeColor(command.color)
        );

        if (pen == nullptr)
            return;

        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(
            dc,
            GetStockObject(NULL_BRUSH)
        );

        const RECT rect = ToNativeRect(command.rect);

        Rectangle(
            dc,
            rect.left,
            rect.top,
            rect.right,
            rect.bottom
        );

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);

        DeleteObject(pen);
    }

    void DrawLine(
        HDC dc,
        const Kjarn::UI::UILineCommand& command
    )
    {
        const int thickness = std::max(
            1,
            static_cast<int>(std::lround(command.thickness))
        );

        HPEN pen = CreatePen(
            PS_SOLID,
            thickness,
            ToNativeColor(command.color)
        );

        if (pen == nullptr)
            return;

        HGDIOBJ oldPen = SelectObject(dc, pen);

        MoveToEx(
            dc,
            static_cast<int>(std::lround(command.start.x)),
            static_cast<int>(std::lround(command.start.y)),
            nullptr
        );

        LineTo(
            dc,
            static_cast<int>(std::lround(command.end.x)),
            static_cast<int>(std::lround(command.end.y))
        );

        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    void DrawUIText(
        HDC dc,
        const Kjarn::UI::UITextCommand& command
    )
    {
        const std::wstring text = Utf8ToWide(command.text);

        if (text.empty())
            return;

        const int fontSize = std::max(
            1,
            static_cast<int>(std::lround(command.fontSize))
        );

        HFONT font = CreateFontW(
            -fontSize,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );

        if (font == nullptr)
            return;

        HGDIOBJ oldFont = SelectObject(dc, font);

        const int oldBkMode = SetBkMode(dc, TRANSPARENT);
        const COLORREF oldColor = SetTextColor(
            dc,
            ToNativeColor(command.color)
        );

        TextOutW(
            dc,
            static_cast<int>(std::lround(command.position.x)),
            static_cast<int>(std::lround(command.position.y)),
            text.c_str(),
            static_cast<int>(text.size())
        );

        SetTextColor(dc, oldColor);
        SetBkMode(dc, oldBkMode);

        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
}

namespace Kjarn::UI::Win32
{
    GdiUIDrawBackend::GdiUIDrawBackend(
        Renderer::Win32::GdiRenderer& renderer
    )
        : m_renderer(renderer)
    {
    }

    void GdiUIDrawBackend::Submit(const UIDrawList& drawList)
    {
        HDC dc = m_renderer.GetFrameDeviceContext();

        if (dc == nullptr)
            return;

        std::vector<int> clipStack;

        for (const UIDrawCommand& command : drawList.GetCommands())
        {
            if (const auto* fill =
                std::get_if<UIFillRectCommand>(&command))
            {
                DrawFillRect(dc, *fill);
            }
            else if (const auto* stroke =
                std::get_if<UIStrokeRectCommand>(&command))
            {
                DrawStrokeRect(dc, *stroke);
            }
            else if (const auto* line =
                std::get_if<UILineCommand>(&command))
            {
                DrawLine(dc, *line);
            }
            else if (const auto* text =
                std::get_if<UITextCommand>(&command))
            {
                DrawUIText(dc, *text);
            }
            else if (const auto* pushClip =
                std::get_if<UIPushClipRectCommand>(&command))
            {
                const int savedState = SaveDC(dc);

                if (savedState == 0)
                    continue;

                const RECT clipRect = ToNativeRect(pushClip->rect);

                if (IntersectClipRect(
                    dc,
                    clipRect.left,
                    clipRect.top,
                    clipRect.right,
                    clipRect.bottom
                ) == ERROR)
                {
                    RestoreDC(dc, savedState);
                    continue;
                }

                clipStack.push_back(savedState);
            }
            else if (std::holds_alternative<UIPopClipRectCommand>(command))
            {
                if (!clipStack.empty())
                {
                    RestoreDC(dc, clipStack.back());
                    clipStack.pop_back();
                }
            }
        }

        // Restore any clipping states left unclosed by the draw list.
        while (!clipStack.empty())
        {
            RestoreDC(dc, clipStack.back());
            clipStack.pop_back();
        }
    }
}