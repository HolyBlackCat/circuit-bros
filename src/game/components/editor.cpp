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
                ButtonTooltip_Erase,
                ButtonTooltip_ConnectionMode_Regular,
                ButtonTooltip_ConnectionMode_Inverted
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

        size_t node_connection_src_node_index = -1; // -1 if no node. This is set when clicking any unselected node in selection mode.
        bool now_creating_node_connection = false; // Then, if stop hovering that node, `now_creating_node_connection` is set to true.
        int node_connection_src_point_index = -1; // This has indeterminate value if `node_connection_src_node_index == -1`.
        bool create_inverted_connections = false; // Whether new connections should be inverted.

        size_t erasing_node_connection_node_index = -1; // -1 if no node. This is set when clicking a node in eraser mode.
        int erasing_node_connection_point_index = -1; // -1 if no node or no point. Set when clicking a node in eraser mode.
        bool erasing_node_connection_point_type_is_out = false; // Same, but can sometimes be updated when selecting a connection, if the points overlap.
        int erasing_node_connection_con_index = -1; // Continuously updated when selecting a connection to be erased (can be -1 if no connection), otherwise -1.
        fvec2 erasing_node_connection_pos_a{}, erasing_node_connection_pos_b{}; // Those are set only when `erasing_node_connection_con_index != -1`.
        bool now_erasing_connections_instead_of_nodes = false; // Has a meaningful value only if `(mouse.left.down() || mouse.left.released()) && eraser_mode`.

        std::vector<BasicNode::id_t> recently_deleted_node_ids; // When deleting nodes, their IDs should be added here.

        static constexpr int circuit_tick_period_when_in_editor_mode = 15;
        int circuit_tick_timer_for_editor_mode = 0;

        struct Button
        {
            ivec2 pos{}, size{};
            ivec2 tex_pos{};

            using tick_func_t = void(Button &button, State &state, TooltipController &tooltip_controller);
            tick_func_t *tick = nullptr;

            bool enabled = true;

            enum class Status {normal, hovered, pressed}; // Do not reorder.
            Status status = Status::normal;

            bool mouse_pressed_here = false;
            bool mouse_released_here_at_this_tick = false;

            Button() {}

            Button(ivec2 pos, ivec2 size, ivec2 tex_pos, tick_func_t *tick = nullptr, bool enabled = true)
                : pos(pos), size(size), tex_pos(tex_pos), tick(tick), enabled(enabled)
            {}

            [[nodiscard]] static ivec2 IndexToTexPos(int index)
            {
                constexpr int wide_button_count = 5;
                ivec2 ret(6 + 24 * index, 0);
                if (index >= wide_button_count)
                    ret.x -= (index - wide_button_count) * 4;
                return ret;
            }

            [[nodiscard]] bool IsPressed() const
            {
                return mouse_released_here_at_this_tick;
            }

            // Internal, use in `tick` callback.
            template <typename F>
            void TooltipFunc(TooltipController &c, F &&func)
            {
                if (status == Status::hovered && c.ShouldShowTooltip())
                    c.SetTooltip(pos with(y += size.y), std::forward<F>(func)());
            }
        };

        struct Buttons
        {
            MEMBERS( DECL(Button)
                stop, start_pause_continue, advance_one_tick,
                separator1,
                add_gate_or, add_gate_and, add_gate_other,
                erase_gate,
                separator2,
                toggle_inverted_connections
            )

            MAYBE_CONST(
                template <typename F>
                void ForEachButton(F &&func) CV // `func` is `void func(CV Button &)`.
                {
                    Meta::cexpr_for<Refl::Class::member_count<Buttons>>([&](auto index)
                    {
                        func(Refl::Class::Member<index.value>(*this));
                    });
                }
            )

            Buttons()
            {
                ivec2 pos = -window_size_with_panel/2 + 2;

                stop = Button(pos, ivec2(24,20), Button::IndexToTexPos(0), [](Button &b, State &s, TooltipController &t)
                {
                    if (s.game_state == GameState::stopped)
                    {
                        b.enabled = false;
                        b.tex_pos = Button::IndexToTexPos(0);
                    }
                    else
                    {
                        b.enabled = true;
                        b.tex_pos = Button::IndexToTexPos(3);
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_Stop();});
                    }
                });
                pos.x += 24;
                start_pause_continue = Button(pos, ivec2(24,20), Button::IndexToTexPos(1), [](Button &b, State &s, TooltipController &t)
                {
                    b.tex_pos = Button::IndexToTexPos(s.game_state == GameState::playing ? 2 : 1);
                    switch (s.game_state)
                    {
                      case GameState::_count:
                        break; // This shouldn't happen.
                      case GameState::stopped:
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_Start();});
                        break;
                      case GameState::playing:
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_Pause();});
                        break;
                      case GameState::paused:
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_Continue();});
                        break;
                    }
                });
                pos.x += 24;
                advance_one_tick = Button(pos, ivec2(24,20), Button::IndexToTexPos(4), [](Button &b, State &s, TooltipController &t)
                {
                    b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_AdvanceOneTick();});
                });
                pos.x += 24;
                separator1 = Button(pos, ivec2(6,20), ivec2(0), nullptr, false);
                pos.x += 6;
                add_gate_or = Button(pos, ivec2(20,20), Button::IndexToTexPos(5), [](Button &b, State &s, TooltipController &t)
                {
                    b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_AddOrGate();});
                });
                pos.x += 20;
                add_gate_and = Button(pos, ivec2(20,20), Button::IndexToTexPos(6), [](Button &b, State &s, TooltipController &t)
                {
                    b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_AddAndGate();});
                });
                pos.x += 20;
                add_gate_other = Button(pos, ivec2(20,20), Button::IndexToTexPos(7), [](Button &b, State &s, TooltipController &t)
                {
                    b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_AddOther();});
                });
                pos.x += 20;
                erase_gate = Button(pos, ivec2(20,20), Button::IndexToTexPos(8), [](Button &b, State &s, TooltipController &t)
                {
                    b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_Erase();});
                });
                pos.x += 20;
                separator2 = Button(pos, ivec2(6,20), ivec2(0), nullptr, false);
                pos.x += 6;
                toggle_inverted_connections = Button(pos, ivec2(20,20), Button::IndexToTexPos(9), [](Button &b, State &s, TooltipController &t)
                {
                    b.tex_pos = Button::IndexToTexPos(s.create_inverted_connections ? 10 : 9);
                    if (s.create_inverted_connections)
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_ConnectionMode_Inverted();});
                    else
                        b.TooltipFunc(t,[&]{return s.strings.ButtonTooltip_ConnectionMode_Regular();});
                });
                pos.x += 20;
            }
        };
        Buttons buttons;

        struct Hotkeys
        {
            Input::Button stop = Input::r;
            Input::Button play_pause = Input::space;
            Input::Button advance_one_tick = Input::f;
        };
        Hotkeys hotkeys;

        State() {}

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

        static void RunWorldTick(World &world, Circuit &circuit)
        {
            circuit.Tick();
            world.Tick();
        }
        static void RunWorldTickPersistent(World &world)
        {
            world.PersistentTick();
        }
    };

    Editor::Editor() : state(std::make_unique<State>()) {}
    Editor::Editor(Editor &&other) noexcept : state(std::move(other.state)) {}
    Editor &Editor::operator=(Editor &&other) noexcept {state = std::move(other.state); return *this;}
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

    void Editor::Tick(std::optional<World> &world, const std::optional<World> &saved_world, Circuit &circuit, MenuController &menu_controller, TooltipController &tooltip_controller)
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

            s.now_creating_node_connection = false;
            s.node_connection_src_node_index = -1;
            s.erasing_node_connection_node_index = -1;

            menu_controller.RemoveMenu();
            tooltip_controller.RemoveTooltipAndResetTimer();
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
        if (s.partially_extended) // Sic
        {
            s.buttons.ForEachButton([&](State::Button &button)
            {
                if (button.tick)
                    button.tick(button, s, tooltip_controller);

                button.mouse_released_here_at_this_tick = false;

                // Skip if disabled.
                if (!button.enabled)
                {
                    button.status = State::Button::Status::normal;
                    button.mouse_pressed_here = false;
                    button.mouse_released_here_at_this_tick = false;
                    return;
                }

                bool hovered = s.fully_extended && (mouse.pos() >= button.pos).all() && (mouse.pos() < button.pos + button.size).all();

                bool can_press = !s.now_creating_rect_selection && !s.now_dragging_selected_nodes;

                if (button.mouse_pressed_here && mouse.left.up())
                {
                    button.mouse_pressed_here = false;
                    if (hovered && can_press)
                        button.mouse_released_here_at_this_tick = true;
                }

                if (hovered && mouse.left.pressed() && can_press)
                {
                    button.mouse_pressed_here = true;
                }

                if (hovered && button.mouse_pressed_here)
                    button.status = State::Button::Status::pressed;
                else if (hovered && mouse.left.up())
                    button.status = State::Button::Status::hovered;
                else
                    button.status = State::Button::Status::normal;
            });
        }

        { // Change editor mode (add/remove node)
            // Disable tools if not extended
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
        }

        { // Button actions
            if (s.buttons.add_gate_or.IsPressed())
            {
                s.held_node = Refl::Polymorphic::ConstructFromName<BasicNode>("Or");
                s.held_node->Tick(circuit);
                s.eraser_mode = false;
            }

            if (s.buttons.add_gate_and.IsPressed())
            {
                s.held_node = Refl::Polymorphic::ConstructFromName<BasicNode>("And");
                s.held_node->Tick(circuit);
                s.eraser_mode = false;
            }

            if (s.buttons.erase_gate.IsPressed())
            {
                s.held_node = nullptr;
                s.eraser_mode = true;
            }

            if (s.buttons.toggle_inverted_connections.IsPressed())
            {
                s.create_inverted_connections = !s.create_inverted_connections;
            }

            if (s.buttons.stop.IsPressed() || s.hotkeys.stop.pressed())
            {
                s.game_state = GameState::stopped;
                if (world && saved_world)
                {
                    World tmp_world = std::move(*world);
                    world.emplace(*saved_world);
                    world->CopyPersistentStateFrom(tmp_world);
                }
                circuit.RestoreState();
            }
            if (s.buttons.start_pause_continue.IsPressed() || s.hotkeys.play_pause.pressed())
            {
                if (s.game_state == GameState::stopped)
                    circuit.SaveState();

                if (s.game_state == GameState::playing)
                    s.game_state = GameState::paused;
                else
                    s.game_state = GameState::playing;
            }
            if (s.buttons.advance_one_tick.IsPressed() || s.hotkeys.advance_one_tick.pressed())
            {
                s.game_state = GameState::paused;
                if (world)
                    s.RunWorldTick(*world, circuit);
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

        // Selection (and erasing nodes)
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
                if (mouse.left.pressed() && s.hovering_over_node_index == size_t(-1) && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                {
                    s.now_creating_rect_selection = true;
                    s.rect_selection_initial_click_pos = mouse.pos() - s.window_offset + s.view_offset;
                }

                // Clicked on a node
                if (mouse.left.released() && s.hovering_over_node_index != size_t(-1) && !s.now_creating_rect_selection
                    && !s.now_dragging_selected_nodes && !s.now_creating_node_connection && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                {
                    if (s.eraser_mode)
                    {
                        if (!s.now_erasing_connections_instead_of_nodes) // If not erasing a connection...
                        {
                            s.recently_deleted_node_ids.push_back(circuit.nodes[s.hovering_over_node_index]->id);
                            circuit.nodes.erase(circuit.nodes.begin() + s.hovering_over_node_index);
                            s.hovering_over_node_index = -1;
                            s.need_recalc_hovered_node = true;
                        }
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
                    && s.hovering_over_node_index != size_t(-1) && s.selected_node_indices.contains(s.hovering_over_node_index)
                    && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
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

                    if (!menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                    {
                        auto NodeIsInSelection = [&](const NodeStorage &node)
                        {
                            ivec2 half_extent = node->GetVisualHalfExtent();
                            return (node->pos - half_extent >= s.rect_selection_pos).all() && (node->pos + half_extent < s.rect_selection_pos + s.rect_selection_size).all();
                        };

                        if (s.eraser_mode)
                        {
                            s.hovering_over_node_index = -1;
                            s.need_recalc_hovered_node = true;

                            std::erase_if(circuit.nodes, [&](const NodeStorage &node)
                            {
                                if (!NodeIsInSelection(node))
                                    return false;
                                s.recently_deleted_node_ids.push_back(node->id);
                                return true;
                            });
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

                    if (!menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                    {
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

        // Creating node connections
        if (s.fully_extended)
        {
            bool can_create_con = !s.held_node && !s.eraser_mode;

            if (can_create_con && mouse.left.pressed() && s.hovering_over_node_index != size_t(-1) && !s.selection_add_modifier_down
                && !s.selection_subtract_modifier_down && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
            {
                ivec2 mouse_abs_pos = mouse.pos() - s.window_offset + s.view_offset;

                if ((s.node_connection_src_point_index = circuit.nodes[s.hovering_over_node_index]->GetClosestConnectionPoint<BasicNode::Dir::out>(mouse_abs_pos)) != -1)
                {
                    // If successfully found a connection node...
                    s.node_connection_src_node_index = s.hovering_over_node_index;
                }
            }

            if (can_create_con && s.node_connection_src_node_index != size_t(-1) && mouse.left.down() && s.node_connection_src_node_index != s.hovering_over_node_index)
                s.now_creating_node_connection = true;

            if (mouse.left.up())
            {
                if (can_create_con && s.now_creating_node_connection && s.node_connection_src_node_index != size_t(-1)
                    && s.hovering_over_node_index != size_t(-1) && s.hovering_over_node_index != s.node_connection_src_node_index && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                {
                    ivec2 mouse_abs_pos = mouse.pos() - s.window_offset + s.view_offset;

                    BasicNode &src_node = *circuit.nodes[s.node_connection_src_node_index];
                    BasicNode &dst_node = *circuit.nodes[s.hovering_over_node_index];
                    int dst_point_index = dst_node.GetClosestConnectionPoint<BasicNode::Dir::in>(mouse_abs_pos);
                    if (dst_point_index != -1)
                    {
                        src_node.Connect(s.node_connection_src_point_index, dst_node, dst_point_index, s.create_inverted_connections);
                    }
                }

                s.node_connection_src_node_index = -1;
                s.now_creating_node_connection = false;
            }
        }

        // Erasing node connections
        if (s.fully_extended)
        {
            ivec2 mouse_abs_pos = mouse.pos() - s.window_offset + s.view_offset;

            if (s.eraser_mode && mouse.left.pressed() && s.hovering_over_node_index != size_t(-1) && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
            {
                s.now_erasing_connections_instead_of_nodes = false;

                s.erasing_node_connection_point_index = circuit.nodes[s.hovering_over_node_index]->GetClosestConnectionPoint<BasicNode::Dir::in_out>(mouse_abs_pos, &s.erasing_node_connection_point_type_is_out);
                if (s.erasing_node_connection_point_index != -1)
                {
                    // If successfully found a connection point...
                    s.erasing_node_connection_node_index = s.hovering_over_node_index;
                    s.erasing_node_connection_con_index = -1;
                }
            }

            if (s.eraser_mode && s.erasing_node_connection_node_index != size_t(-1) && mouse.left.down())
            {
                s.erasing_node_connection_con_index = -1;

                if (s.erasing_node_connection_node_index != s.hovering_over_node_index)
                {
                    s.now_erasing_connections_instead_of_nodes = true;

                    const BasicNode &node = *circuit.nodes[s.erasing_node_connection_node_index];

                    float dist_to_nearest_con = 10; // Minimal distance to connection.

                    auto UpdateSelectedCon = [&](bool is_out, int point_index)
                    {
                        Meta::with_cexpr_flags(is_out) >> [&](auto is_out_tag)
                        {
                            constexpr bool is_out = is_out_tag.value;

                            const auto &point = node.GetInOrOutPoint<is_out>(point_index);

                            for (int con_index = 0; con_index < int(point.connections.size()); con_index++)
                            {
                                const BasicNode::NodeAndPointId &remote_ids = point.connections[con_index].ids;

                                const BasicNode &remote_node = *circuit.FindNodeOrThrow(remote_ids.node);
                                const auto &remote_point = remote_node.GetInOrOutPoint<!is_out>(remote_ids.point);

                                ivec2 a = node.pos + point.info->offset_to_node;
                                ivec2 b = remote_node.pos + remote_point.info->offset_to_node;
                                fvec2 dir = fvec2(b - a).norm();
                                if (fvec2(mouse_abs_pos - a) /dot/ dir <= 0)
                                    continue;

                                fvec2 normal = dir.rot90();

                                float dist = abs(fvec2(mouse_abs_pos - a) /dot/ normal);
                                if (dist < dist_to_nearest_con)
                                {
                                    // Update the nearest connection info.
                                    dist_to_nearest_con = dist;
                                    s.erasing_node_connection_con_index = con_index;
                                    s.erasing_node_connection_pos_a = a + dir * point.info->visual_radius; // Note that we don't add `extra_out_visual_radius` here, it looks better without it.
                                    s.erasing_node_connection_pos_b = b - dir * remote_point.info->visual_radius; // ^

                                    // Yeah, we also need to set those because of how we handle overlapping connection points.
                                    s.erasing_node_connection_point_index = point_index;
                                    s.erasing_node_connection_point_type_is_out = is_out;
                                }
                            }
                        };
                    };

                    UpdateSelectedCon(s.erasing_node_connection_point_type_is_out, s.erasing_node_connection_point_index);

                    int overlapping_point_index = s.erasing_node_connection_point_type_is_out ? node.GetInPointOverlappingOutPoint(s.erasing_node_connection_point_index)
                                                                                              : node.GetOutPointOverlappingInPoint(s.erasing_node_connection_point_index);
                    if (overlapping_point_index != -1)
                        UpdateSelectedCon(!s.erasing_node_connection_point_type_is_out, overlapping_point_index);
                }
            }

            if (mouse.left.up())
            {
                if (s.eraser_mode && s.erasing_node_connection_node_index != size_t(-1) && s.erasing_node_connection_con_index != -1 && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
                {
                    BasicNode &node = *circuit.nodes[s.erasing_node_connection_node_index];
                    node.Disconnect(circuit, s.erasing_node_connection_point_index, s.erasing_node_connection_point_type_is_out, s.erasing_node_connection_con_index);
                }

                s.erasing_node_connection_node_index = -1;
                s.erasing_node_connection_point_index = -1;
                s.erasing_node_connection_con_index = -1;
            }
        }

        // Add a node
        if (s.fully_extended)
        {
            if (mouse.left.pressed() && s.mouse_in_window && s.held_node && s.hovering_over_node_index == size_t(-1) && !menu_controller.MenuIsOpen() && s.game_state == GameState::stopped)
            {
                BasicNode::id_t new_node_id = circuit.nodes.empty() ? 0 : circuit.nodes.back()->id + 1;

                BasicNode &new_node = *circuit.nodes.emplace_back(s.held_node);
                new_node.pos = mouse.pos() - s.window_offset + s.view_offset;
                new_node.id = new_node_id;

                s.need_recalc_hovered_node = true;
            }
        }

        { // Renormalize nodes, if needed (this must be close to the end of `Tick()`, after all node manipulations)
            if (s.recently_deleted_node_ids.size() > 0)
            {
                // Sort IDs to allow binary search.
                std::sort(s.recently_deleted_node_ids.begin(), s.recently_deleted_node_ids.end());

                // Check if a node with the specified ID was recently deleted.
                auto NodeIdWasDeleted = [&](BasicNode::id_t id)
                {
                    return std::binary_search(s.recently_deleted_node_ids.begin(), s.recently_deleted_node_ids.end(), id);
                };

                // For each existing node, remove all connections to nodes that were deleted.
                for (NodeStorage &node : circuit.nodes)
                {
                    int in_points = node->InPointCount();
                    int out_points = node->OutPointCount();

                    for (int i = 0; i < in_points; i++)
                    {
                        BasicNode::InPoint &point = node->GetInPoint(i);
                        std::erase_if(point.connections, [&](const BasicNode::InPointCon &con){return NodeIdWasDeleted(con.ids.node);});
                    }
                    for (int i = 0; i < out_points; i++)
                    {
                        BasicNode::OutPoint &point = node->GetOutPoint(i);
                        std::erase_if(point.connections, [&](const BasicNode::OutPointCon &con){return NodeIdWasDeleted(con.ids.node);});
                    }
                }

                // Clear the list of deleted IDs.
                s.recently_deleted_node_ids.clear();
            }
        }

        { // Circuit tick (in the editor mode only)
            if (s.game_state != GameState::stopped)
            {
                s.circuit_tick_timer_for_editor_mode = 0;
            }
            else
            {
                s.circuit_tick_timer_for_editor_mode++;
                if (s.circuit_tick_timer_for_editor_mode >= s.circuit_tick_period_when_in_editor_mode)
                {
                    s.circuit_tick_timer_for_editor_mode = 0;
                    circuit.Tick();
                }
            }
        }

        { // World tick (has to be done after the circuit tick)
            if (world)
            {
                if (s.game_state == GameState::playing)
                    s.RunWorldTick(*world, circuit);

                s.RunWorldTickPersistent(*world);
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
            // constexpr fvec3 grid_color(0,0.5,1);
            constexpr fvec3 grid_color(0.1,0.2,0.5);
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
                s.buttons.ForEachButton([&](const State::Button &button)
                {
                    r.iquad(button.pos + s.frame_offset, button.size).tex(s.atlas.editor_buttons.pos + button.tex_pos + ivec2(0, button.size.y * int(button.status)));
                });
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

            // Render node connections
            for (const NodeStorage &node_ptr : circuit.nodes)
            {
                const BasicNode &dst_node = *node_ptr;

                int dst_point_count = dst_node.InPointCount();
                for (int i = 0; i < dst_point_count; i++)
                {
                    const BasicNode::InPoint &dst_point = dst_node.GetInPoint(i);

                    for (const BasicNode::InPointCon &in_con : dst_point.connections)
                    {
                        const BasicNode &src_node = *circuit.FindNodeOrThrow(in_con.ids.node);
                        const BasicNode::OutPoint &src_point = src_node.GetOutPoint(in_con.ids.point);

                        BasicNode::DrawConnection(s.window_offset, src_node.pos + src_point.info->offset_to_node - s.view_offset, dst_node.pos + dst_point.info->offset_to_node - s.view_offset,
                            in_con.is_inverted, src_point.is_powered ^ in_con.is_inverted, src_point.info->visual_radius + src_point.info->extra_out_visual_radius, dst_point.info->visual_radius);
                    }
                }
            }

            // Render a connection that's being created
            if (s.now_creating_node_connection)
            {
                const BasicNode &src_node = *circuit.nodes[s.node_connection_src_node_index];
                const BasicNode::OutPoint &src_point = src_node.GetOutPoint(s.node_connection_src_point_index);

                float visual_radius = src_point.info->visual_radius + src_point.info->extra_out_visual_radius;
                BasicNode::DrawConnection(s.window_offset, src_node.pos + src_point.info->offset_to_node - s.view_offset, mouse.pos() - s.window_offset, s.create_inverted_connections, false, visual_radius, 0);
            }

            // Indicators on selected nodes
            for (size_t index : s.selected_node_indices)
            {
                const BasicNode &node = *circuit.nodes[index];
                ivec2 half_extent = node.GetVisualHalfExtent() + 2;

                Draw::RectFrame(s.window_offset + node.pos - s.view_offset - half_extent+1, half_extent*2-1, 1, true, fvec3(31,240,255)/255, 143/255.f);
            }

            // Indicator on a hovered node
            if (s.hovering_over_node_index != size_t(-1) && !s.now_creating_rect_selection && !s.now_dragging_selected_nodes
                && (!s.eraser_mode || mouse.left.up() || !s.now_erasing_connections_instead_of_nodes))
            {
                const BasicNode &node = *circuit.nodes[s.hovering_over_node_index];
                ivec2 half_extent = node.GetVisualHalfExtent() + 3;

                fvec4 color = s.held_node || s.eraser_mode ? fvec4(1,55/255.f,0,0.5) : fvec4(0,81,255,100)/255.f;

                Draw::RectFrame(s.window_offset + node.pos - s.view_offset - half_extent+1, half_extent*2-1, 1, true, color.to_vec3(), color.a);
            }

            // Indicator on a hovered connection (when in eraser mode)
            if (s.eraser_mode && s.erasing_node_connection_node_index != size_t(-1) && s.erasing_node_connection_con_index != -1)
            {
                constexpr float half_size = 2;
                constexpr fvec3 frame_color(1,55/255.f,0);
                constexpr float frame_alpha = 0.5;

                fvec2 a = s.erasing_node_connection_pos_a - s.view_offset + s.window_offset;
                fvec2 b = s.erasing_node_connection_pos_b - s.view_offset + s.window_offset;
                fvec2 delta = b - a;
                float dist = clamp_min(delta.len(), 1);
                fvec2 dir = delta / dist;
                fvec2 normal = dir.rot90();

                r.fquad(a + 0.5, fvec2(dist, half_size * 2 - 1)).center(fvec2(0, half_size - 0.5)).matrix(fmat2(dir, normal)).color(frame_color).alpha(frame_alpha);
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
    }

    void Editor::RenderCursor() const
    {
        const State &s = *state;

        // Cursor
        if (s.partially_extended)
        {
            if (window.HasMouseFocus())
                r.iquad(mouse.pos(), s.atlas.cursor.region(ivec2(0), ivec2(16))).center().alpha(smoothstep(pow(s.open_close_state, 1.5)));
        }
    }
}
