#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game/components/tooltip_controller.h"
#include "game/main.h"
#include "graphics/text.h"
#include "signals/signal_slot.h"
#include "utils/mat.h"

namespace Components
{
    class MenuController
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        MenuController();
        MenuController(MenuController &&) noexcept;
        MenuController &operator=(MenuController &&) noexcept;
        ~MenuController();

        struct MenuEntry
        {
            using signal_t = Sig::MonoSignal<void()>;
            signal_t signal; // The entry is not pressable if this is null.

            Graphics::Text text;
            std::optional<fvec3> override_color;

            MenuEntry() {}

            MenuEntry(signal_t signal, Graphics::Text text, std::optional<fvec3> override_color = {})
                : signal(std::move(signal)), text(std::move(text)), override_color(std::move(override_color))
            {}
        };

        struct Menu
        {
            ivec2 pos{};
            std::vector<MenuEntry> entries;
        };

        void SetMenu(Menu menu);
        void RemoveMenu();

        bool MenuIsOpen() const;

        void Tick(TooltipController *tooltip_controller_opt);
        void Render() const;
    };
}
