#include "tooltip_controller.h"

#include "game/draw.h"
#include "game/gui_style.h"
#include "game/main.h"

namespace Components
{
    struct TooltipController::State
    {
        int ticks_since_mouse_moved = 0;

        bool show_tooltip = false;
        ivec2 tooltip_pos{};
        Graphics::Text tooltip_text;
        Graphics::Text::Stats tooltip_text_stats;
    };

    TooltipController::TooltipController() : state(std::make_unique<State>()) {}
    TooltipController::TooltipController(const TooltipController &other) : state(std::make_unique<State>(*other.state)) {}
    TooltipController::TooltipController(TooltipController &&other) noexcept : state(std::move(other.state)) {}
    TooltipController &TooltipController::operator=(TooltipController other) noexcept {std::swap(state, other.state); return *this;}
    TooltipController::~TooltipController() = default;

    bool TooltipController::ShouldShowTooltip() const
    {
        return state->ticks_since_mouse_moved == 30;
    }

    void TooltipController::SetTooptip(ivec2 pos, std::string text)
    {
        state->show_tooltip = true;
        state->tooltip_pos = pos;
        state->tooltip_text = Graphics::Text(font_main(), text);
        state->tooltip_text_stats = state->tooltip_text.ComputeStats();
    }
    void TooltipController::RemoveTooltipAndResetTimer()
    {
        State &s = *state;
        s.ticks_since_mouse_moved = 0;
        s.show_tooltip = false;
        s.tooltip_text = Graphics::Text{};
        s.tooltip_text_stats = Graphics::Text::Stats{};
    }

    void TooltipController::Tick()
    {
        State &s = *state;

        s.ticks_since_mouse_moved++;
        if (mouse.pos_delta())
            RemoveTooltipAndResetTimer();
    }
    void TooltipController::Render() const
    {
        constexpr ivec2 offset(2,2);

        const State &s = *state;

        if (s.show_tooltip)
        {
            ivec2 pos = s.tooltip_pos + GuiStyle::padding_around_text_a + offset;
            clamp_var(pos, -screen_size/2 + GuiStyle::padding_around_text_a + GuiStyle::popup_min_dist_to_screen_edge,
                      screen_size/2 - s.tooltip_text_stats.size - GuiStyle::padding_around_text_b - GuiStyle::popup_min_dist_to_screen_edge);

            // Background
            r.iquad(pos - GuiStyle::padding_around_text_a, s.tooltip_text_stats.size + GuiStyle::padding_around_text_a + GuiStyle::padding_around_text_b).color(GuiStyle::color_bg).alpha(GuiStyle::alpha_bg);
            // Frame
            Draw::RectFrame(pos - GuiStyle::padding_around_text_a - 1, s.tooltip_text_stats.size + GuiStyle::padding_around_text_a + GuiStyle::padding_around_text_b + 2, 1, false, GuiStyle::color_border, GuiStyle::alpha_border);
            // Text
            r.itext(pos, s.tooltip_text).align(ivec2(-1,-1),-1).color(GuiStyle::color_text).alpha(GuiStyle::alpha_text);
        }
    }

}
