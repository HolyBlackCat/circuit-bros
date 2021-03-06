#pragma once

#include <memory>
#include <optional>

#include "game/components/circuit.h"
#include "game/components/menu_controller.h"
#include "game/components/tooltip_controller.h"
#include "game/components/world.h"

namespace Components
{
    class Editor
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        Editor();
        Editor(Editor &&) noexcept;
        Editor &operator=(Editor &&) noexcept;
        ~Editor();

        [[nodiscard]] bool IsOpen() const;
        void SetOpen(bool is_open, bool immediately = false);

        enum class GameState {stopped, playing, paused, _count};
        GameState GetState() const;

        void Tick(std::optional<World> &world, const std::optional<World> &saved_world, Circuit &circuit, MenuController &menu_controller, TooltipController &tooltip_controller);
        void Render(const Circuit &circuit) const;
        void RenderCursor() const;
    };
}
