#pragma once

#include <memory>
#include <string_view>

#include "game/main.h"
#include "utils/mat.h"

namespace Components
{
    class TooltipController
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        TooltipController();
        TooltipController(const TooltipController &);
        TooltipController(TooltipController &&) noexcept;
        TooltipController &operator=(TooltipController) noexcept;
        ~TooltipController();

        // Returns true if the mouse wasn't moved for a while.
        // Then it's a good time to call `SetTooptip()`.
        [[nodiscard]] bool ShouldShowTooltip() const;

        // Show tooltip with the specified text.
        // The tooltip will be removed when user moves mouse.
        void SetTooptip(ivec2 pos, std::string text);
        void RemoveTooltipAndResetTimer();

        void Tick();
        void Render() const;
    };
}
