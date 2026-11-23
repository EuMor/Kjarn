#pragma once
#include "Core/Scene/Entity.h" 
#include <vector>              
#include <cmath>

namespace Core {
    namespace Physics {
        // Проверка пересечения двух прямоугольников (AABB)
        inline bool CheckCollision(const Entity& a, const Entity& b) {
            return (a.transform.position.x < b.transform.position.x + b.width &&
                a.transform.position.x + a.width > b.transform.position.x &&
                a.transform.position.y < b.transform.position.y + b.height &&
                a.transform.position.y + a.height > b.transform.position.y);
        }

        // Разрешение коллизий для игрока (упрощенное)
        inline void ResolvePlayerCollision(Entity& player, const std::vector<Entity>& scene, float dt) {
            player.isGrounded = false;

            // Двигаем по X
            player.transform.position.x += player.velocityX * dt;
            for (const auto& other : scene) {
                if (&player == &other || other.isPlayer || !other.isStatic) continue;
                if (CheckCollision(player, other)) {
                    if (player.velocityX > 0) player.transform.position.x = other.transform.position.x - player.width;
                    if (player.velocityX < 0) player.transform.position.x = other.transform.position.x + other.width;
                    player.velocityX = 0;
                }
            }

            // Двигаем по Y
            player.transform.position.y += player.velocityY * dt;
            for (const auto& other : scene) {
                if (&player == &other || other.isPlayer || !other.isStatic) continue;
                if (CheckCollision(player, other)) {
                    if (player.velocityY > 0) { // Падаем вниз
                        player.transform.position.y = other.transform.position.y - player.height;
                        player.velocityY = 0;
                        player.isGrounded = true;
                    }
                    else if (player.velocityY < 0) { // Прыгаем вверх
                        player.transform.position.y = other.transform.position.y + other.height;
                        player.velocityY = 0;
                    }
                }
            }
        }
    }
}