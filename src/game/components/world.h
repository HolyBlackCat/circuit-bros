#pragma once

#include <memory>
#include <string>

#include "macros/maybe_const.h"

namespace Components
{
    class World
    {
      public:
        struct State;

      private:
        std::unique_ptr<State> state;

      public:
        World(std::string level_name);
        World(const World &);
        World(World &&);
        World &operator=(const World &);
        World &operator=(World &&);
        ~World();

        void CopyPersistentStateFrom(const World &other);

        MAYBE_CONST( CV State &GetState() CV; )

        void Tick();
        void PersistentTick();

        void Render() const;
    };
}
