#pragma once
#include "Core/Scene/Entity.h"
#include <vector>
#include <cmath>

namespace Core {
    namespace Physics {
        inline bool CheckCollision(const Entity& a, const Entity& b) {
            return (a.transform.position.x < b.transform.position.x + b.width &&
                a.transform.position.x + a.width > b.transform.position.x &&
                a.transform.position.y < b.transform.position.y + b.height &&
                a.transform.position.y + a.height > b.transform.position.y);
        }

        // Теперь физика обновляет ВСЕ объекты на сцене
        inline void UpdateScene(std::vector<Entity>& entities, float dt, float gravity) {
            // 1. Применяем гравитацию ко всем динамическим объектам
            for (auto& e : entities) {
                if (!e.isStatic && e.useGravity) {
                    e.velocityY += gravity * dt;
                    if (e.velocityY > 1500.0f) e.velocityY = 1500.0f; // Макс скорость падения
                    e.isGrounded = false;
                }
            }

            // 2. Двигаем по X и решаем коллизии по X
            for (auto& e : entities) {
                if (e.isStatic) continue;
                e.transform.position.x += e.velocityX * dt;

                for (auto& other : entities) {
                    if (&e == &other) continue; // Не проверяем сам с собой
                    if (CheckCollision(e, other)) {
                        if (e.velocityX > 0) e.transform.position.x = other.transform.position.x - e.width;
                        else if (e.velocityX < 0) e.transform.position.x = other.transform.position.x + other.width;
                        e.velocityX = 0;
                    }
                }
            }

            // 3. Двигаем по Y и решаем коллизии по Y
            for (auto& e : entities) {
                if (e.isStatic) continue;
                e.transform.position.y += e.velocityY * dt;

                for (auto& other : entities) {
                    if (&e == &other) continue;
                    if (CheckCollision(e, other)) {
                        if (e.velocityY > 0) { // Падаем вниз на объект
                            e.transform.position.y = other.transform.position.y - e.height;
                            e.velocityY = 0;
                            e.isGrounded = true;
                        }
                        else if (e.velocityY < 0) { // Прыгаем вверх и бьемся головой
                            e.transform.position.y = other.transform.position.y + other.height;
                            e.velocityY = 0;
                        }
                    }
                }
            }
        }
    }
}