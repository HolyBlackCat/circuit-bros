#include "editor.h"

#include <array>
#include <vector>
#include <set>

#include "game/components/circuit.h"
#include "game/draw.h"
#include "game/main.h"
#include "reflection/full_with_poly.h"

namespace Components
{
    struct Editor::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) editor_frame, editor_buttons, cursor
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
        static constexpr int mouse_min_drag_distance = 1, hover_radius = 3;


        GameState game_state = GameState::stopped;

        bool want_open = false;
        float open_close_state = 0;

        bool partially_extended = false;
        bool fully_extended = false;
        bool prev_fully_extended = false;

        fvec2 view_offset_float{};
        ivec2 view_offset{}, prev_view_offset{}; // Camera offset in the editor.
        bool now_dragging_view = false;
        bool now_dragging_view_using_rmb = false; // This is set in addition to `now_dragging_view` if needed.
        ivec2 view_drag_offset_relative_to_mouse{};
        fvec2 view_offset_vel{};

        ivec2 frame_offset{}; // Offset of the editor frame relative to the center of the screen.
        ivec2 window_offset{}; // Offset of the editor viewport (not counting the panel) relative to the center of the screen.

        bool mouse_in_window = false;

        NodeStorage held_node;
        bool eraser_mode = false, prev_eraser_mode = false;

        // If not holding a node, this is the index of the currently hovered node.
        // If we do hold a node, this is the index of a node that overlaps with the hovered one.
        size_t hovering_over_node_index = -1; // -1 if no node.
        bool need_recalc_hovered_node = false;

        std::set<std::size_t> selected_node_indices;
        bool selection_add_modifier_down = false, selection_subtract_modifier_down = false;

        bool now_creating_rect_selection = false;
        ivec2 rect_selection_initial_click_pos{};
        ivec2 rect_selection_pos{}, rect_selection_size{};

        bool now_dragging_selected_nodes = false;
        ivec2 dragging_nodes_initial_click_pos{};
        std::vector<ivec2> dragged_nodes_offsets_to_mouse_pos{}; // The indices match `selected_node_indices`.

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

        // Returns -1 if we don't hover over a node.
        size_t CalcHoveredNodeIndex(const std::vector<NodeStorage> &nodes, ivec2 radius) const
        {
            if (!mouse_in_window)
                return -1;

            ivec2 mouse_abs_pos = mouse.pos() - window_offset + view_offset;

            size_t closest_index = -1;
            int closest_dist_sqr = std::numeric_limits<int>::max();

            for (size_t i = 0; i < nodes.size(); i++)
            {
                if (!nodes[i]->VisuallyContainsPoint(mouse_abs_pos, radius))
                    continue;

                int this_dist_sqr = (mouse_abs_pos - nodes[i]->pos).len_sqr();
                if (this_dist_sqr < closest_dist_sqr)
                {
                    closest_dist_sqr = this_dist_sqr;
                    closest_index = i;
                }
            }

            return closest_index;
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

    void Editor::Tick(Circuit &circuit, TooltipController &tooltip_controller)
    {
        static constexpr float open_close_state_step = 0.025;

        State &s = *state;

        { // Open/close
            clamp_var(s.open_close_state += open_close_state_step * (s.want_open ? 1 : -1));
            s.partially_extended = s.open_close_state > 0.001;
            s.fully_extended = s.open_close_state > 0.999;
        }

        // Stop interactions if just closed
        if (!s.fully_extended && s.prev_fully_extended)
        {
            s.now_dragging_view = false;
            s.view_offset_vel = fvec2(0);
            s.now_creating_rect_selection = false;
            s.now_dragging_selected_nodes = false;
            s.dragged_nodes_offsets_to_mouse_pos = std::vector<ivec2>{};
        }

        // Do things if just opened
        if (s.fully_extended && !s.prev_fully_extended)
        {
            s.need_recalc_hovered_node = true;
            s.hovering_over_node_index = -1;
        }

        { // Update mouse-related variables
            // Check if the window is hovered.
            s.mouse_in_window = ((mouse.pos() - s.window_offset).abs() <= s.window_size/2).all();
        }

        bool drag_modifier_down = Input::Button(Input::l_shift).down() || Input::Button(Input::r_shift).down();
        s.selection_add_modifier_down = drag_modifier_down;
        s.selection_subtract_modifier_down = !s.selection_add_modifier_down && (Input::Button(Input::l_ctrl).down() || Input::Button(Input::r_ctrl).down());

        // Change view offset
        if (s.fully_extended)
        {
            // Start dragging
            if (s.fully_extended && s.mouse_in_window && (mouse.middle.pressed() || (drag_modifier_down && mouse.right.pressed())))
            {
                s.now_dragging_view = true;
                s.now_dragging_view_using_rmb = !mouse.middle.pressed();
                s.view_drag_offset_relative_to_mouse = mouse.pos() + s.view_offset;
                s.view_offset_vel = fvec2(0);
            }
            // Stop dragging
            if (s.now_dragging_view && (s.now_dragging_view_using_rmb ? mouse.right : mouse.middle).up())
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

        // Buttons
        if (s.fully_extended)
        {
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

        { // Change editor mode (add/remove node)
            // Disatble tools if not extended
            if (!s.partially_extended)
            {
                s.held_node = nullptr;
                s.eraser_mode = false;
            }

            // Disable tools on right click
            if (s.fully_extended && mouse.right.pressed() && !drag_modifier_down)
            {
                s.held_node = nullptr;
                s.eraser_mode = false;
            }

            bool can_change_mode = !s.now_creating_rect_selection && !s.now_dragging_selected_nodes;

            // Button actions
            if (can_change_mode)
            {
                if (s.button_add_or)
                {
                    s.held_node = Refl::Polymorphic::ConstructFromName<BasicNode>("Or");
                    s.eraser_mode = false;
                }

                if (s.button_add_and)
                {
                    s.held_node = Refl::Polymorphic::ConstructFromName<BasicNode>("And");
                    s.eraser_mode = false;
                }

                if (s.button_erase)
                {
                    s.held_node = nullptr;
                    s.eraser_mode = true;
                }
            }
        }

        { // Detect hovered node if needed
            if (!s.fully_extended)
            {
                s.hovering_over_node_index = -1;
            }
            else if (s.eraser_mode != s.prev_eraser_mode || mouse.pos_delta() || s.view_offset != s.prev_view_offset || s.need_recalc_hovered_node)
            {
                s.need_recalc_hovered_node = false;
                s.hovering_over_node_index = s.CalcHoveredNodeIndex(circuit.nodes, s.held_node ? s.held_node->GetVisualHalfExtent() : ivec2(s.hover_radius));
            }
        }

        // Selection
        if (s.fully_extended)
        {
            ivec2 abs_mouse_pos = mouse.pos() - s.window_offset + s.view_offset;

            // Clear selection when holding a node or when in the eraser mode.
            if (s.held_node || s.eraser_mode)
            {
                s.selected_node_indices.clear();
            }

            // Process clicks in selection or eraser mode.
            if (s.mouse_in_window && !s.held_node)
            {
                // Clicked on an empty space, form a rectangular selection
                if (mouse.left.pressed() && s.hovering_over_node_index == size_t(-1))
                {
                    s.now_creating_rect_selection = true;
                    s.rect_selection_initial_click_pos = mouse.pos() - s.window_offset + s.view_offset;
                }

                // Clicked on a node
                if (mouse.left.released() && s.hovering_over_node_index != size_t(-1) && !s.now_creating_rect_selection && !s.now_dragging_selected_nodes)
                {
                    if (s.eraser_mode)
                    {
                        circuit.nodes.erase(circuit.nodes.begin() + s.hovering_over_node_index);
                        s.hovering_over_node_index = -1;
                        s.need_recalc_hovered_node = true;
                    }
                    else if (s.selection_add_modifier_down)
                    {
                        s.selected_node_indices.insert(s.hovering_over_node_index); // Add node to selection.
                    }
                    else if (s.selection_subtract_modifier_down)
                    {
                        s.selected_node_indices.erase(s.hovering_over_node_index); // Remove node from selection.
                    }
                    else if (!s.selected_node_indices.contains(s.hovering_over_node_index))
                    {
                        s.selected_node_indices = {s.hovering_over_node_index}; // Replace selection.
                    }
                }

                // Start dragging
                if (mouse.left.pressed() && !s.selection_add_modifier_down && !s.selection_subtract_modifier_down
                    && s.hovering_over_node_index != size_t(-1) && s.selected_node_indices.contains(s.hovering_over_node_index))
                {
                    s.now_dragging_selected_nodes = true;

                    s.dragging_nodes_initial_click_pos = abs_mouse_pos;

                    s.dragged_nodes_offsets_to_mouse_pos.clear();
                    s.dragged_nodes_offsets_to_mouse_pos.reserve(s.selected_node_indices.size());

                    for (size_t index : s.selected_node_indices)
                        s.dragged_nodes_offsets_to_mouse_pos.push_back(circuit.nodes[index]->pos - abs_mouse_pos);

                    s.dragged_nodes_offsets_to_mouse_pos.shrink_to_fit();
                }
            }

            // Process a rectangular selection
            if (s.now_creating_rect_selection)
            {
                if (mouse.left.up())
                {
                    // Released mouse button, determine which nodes should be selected or erased
                    s.now_creating_rect_selection = false;

                    auto NodeIsInSelection = [&](const NodeStorage &node)
                    {
                        ivec2 half_extent = node->GetVisualHalfExtent();
                        return (node->pos - half_extent >= s.rect_selection_pos).all() && (node->pos + half_extent < s.rect_selection_pos + s.rect_selection_size).all();
                    };

                    if (s.eraser_mode)
                    {
                        s.hovering_over_node_index = -1;
                        s.need_recalc_hovered_node = true;

                        std::erase_if(circuit.nodes, NodeIsInSelection);
                    }
                    else
                    {
                        if (!s.selection_add_modifier_down && !s.selection_subtract_modifier_down)
                            s.selected_node_indices.clear();

                        for (size_t i = 0; i < circuit.nodes.size(); i++)
                        {
                            if (!NodeIsInSelection(circuit.nodes[i]))
                                continue;

                            if (s.selection_subtract_modifier_down)
                                s.selected_node_indices.erase(i);
                            else
                                s.selected_node_indices.insert(i);
                        }
                    }
                }
                else
                {
                    // Still selecting, update rectangle bounds
                    ivec2 abs_mouse_pos = mouse.pos() - s.window_offset + s.view_offset;

                    s.rect_selection_pos = min(s.rect_selection_initial_click_pos, abs_mouse_pos);
                    s.rect_selection_size = max(s.rect_selection_initial_click_pos, abs_mouse_pos) - s.rect_selection_pos + 1;
                }
            }

            // Drag selected nodes
            if (s.now_dragging_selected_nodes)
            {
                if (mouse.left.up())
                {
                    DebugAssertNameless(s.selected_node_indices.size() == s.dragged_nodes_offsets_to_mouse_pos.size());

                    s.now_dragging_selected_nodes = false;

                    bool can_move = true;

                    // Make sure the nodes are not dragged out of bounds
                    if (can_move)
                    {
                        for (size_t i = 0; i < s.selected_node_indices.size(); i++)
                        {
                            ivec2 new_pos = abs_mouse_pos + s.dragged_nodes_offsets_to_mouse_pos[i];
                            if ((new_pos < -s.area_size/2).any() || (new_pos > s.area_size/2).any())
                            {
                                can_move = false;
                                break;
                            }
                        }
                    }

                    // Make sure the dragged nodes don't overlap with the other nodes.
                    if (can_move)
                    {
                        auto sel_index_iter = s.selected_node_indices.begin();
                        for (size_t static_node_index = 0; static_node_index < circuit.nodes.size(); static_node_index++)
                        {
                            // Skip node indices that are selected.
                            if (sel_index_iter != s.selected_node_indices.end() && *sel_index_iter == static_node_index)
                            {
                                sel_index_iter++;
                                continue;
                            }

                            const BasicNode &static_node = *circuit.nodes[static_node_index];

                            size_t i = 0;
                            for (size_t moving_node_index : s.selected_node_indices)
                            {
                                const BasicNode &moving_node = *circuit.nodes[moving_node_index];

                                ivec2 new_moving_node_pos = abs_mouse_pos + s.dragged_nodes_offsets_to_mouse_pos[i++];

                                if (static_node.VisuallyContainsPoint(new_moving_node_pos, moving_node.GetVisualHalfExtent()))
                                {
                                    can_move = false;
                                    break;
                                }
                            }

                            if (!can_move)
                                break;
                        }
                    }

                    // If there's something wrong with the new node positions, move them back to their original location.
                    if (!can_move)
                    {
                        s.need_recalc_hovered_node = true;

                        size_t i = 0;
                        for (size_t index : s.selected_node_indices)
                        {
                            circuit.nodes[index]->pos = s.dragging_nodes_initial_click_pos + s.dragged_nodes_offsets_to_mouse_pos[i++];
                        }
                    }
                }
                else
                {
                    size_t i = 0;
                    for (size_t index : s.selected_node_indices)
                    {
                        circuit.nodes[index]->pos = abs_mouse_pos + s.dragged_nodes_offsets_to_mouse_pos[i++];
                    }
                }
            }
        }

        // Add a node
        if (s.fully_extended)
        {
            if (mouse.left.pressed() && s.mouse_in_window && s.held_node && s.hovering_over_node_index == size_t(-1))
            {
                circuit.nodes.push_back(s.held_node);
                circuit.nodes.back()->pos = mouse.pos() - s.window_offset + s.view_offset;
                s.need_recalc_hovered_node = true;
            }
        }

        { // Update `prev_*` variables
            s.prev_fully_extended = s.fully_extended;
            s.prev_eraser_mode = s.eraser_mode;
            s.prev_view_offset = s.view_offset;
        }
    }
    void Editor::Render(const Circuit &circuit) const
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

        // Circuit
        if (s.partially_extended)
        {
            // Set scissor box
            r.Finish();
            Graphics::Scissor::Enable();
            FINALLY( r.Finish(); Graphics::Scissor::Disable(); )
            Graphics::Scissor::SetBounds_FlipY(screen_size/2 + s.window_offset - s.window_size/2, s.window_size, screen_size.y);

            // Render nodes
            for (const NodeStorage &node : circuit.nodes)
            {
                if (((node->pos - s.view_offset).abs() > s.window_size/2 + node->GetVisualHalfExtent()).any())
                    continue;

                node->Render(s.window_offset - s.view_offset);
            }

            // Indicators on selected nodes
            for (size_t index : s.selected_node_indices)
            {
                const BasicNode &node = *circuit.nodes[index];
                ivec2 half_extent = node.GetVisualHalfExtent() + 2;

                Draw::RectFrame(s.window_offset + node.pos - s.view_offset - half_extent+1, half_extent*2-1, 1, true, fvec3(31,240,255)/255, 143/255.f);
            }

            // Indicator on a hovered node
            if (s.hovering_over_node_index != size_t(-1) && !s.now_creating_rect_selection && !s.now_dragging_selected_nodes)
            {
                const BasicNode &node = *circuit.nodes[s.hovering_over_node_index];
                ivec2 half_extent = node.GetVisualHalfExtent() + 3;

                fvec4 color = s.held_node || s.eraser_mode ? fvec4(1,55/255.f,0,0.5) : fvec4(0,81,255,100)/255.f;

                Draw::RectFrame(s.window_offset + node.pos - s.view_offset - half_extent+1, half_extent*2-1, 1, true, color.to_vec3(), color.a);
            }

            // Rectangular selection
            if (s.now_creating_rect_selection && s.rect_selection_size != 1)
            {
                fvec4 color = s.eraser_mode ? fvec4(1,55/255.f,0,0.5) : fvec4(71,243,255,173)/255;

                Draw::RectFrame(s.rect_selection_pos - s.view_offset + s.window_offset, s.rect_selection_size, 1, true, color.to_vec3(), color.a);
            }
        }

        // Active node or tool
        if (s.partially_extended && s.mouse_in_window)
        {
            // A node
            if (s.held_node)
            {
                // The node itself
                s.held_node->Render(mouse.pos() + s.frame_offset);

                // And indicator
                r.iquad(s.frame_offset + mouse.pos() + ivec2(5), s.atlas.cursor.region(ivec2(16,0), ivec2(16))).center();
            }

            // Eraser indicator
            if (s.eraser_mode)
                r.iquad(s.frame_offset + mouse.pos() + ivec2(5), s.atlas.cursor.region(ivec2(32,0), ivec2(16))).center();

            // Selection modifier
            if (!s.held_node && !s.eraser_mode)
            {
                if (s.selection_add_modifier_down)
                    r.iquad(s.frame_offset + mouse.pos() + ivec2(5), s.atlas.cursor.region(ivec2(48,0), ivec2(16))).center();
                else if (s.selection_subtract_modifier_down)
                    r.iquad(s.frame_offset + mouse.pos() + ivec2(5), s.atlas.cursor.region(ivec2(64,0), ivec2(16))).center();
            }
        }

        // Frame
        if (s.partially_extended)
            r.iquad(s.frame_offset, s.atlas.editor_frame).center();

        // Cursor
        if (s.partially_extended)
        {
            if (window.HasMouseFocus())
                r.iquad(mouse.pos(), s.atlas.cursor.region(ivec2(0), ivec2(16))).center().alpha(smoothstep(pow(s.open_close_state, 1.5)));
        }
    }

}
