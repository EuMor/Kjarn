
#pragma once

#include <cstdint>

namespace Core
{
    struct EntityId
    {
        std::uint64_t value = 0;

        bool IsValid() const
        {
            return value != 0;
        }

        bool operator==(const EntityId& other) const
        {
            return value == other.value;
        }

        bool operator!=(const EntityId& other) const
        {
            return value != other.value;
        }
    };
}
