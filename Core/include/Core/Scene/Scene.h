
#pragma once

#include "Entity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Core
{
    class Scene
    {
    public:
        EntityId CreateEntity();

        bool DestroyEntity(EntityId id);

        Entity* FindEntity(EntityId id);

        const Entity* FindEntity(EntityId id) const;

        std::size_t GetEntityCount() const;

        const std::vector<Entity>& GetEntities() const;

        std::vector<Entity>& GetEntities() { return m_entities; }

        void Clear() {
            m_entities.clear(); 
        }
    private:
        std::vector<Entity> m_entities;

        std::uint64_t m_nextEntityId = 1;
    };
}
