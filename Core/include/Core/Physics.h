#pragma once

#include "Core/Scene/Entity.h"
#include "Core/Tilemap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Core::Physics {

    inline bool CheckCollision(const Entity& a, const Entity& b) {
        return a.transform.position.x < b.transform.position.x + b.width &&
            a.transform.position.x + a.width > b.transform.position.x &&
            a.transform.position.y < b.transform.position.y + b.height &&
            a.transform.position.y + a.height > b.transform.position.y;
    }

    namespace Detail {

        enum class Axis { X, Y };

        inline void MoveAxis(
            Entity& entity,
            const std::vector<Entity>& entities,
            const Tilemap& tilemap,
            float cellSize,
            float dt,
            Axis axis)
        {
            const bool horizontal = axis == Axis::X;
            float& position = horizontal
                ? entity.transform.position.x : entity.transform.position.y;
            float& velocity = horizontal ? entity.velocityX : entity.velocityY;

            const double start = position;
            const double extent = horizontal ? entity.width : entity.height;
            const double sideStart = horizontal
                ? entity.transform.position.y : entity.transform.position.x;
            const double sideExtent = horizontal ? entity.height : entity.width;
            const double delta = static_cast<double>(velocity) * dt;

            if (!std::isfinite(start) || !std::isfinite(sideStart) ||
                !std::isfinite(extent) || !std::isfinite(sideExtent) ||
                !std::isfinite(delta) || extent <= 0.0 || sideExtent <= 0.0) {
                velocity = 0.0f;
                return;
            }
            if (delta == 0.0) return;

            double target = start + delta;
            bool blocked = false;

            // Ограничиваем движение ближайшей пересечённой гранью.
            const auto clip = [&](double obstacleStart, double obstacleExtent,
                double obstacleSide, double obstacleSideExtent) {
                    if (!std::isfinite(obstacleStart) || !std::isfinite(obstacleSide) ||
                        !std::isfinite(obstacleExtent) || !std::isfinite(obstacleSideExtent) ||
                        obstacleExtent <= 0.0 || obstacleSideExtent <= 0.0) return;

                    if (sideStart >= obstacleSide + obstacleSideExtent ||
                        sideStart + sideExtent <= obstacleSide) return;

                    if (delta > 0.0 && start + extent <= obstacleStart &&
                        target + extent >= obstacleStart) {
                        target = std::min(target, obstacleStart - extent);
                        blocked = true;
                    }
                    else if (delta < 0.0 && start >= obstacleStart + obstacleExtent &&
                        target <= obstacleStart + obstacleExtent) {
                        target = std::max(target, obstacleStart + obstacleExtent);
                        blocked = true;
                    }
                };

            for (const auto& other : entities) {
                if (&other == &entity) continue;
                if (horizontal) {
                    clip(other.transform.position.x, other.width,
                        other.transform.position.y, other.height);
                }
                else {
                    clip(other.transform.position.y, other.height,
                        other.transform.position.x, other.width);
                }
            }

            const double left = horizontal ? std::min(start, target) : sideStart;
            const double right = horizontal
                ? std::max(start, target) + extent : sideStart + sideExtent;
            const double top = horizontal ? sideStart : std::min(start, target);
            const double bottom = horizontal
                ? sideStart + sideExtent : std::max(start, target) + extent;

            // Включаем клетки, которых путь касается ровно на границе.
            const double firstX = std::ceil(left / cellSize) - 1.0;
            const double lastX = std::floor(right / cellSize);
            const double firstY = std::ceil(top / cellSize) - 1.0;
            const double lastY = std::floor(bottom / cellSize);

            const auto clipTile = [&](int x, int y) {
                const double tileX = static_cast<double>(x) * cellSize;
                const double tileY = static_cast<double>(y) * cellSize;
                if (horizontal) clip(tileX, cellSize, tileY, cellSize);
                else clip(tileY, cellSize, tileX, cellSize);
                };

            const double candidateCount =
                (lastX - firstX + 1.0) * (lastY - firstY + 1.0);
            const bool fitsCoordinates =
                firstX >= std::numeric_limits<int>::min() &&
                lastX <= std::numeric_limits<int>::max() &&
                firstY >= std::numeric_limits<int>::min() &&
                lastY <= std::numeric_limits<int>::max();

            // Обычно проверяем только клетки вдоль пути. Для очень длинного
            // движения дешевле перебрать занятые клетки разреженной карты.
            if (fitsCoordinates &&
                candidateCount <= static_cast<double>(tilemap.GetTiles().size())) {
                for (std::int64_t y = static_cast<std::int64_t>(firstY);
                    y <= static_cast<std::int64_t>(lastY); ++y) {
                    for (std::int64_t x = static_cast<std::int64_t>(firstX);
                        x <= static_cast<std::int64_t>(lastX); ++x) {
                        const int cellX = static_cast<int>(x);
                        const int cellY = static_cast<int>(y);
                        if (tilemap.IsOccupied(cellX, cellY)) clipTile(cellX, cellY);
                    }
                }
            }
            else {
                for (const auto& [key, tile] : tilemap.GetTiles()) {
                    (void)tile;
                    int x, y;
                    Tilemap::UnpackKey(key, x, y);
                    if (x >= firstX && x <= lastX && y >= firstY && y <= lastY) {
                        clipTile(x, y);
                    }
                }
            }

            // Не записываем бесконечность при выходе за диапазон float.
            const double maxPosition = std::numeric_limits<float>::max();
            if (!std::isfinite(target) || target < -maxPosition || target > maxPosition) {
                velocity = 0.0f;
                return;
            }

            position = static_cast<float>(target);
            if (blocked) {
                // Округление double -> float не должно занести тело за грань.
                const float infinity = std::numeric_limits<float>::infinity();
                if (delta > 0.0 && static_cast<double>(position) > target) {
                    position = std::nextafter(position, -infinity);
                }
                else if (delta < 0.0 && static_cast<double>(position) < target) {
                    position = std::nextafter(position, infinity);
                }
                velocity = 0.0f;
                if (!horizontal && delta > 0.0) entity.isGrounded = true;
            }
        }
    }

    inline void UpdateScene(
        std::vector<Entity>& entities,
        const Tilemap& tilemap,
        float cellSize,
        float dt,
        float gravity)
    {
        if (!std::isfinite(cellSize) || cellSize <= 0.0f ||
            !std::isfinite(dt) || dt <= 0.0f || !std::isfinite(gravity)) return;

        for (auto& entity : entities) {
            if (entity.isStatic) continue;
            entity.isGrounded = false;
            if (entity.useGravity) {
                entity.velocityY = std::min(entity.velocityY + gravity * dt, 1500.0f);
            }
        }

        for (auto& entity : entities) {
            if (!entity.isStatic) {
                Detail::MoveAxis(entity, entities, tilemap, cellSize, dt, Detail::Axis::X);
            }
        }

        for (auto& entity : entities) {
            if (!entity.isStatic) {
                Detail::MoveAxis(entity, entities, tilemap, cellSize, dt, Detail::Axis::Y);
            }
        }
    }
}
