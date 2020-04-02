#include "tooltip_controller.h"

#include "game/draw.h"
#include "game/main.h"
#include "reflection/full_with_poly.h"

namespace Components
{
    struct TooltipController::State
    {
        static constexpr ivec2 padding_around_text_a = ivec2(2, 1), padding_around_text_b = ivec2(1, 1);

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

    void TooltipController::Tick()
    {
        State &s = *state;

        s.ticks_since_mouse_moved++;
        if (mouse.pos_delta())
        {
            s.ticks_since_mouse_moved = 0;
            s.show_tooltip = false;
            s.tooltip_text = Graphics::Text{};
            s.tooltip_text_stats = Graphics::Text::Stats{};
        }
    }
    void TooltipController::Render() const
    {
        constexpr fvec3 color_text(1), color_bg(0), color_border(0.2,0.5,1);
        constexpr float alpha_text = 1, alpha_bg = 0.5, alpha_border = 0.8;

        constexpr ivec2 offset(2,2);
        constexpr int screen_edge_min_dist = 2;

        const State &s = *state;

        if (s.show_tooltip)
        {
            ivec2 pos = s.tooltip_pos + s.padding_around_text_a + offset;
            clamp_var(pos, -screen_size/2 + s.padding_around_text_a + screen_edge_min_dist, screen_size/2 - s.tooltip_text_stats.size - s.padding_around_text_b - screen_edge_min_dist);

            // Background
            r.iquad(pos - s.padding_around_text_a, s.tooltip_text_stats.size + s.padding_around_text_a + s.padding_around_text_b).color(color_bg).alpha(alpha_bg);
            // Frame
            Draw::RectFrame(pos - s.padding_around_text_a - 1, s.tooltip_text_stats.size + s.padding_around_text_a + s.padding_around_text_b + 2, 1, false, color_border, alpha_border);
            // Text
            r.itext(pos, s.tooltip_text).align(ivec2(-1,-1),-1).color(color_text).alpha(alpha_text);
        }
    }

}
