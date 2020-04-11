#include "menu_controller.h"

#include "game/draw.h"
#include "game/gui_style.h"
#include "game/main.h"
#include "macros/adjust.h"
#include "reflection/full_with_poly.h"

namespace Components
{
    struct MenuController::State
    {
        struct MenuEntryLow
        {
            Sig::MonoSignal<void()> signal; // The entry is not pressable if this is null.
            Graphics::Text text;
            fvec3 color{};
            bool now_hovered = false;
        };

        struct MenuLow
        {
            std::vector<MenuEntryLow> entries;

            ivec2 size{};
            ivec2 pos{};
        };

        std::unique_ptr<MenuLow> menu;

        int LineHeight() const
        {
            return font_main().Height() + GuiStyle::padding_around_text_a.y + GuiStyle::padding_around_text_b.y;
        }
    };

    MenuController::MenuController() : state(std::make_unique<State>()) {}
    MenuController::MenuController(MenuController &&other) noexcept : state(std::move(other.state)) {}
    MenuController &MenuController::operator=(MenuController &&other) noexcept {state = std::move(other.state); return *this;}
    MenuController::~MenuController() = default;

    void MenuController::SetMenu(Menu menu)
    {
        state->menu = std::make_unique<State::MenuLow>();
        State::MenuLow &new_menu = *state->menu;

        for (MenuEntry &entry : menu.entries)
        {
            State::MenuEntryLow &new_entry = new_menu.entries.emplace_back();
            new_entry.signal = std::move(entry.signal);
            new_entry.text = std::move(entry.text);
            new_entry.color = entry.override_color.value_or(new_entry.signal ? GuiStyle::color_text : GuiStyle::color_text_inactive);

            Graphics::Text::Stats text_stats = new_entry.text.ComputeStats();
            ivec2 size = text_stats.size + GuiStyle::padding_around_text_a + GuiStyle::padding_around_text_b;
            clamp_var_min(new_menu.size.x, size.x);
            new_menu.size.y += size.y;
        }

        new_menu.pos = menu.pos;
        clamp_var(new_menu.pos, -screen_size/2 + GuiStyle::popup_min_dist_to_screen_edge, screen_size/2 - GuiStyle::popup_min_dist_to_screen_edge - new_menu.size);
    }

    void MenuController::RemoveMenu()
    {
        state->menu = nullptr;
    }

    bool MenuController::MenuIsOpen() const
    {
        return bool(state->menu);
    }

    void MenuController::Tick(TooltipController *tooltip_controller_opt)
    {
        State &s = *state;

        // If have a menu...
        if (s.menu)
        {
            // If a tooltip controller is provided, remove the tooltip.
            if (tooltip_controller_opt)
                tooltip_controller_opt->RemoveTooltipAndResetTimer();

            // Temporarily move the menu data into a local variable, in case one of the callbacks sets a different menu.
            std::unique_ptr<State::MenuLow> menu = std::move(s.menu);
            bool close_menu = false;
            FINALLY( if (!s.menu && !close_menu) s.menu = std::move(menu); )

            // DO NOT use `s.menu` here. Use `menu` instead.

            bool any_entry_hovered = false;

            for (size_t i = 0; i < menu->entries.size(); i++)
            {
                State::MenuEntryLow &entry = menu->entries[i];
                ivec2 entry_pos = menu->pos with(y += i * s.LineHeight());

                entry.now_hovered = (mouse.pos() >= entry_pos).all() && (mouse.pos() < entry_pos + ivec2(menu->size.x, s.LineHeight())).all();

                if (entry.now_hovered)
                    any_entry_hovered = true; // Note that we do it here, before checking if `entry.signal` is null.

                if (!entry.signal)
                    entry.now_hovered = false;

                if (entry.now_hovered && mouse.left.pressed())
                {
                    entry.signal();
                    close_menu = true;
                }
            }

            if (!any_entry_hovered && Input::Button{}.AssignMouseButton())
                close_menu = true;
        }
    }
    void MenuController::Render() const
    {
        const State &s = *state;

        if (s.menu)
        {
            // Frame
            Draw::RectFrame(s.menu->pos - 1, s.menu->size + 2, 1, true, GuiStyle::color_border, GuiStyle::alpha_border);
            // Entries
            for (size_t i = 0; i < s.menu->entries.size(); i++)
            {
                const State::MenuEntryLow &entry = s.menu->entries[i];

                ivec2 entry_pos = s.menu->pos with(y += i * s.LineHeight());

                // Background
                r.iquad(entry_pos, ivec2(s.menu->size.x, s.LineHeight())).color(entry.now_hovered ? GuiStyle::color_bg_active : GuiStyle::color_bg).alpha(GuiStyle::alpha_bg);

                // Text
                r.itext(entry_pos + GuiStyle::padding_around_text_a, entry.text).color(entry.color).alpha(GuiStyle::alpha_text).align(ivec2(-1));
            }
        }
    }
}
