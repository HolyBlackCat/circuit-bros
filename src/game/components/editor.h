#pragma once

#include <memory>

#include "game/components/tooltip_controller.h"

namespace Components
{
    class Editor
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        Editor();
        Editor(const Editor &);
        Editor(Editor &&) noexcept;
        Editor &operator=(Editor) noexcept;
        ~Editor();

        [[nodiscard]] bool IsOpen() const;
        void SetOpen(bool is_open, bool immediately = false);

        enum class GameState {stopped, playing, paused, _count};
        GameState GetState() const;

        void Tick(TooltipController &tooltip_controller);
        void Render() const;
    };
}
