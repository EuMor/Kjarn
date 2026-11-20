
#include "Core/Scene/Scene.h"

#include <algorithm>

namespace Core {

    EntityId Scene::CreateEntity() {
        Entity entity;

        entity.id.value = m_nextEntityId++;

        m_entities.push_back(entity);

        return entity.id;
    }

    bool Scene::DestroyEntity(EntityId id) {
        if (!id.IsValid()) {
            return false;
        }

        const auto it = std::find_if(
            m_entities.begin(),
            m_entities.end(),
            [id](const Entity& entity) {
            return entity.id == id;
        }
        );

        if (it == m_entities.end()) {
            return false;
        }

        m_entities.erase(it);
        return true;
    }

    Entity* Scene::FindEntity(EntityId id) {
        if (!id.IsValid()) {
            return nullptr;
        }

        const auto it = std::find_if(
            m_entities.begin(),
            m_entities.end(),
            [id](const Entity& entity) {
            return entity.id == id;
        }
        );

        if (it == m_entities.end()) {
            return nullptr;
        }

        return &(*it);
    }

    const Entity* Scene::FindEntity(EntityId id) const {
        if (!id.IsValid()) {
            return nullptr;
        }

        const auto it = std::find_if(
            m_entities.begin(),
            m_entities.end(),
            [id](const Entity& entity) {
            return entity.id == id;
        }
        );

        if (it == m_entities.end()) {
            return nullptr;
        }

        return &(*it);
    }

    std::size_t Scene::GetEntityCount() const {
        return m_entities.size();
    }

    const std::vector<Entity>& Scene::GetEntities() const {
        return m_entities;
    }

}
