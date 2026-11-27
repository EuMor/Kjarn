
#pragma once

#include "Entity.h"
#include "Core/Tilemap.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Core
{
    class Scene
    {
    public:
        EntityId CreateEntity();
        Entity* CreateEntityWithId(EntityId id);

        bool DestroyEntity(EntityId id);

        Entity* FindEntity(EntityId id);

        const Entity* FindEntity(EntityId id) const;

        std::size_t GetEntityCount() const;

        const std::vector<Entity>& GetEntities() const;

        std::vector<Entity>& GetEntities() { return m_entities; }

        void Clear() {
            m_entities.clear(); 
            m_tilemap.Clear();
        }

        Tilemap& GetTilemap() { return m_tilemap; }
        const Tilemap& GetTilemap() const { return m_tilemap; }

    private:
        std::vector<Entity> m_entities;

        std::uint64_t m_nextEntityId = 1;
        Tilemap m_tilemap;
    };
}
