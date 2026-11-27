#include "Core/Tilemap.h"

#include <algorithm>
#include <utility>

namespace Core
{
    namespace
    {
        // Ограничение на размер буфера: защита от случайного
        // гигантского расширения (например, при загрузке битого файла).
        constexpr int MaxExtent = 8192;
    }

    void Tilemap::Resize(int width, int height)
    {
        width = std::clamp(width, 1, MaxExtent);
        height = std::clamp(height, 1, MaxExtent);

        Reallocate(0, 0, width, height);
    }

    void Tilemap::Reallocate(int originX, int originY, int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            Clear();
            return;
        }

        std::vector<std::uint8_t> newTiles(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
            EmptyTile
        );
        std::vector<Color> newColors(newTiles.size(), Color{});

        // Копируем пересечение старых и новых границ:
        // расширение карты не должно стирать нарисованное.
        const int copyLeft = std::max(m_originX, originX);
        const int copyRight = std::min(m_originX + m_width, originX + width);
        const int copyTop = std::max(m_originY, originY);
        const int copyBottom = std::min(m_originY + m_height, originY + height);

        for (int y = copyTop; y < copyBottom; ++y)
        {
            for (int x = copyLeft; x < copyRight; ++x)
            {
                const std::size_t oldIndex =
                    static_cast<std::size_t>(y - m_originY) *
                    static_cast<std::size_t>(m_width) +
                    static_cast<std::size_t>(x - m_originX);

                const std::size_t newIndex =
                    static_cast<std::size_t>(y - originY) *
                    static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x - originX);

                newTiles[newIndex] = m_tiles[oldIndex];
                newColors[newIndex] = m_colors[oldIndex];
            }
        }

        m_originX = originX;
        m_originY = originY;
        m_width = width;
        m_height = height;
        m_tiles = std::move(newTiles);
        m_colors = std::move(newColors);
    }

    bool Tilemap::IsEmpty() const
    {
        return m_tiles.empty();
    }

    void Tilemap::Clear()
    {
        m_originX = 0;
        m_originY = 0;
        m_width = 0;
        m_height = 0;
        m_tiles.clear();
        m_colors.clear();
    }

    bool Tilemap::IsInside(int x, int y) const
    {
        return x >= m_originX && x < m_originX + m_width &&
            y >= m_originY && y < m_originY + m_height;
    }

    std::size_t Tilemap::Index(int x, int y) const
    {
        return static_cast<std::size_t>(y - m_originY) *
            static_cast<std::size_t>(m_width) +
            static_cast<std::size_t>(x - m_originX);
    }

    std::uint8_t Tilemap::GetTile(int x, int y) const
    {
        if (!IsInside(x, y))
            return EmptyTile;

        return m_tiles[Index(x, y)];
    }

    const Color& Tilemap::GetColor(int x, int y) const
    {
        static const Color absent{ 255, 255, 255 };

        if (!IsInside(x, y))
            return absent;

        return m_colors[Index(x, y)];
    }

    void Tilemap::SetTile(int x, int y, std::uint8_t tile, const Color& color)
    {
        EnsureContains(x, y);

        const std::size_t index = Index(x, y);

        m_tiles[index] = tile;
        m_colors[index] = color;
    }

    void Tilemap::EnsureContains(int x, int y)
    {
        if (IsInside(x, y))
            return;

        if (m_tiles.empty())
        {
            Resize(std::min(std::max(x + 1, 1), MaxExtent),
                std::min(std::max(y + 1, 1), MaxExtent));
            return;
        }

        const int left = std::min(m_originX, x);
        const int top = std::min(m_originY, y);
        const int right = std::max(m_originX + m_width, x + 1);
        const int bottom = std::max(m_originY + m_height, y + 1);

        if (right - left > MaxExtent || bottom - top > MaxExtent)
            return;

        Reallocate(left, top, right - left, bottom - top);
    }

    void Tilemap::EnsureContains(const CellAddress& cell)
    {
        EnsureContains(cell.x, cell.y);
    }

    void Tilemap::ApplyChanges(const std::vector<CellChange>& changes)
    {
        for (const CellChange& change : changes)
            SetTile(change.cell.x, change.cell.y, change.tile, change.color);
    }

    void Tilemap::ForEachOccupied(
        const std::function<void(int, int, const Color&)>& visitor) const
    {
        if (!visitor)
            return;

        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                const std::size_t index =
                    static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(m_width) +
                    static_cast<std::size_t>(x);

                if (m_tiles[index] != EmptyTile)
                {
                    visitor(m_originX + x, m_originY + y, m_colors[index]);
                }
            }
        }
    }
}
