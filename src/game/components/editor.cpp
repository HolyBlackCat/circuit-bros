#include "editor.h"

#include <array>
#include <vector>

#include "game/draw.h"
#include "game/main.h"
#include "reflection/full_with_poly.h"

namespace Components
{
    struct Editor::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) editor_frame, editor_buttons
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;

        SIMPLE_STRUCT( Strings
            DECL(InterfaceStrings::Str<>)
                ButtonTooltip_Start,
                ButtonTooltip_Pause,
                ButtonTooltip_AdvanceOneTick,
                ButtonTooltip_Stop,
                ButtonTooltip_Continue,
                ButtonTooltip_AddOrGate,
                ButtonTooltip_AddAndGate,
                ButtonTooltip_AddOther,
                ButtonTooltip_Erase
            VERBATIM Strings() {interface_strings().InitStrings(*this, "Editor/");}
        )
        inline static Strings strings;


        static constexpr int panel_h = 24;
        static constexpr ivec2 window_size_with_panel = screen_size - 40, window_size = window_size_with_panel - ivec2(0, panel_h);
        static constexpr ivec2 area_size = ivec2(1024, 512), min_view_offset = -(area_size - window_size) / 2, max_view_offset = -min_view_offset + 1;


        GameState game_state = GameState::stopped;

        bool want_open = false;
        float open_close_state = 0;

        bool partially_extended = false;
        bool fully_extended = false;
        bool prev_fully_extended = false;

        fvec2 view_offset_float{};
        ivec2 view_offset{}; // Camera offset in the editor.
        bool now_dragging_view = false;
        ivec2 view_drag_offset_relative_to_mouse{};
        fvec2 view_offset_vel{};

        ivec2 frame_offset{}; // Offset of the editor frame relative to the center of the screen.
        ivec2 window_offset{}; // Offset of the editor viewport (not counting the panel) relative to the center of the screen.

        struct Button
        {
            static constexpr GameState any_game_state = GameState::_count;
            GameState game_state = any_game_state;

            ivec2 pos{}, size{};
            ivec2 tex_pos{};

            bool State::*target = nullptr;

            using tooltip_func_t = std::string(const State &s);
            tooltip_func_t *tooltip_func = nullptr;

            enum class ButtonState {normal, hovered, pressed}; // Do not reorder.
            ButtonState status = ButtonState::normal;

            bool mouse_pressed_here = false;

            Button() {}
            Button(GameState game_state, ivec2 pos, ivec2 size, ivec2 tex_pos, bool State::*target, tooltip_func_t *tooltip_func)
                : game_state(game_state), pos(pos), size(size), tex_pos(tex_pos), target(target), tooltip_func(tooltip_func)
            {}
        };
        std::vector<Button> buttons;

        bool button_stop = false;
        bool button_start = false;
        bool button_step = false;
        bool button_add_or = false;
        bool button_add_and = false;
        bool button_add_other = false;
        bool button_erase = false;

        State()
        {
            auto IndexToTexPos = [](int index)
            {
                constexpr int wide_button_count = 5;
                ivec2 ret(6 + 24 * index, 0);
                if (index >= wide_button_count)
                    ret.x -= (index - wide_button_count) * 4;
                return ret;
            };

            ivec2 pos = -window_size_with_panel/2 + 2;
            buttons.push_back(Button(GameState::stopped, pos, ivec2(24,20), IndexToTexPos(0), nullptr, nullptr));
            buttons.push_back(Button(GameState::playing, pos, ivec2(24,20), IndexToTexPos(3), &State::button_stop, [](const State &s){return s.strings.ButtonTooltip_Stop();}));
            buttons.push_back(Button(GameState::paused, pos, ivec2(24,20), IndexToTexPos(3), &State::button_stop, [](const State &s){return s.strings.ButtonTooltip_Stop();}));
            pos.x += 24;
            buttons.push_back(Button(GameState::stopped, pos, ivec2(24,20), IndexToTexPos(1), &State::button_start, [](const State &s){return s.strings.ButtonTooltip_Start();}));
            buttons.push_back(Button(GameState::playing, pos, ivec2(24,20), IndexToTexPos(2), &State::button_start, [](const State &s){return s.strings.ButtonTooltip_Pause();}));
            buttons.push_back(Button(GameState::paused, pos, ivec2(24,20), IndexToTexPos(1), &State::button_start, [](const State &s){return s.strings.ButtonTooltip_Continue();}));
            pos.x += 24;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(24,20), IndexToTexPos(4), &State::button_step, [](const State &s){return s.strings.ButtonTooltip_AdvanceOneTick();}));
            pos.x += 24;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(6,20), ivec2(0), nullptr, nullptr));
            pos.x += 6;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(20,20), IndexToTexPos(5), &State::button_add_or, [](const State &s){return s.strings.ButtonTooltip_AddOrGate();}));
            pos.x += 20;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(20,20), IndexToTexPos(6), &State::button_add_and, [](const State &s){return s.strings.ButtonTooltip_AddAndGate();}));
            pos.x += 20;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(20,20), IndexToTexPos(7), &State::button_add_other, [](const State &s){return s.strings.ButtonTooltip_AddOther();}));
            pos.x += 20;
            buttons.push_back(Button(Button::any_game_state, pos, ivec2(20,20), IndexToTexPos(8), &State::button_erase, [](const State &s){return s.strings.ButtonTooltip_Erase();}));
            pos.x += 20;
        }
    };

    Editor::Editor() : state(std::make_unique<State>()) {}
    Editor::Editor(const Editor &other) : state(std::make_unique<State>(*other.state)) {}
    Editor::Editor(Editor &&other) noexcept : state(std::move(other.state)) {}
    Editor &Editor::operator=(Editor other) noexcept {std::swap(state, other.state); return *this;}
    Editor::~Editor() = default;

    bool Editor::IsOpen() const
    {
        return state->want_open;
    }
    void Editor::SetOpen(bool is_open, bool immediately)
    {
        state->want_open = is_open;
        if (immediately)
            state->open_close_state = is_open;
    }
    Editor::GameState Editor::GetState() const
    {
        return state->game_state;
    }

    void Editor::Tick(TooltipController &tooltip_controller)
    {
        static constexpr float open_close_state_step = 0.025;

        State &s = *state;

        { // Open/close
            clamp_var(s.open_close_state += open_close_state_step * (s.want_open ? 1 : -1));
            s.partially_extended = s.open_close_state > 0.001;
            s.fully_extended = s.open_close_state > 0.999;
        }

        // Stop interactions if closed
        if (!s.fully_extended && s.prev_fully_extended)
        {
            s.now_dragging_view = false;
            s.view_offset_vel = fvec2(0);
        }

        bool mouse_in_window = s.fully_extended && (mouse.pos().abs() <= s.window_size/2).all();

        // Change view offset
        if (s.fully_extended)
        {
            // Start dragging
            if (!s.now_dragging_view && mouse_in_window && mouse.right.pressed())
            {
                s.now_dragging_view = true;
                s.view_drag_offset_relative_to_mouse = mouse.pos() + s.view_offset;
                s.view_offset_vel = fvec2(0);
            }
            // Stop dragging
            if (s.now_dragging_view && mouse.right.up())
            {
                s.now_dragging_view = false;
                s.view_offset_vel = -mouse.pos_delta();
            }

            // Change offset
            if (s.now_dragging_view)
            {
                s.view_offset_float = s.view_drag_offset_relative_to_mouse - mouse.pos();
            }
            else
            {
                constexpr float view_offset_vel_drag = 0.05, view_offset_min_vel = 0.25;

                if (s.view_offset_vel != fvec2(0))
                {
                    s.view_offset_float += s.view_offset_vel;

                    s.view_offset_vel *= 1 - view_offset_vel_drag;
                    if ((s.view_offset_vel.abs() < view_offset_min_vel).all())
                        s.view_offset_vel = fvec2(0);
                }
            }

            { // Clamp offset
                for (int i = 0; i < 2; i++)
                {
                    if (s.view_offset_float[i] < s.min_view_offset[i])
                    {
                        s.view_offset_float[i] = s.min_view_offset[i];
                        s.view_offset_vel[i] = 0;
                    }
                    else if (s.view_offset_float[i] > s.max_view_offset[i])
                    {
                        s.view_offset_float[i] = s.max_view_offset[i];
                        s.view_offset_vel[i] = 0;
                    }
                }
            }

            // Compute rounded offset
            s.view_offset = iround(s.view_offset_float);
        }

        { // Compute frame and window offsets
            s.frame_offset = ivec2(0, iround(smoothstep(1 - s.open_close_state) * screen_size.y));
            s.window_offset = ivec2(s.frame_offset.x, s.frame_offset.y + s.panel_h / 2);
        }

        { // Buttons
            for (State::Button &button : s.buttons)
            {
                // Skip if in wrong state.
                if (button.game_state != button.any_game_state && button.game_state != s.game_state)
                {
                    button.mouse_pressed_here = false;
                    button.status = State::Button::ButtonState::normal;
                    continue;
                }

                // Skip if not pressable.
                if (!button.target)
                    continue;

                s.*button.target = false;

                bool hovered = (mouse.pos() >= button.pos).all() && (mouse.pos() < button.pos + button.size).all();

                // Show tooltip if needed
                if (tooltip_controller.ShouldShowTooltip() && button.tooltip_func && hovered)
                    tooltip_controller.SetTooptip(button.pos + ivec2(0,button.size.y), button.tooltip_func(s));

                if (button.mouse_pressed_here && mouse.left.up())
                {
                    button.mouse_pressed_here = false;
                    if (hovered)
                        s.*button.target = true;
                }

                if (hovered && mouse.left.pressed())
                {
                    button.mouse_pressed_here = true;
                }

                if (hovered && button.mouse_pressed_here)
                    button.status = State::Button::ButtonState::pressed;
                else if (hovered && mouse.left.up())
                    button.status = State::Button::ButtonState::hovered;
                else
                    button.status = State::Button::ButtonState::normal;
            }
        }

        { // Update `prev_*` variable
            s.prev_fully_extended = s.fully_extended;
        }
    }
    void Editor::Render() const
    {
        const State &s = *state;

        { // Fade
            static constexpr float fade_alpha = 0.6;

            float t = smoothstep(pow(s.open_close_state, 2));
            float alpha = t * fade_alpha;

            if (s.partially_extended)
            {
                // Fill only the thin stripe around the frame (which should be fully extended at this point).
                constexpr int width = 5; // Depends on the frame texture.
                // Top
                r.iquad(-screen_size/2, ivec2(screen_size.x, width + s.frame_offset.y)).color(fvec3(0)).alpha(alpha);
                // Bottom
                r.iquad(s.frame_offset + ivec2(-screen_size.x/2, screen_size.y/2 - width), ivec2(screen_size.x, width)).color(fvec3(0)).alpha(alpha);
                // Left
                r.iquad(s.frame_offset + ivec2(-screen_size.x/2, -screen_size.y/2 + width), ivec2(width, screen_size.y - width*2)).color(fvec3(0)).alpha(alpha);
                // Right
                r.iquad(s.frame_offset + ivec2(screen_size.x/2 - width, -screen_size.y/2 + width), ivec2(width, screen_size.y - width*2)).color(fvec3(0)).alpha(alpha);
            }
        }

        // Background
        if (s.partially_extended)
            r.iquad(s.frame_offset, s.window_size_with_panel).center().color(fvec3(0)).alpha(0.9);

        // Grid
        if (s.partially_extended)
        {
            constexpr int cell_size = 32, sub_cell_count = 4;
            constexpr fvec3 grid_color(0,0.5,1);
            constexpr float grid_alpha = 0.25, grid_alpha_alt = 0.5;

            auto GetLineAlpha = [&](int index) {return index % sub_cell_count == 0 ? grid_alpha_alt : grid_alpha;};

            ivec2 grid_center_cell = div_ex(-s.view_offset, cell_size);
            ivec2 grid_center = mod_ex(-s.view_offset, cell_size);

            for (int x = 0;; x++)
            {
                int pixel_x = grid_center.x + x * cell_size;
                if (pixel_x > s.window_size.x/2) break;
                r.iquad(s.window_offset + ivec2(pixel_x,-s.window_size.y/2), ivec2(1,s.window_size.y)).color(grid_color).alpha(GetLineAlpha(grid_center_cell.x - x));
            }
            for (int x = -1;; x--)
            {
                int pixel_x = grid_center.x + x * cell_size;
                if (pixel_x < -s.window_size.x/2) break;
                r.iquad(s.window_offset + ivec2(pixel_x,-s.window_size.y/2), ivec2(1,s.window_size.y)).color(grid_color).alpha(GetLineAlpha(grid_center_cell.x - x));
            }
            for (int y = 0;; y++)
            {
                int pixel_y = grid_center.y + y * cell_size;
                if (pixel_y > s.window_size.y/2) break;
                r.iquad(s.window_offset + ivec2(-s.window_size.x/2,pixel_y), ivec2(s.window_size.x,1)).color(grid_color).alpha(GetLineAlpha(grid_center_cell.y - y));
            }
            for (int y = -1;; y--)
            {
                int pixel_y = grid_center.y + y * cell_size;
                if (pixel_y < -s.window_size.y/2) break;
                r.iquad(s.window_offset + ivec2(-s.window_size.x/2,pixel_y), ivec2(s.window_size.x,1)).color(grid_color).alpha(GetLineAlpha(grid_center_cell.y - y));
            }
        }

        // Toolbar
        if (s.partially_extended)
        {
            { // Buttons
                for (const State::Button &button : s.buttons)
                {
                    if (button.game_state != button.any_game_state && button.game_state != s.game_state)
                        continue;

                    r.iquad(button.pos + s.frame_offset, button.size).tex(s.atlas.editor_buttons.pos + button.tex_pos + ivec2(0, button.size.y * int(button.status)));
                }
            }

            { // Minimap
                constexpr fvec3 color_bg = fvec3(0,20,40)/255, color_border = fvec3(0,80,160)/255, color_marker_border(0.75), color_marker_bg(0.2);
                constexpr float alpha_bg = 0.5;

                ivec2 minimap_size(s.panel_h - 6);
                minimap_size.x = minimap_size.x * s.area_size.x / s.area_size.y;

                ivec2 minimap_pos(s.frame_offset.x + s.window_size_with_panel.x/2 - minimap_size.x - 4, s.frame_offset.y - s.window_size_with_panel.y/2 + 4);

                // Background
                r.iquad(minimap_pos, minimap_size).color(color_bg).alpha(alpha_bg);
                // Border
                Draw::RectFrame(minimap_pos-1, minimap_size+2, 1, false, color_border);

                ivec2 rect_size = iround(s.window_size / fvec2(s.area_size) * minimap_size);

                fvec2 relative_view_offset = (s.view_offset - s.min_view_offset) / fvec2(s.max_view_offset -s.min_view_offset);
                ivec2 rect_pos = iround((minimap_size - rect_size) * relative_view_offset);
                r.iquad(minimap_pos + rect_pos, rect_size).color(color_marker_border);
                r.iquad(minimap_pos + rect_pos+1, rect_size-2).color(color_marker_bg);
            }
        }

        // Frame
        if (s.partially_extended)
            r.iquad(s.frame_offset, s.atlas.editor_frame).center();
    }

}
