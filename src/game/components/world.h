#pragma once

#include <memory>
#include <string>

namespace Components
{
    class World
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        World(std::string level_name);
        World(const World &);
        World(World &&);
        World &operator=(const World &);
        World &operator=(World &&);
        ~World();

        void CopyPersistentStateFrom(const World &other);

        void Tick();
        void PersistentTick();

        void Render() const;
    };
}
