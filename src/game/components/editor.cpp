#include "editor.h"

#include "game/main.h"
#include "reflection/full_with_poly.h"

namespace Components
{
    struct Editor::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) editor_frame
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;


        static constexpr int panel_h = 24;
        static constexpr ivec2 window_size_with_panel = screen_size - 40, window_size = window_size_with_panel - ivec2(0, panel_h);
        static constexpr ivec2 area_size = ivec2(1024, 512), min_view_offset = -(area_size - window_size) / 2, max_view_offset = -min_view_offset + 1;


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

    void Editor::Tick()
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

        { // Update `prev_*` variable
            s.prev_fully_extended = s.fully_extended;
        }
    }
    void Editor::Render() const
    {
        const State &s = *state;

        { // Fade
            float t = smoothstep(pow(s.open_close_state, 2));
            float alpha = t * 2/3;

            if (s.partially_extended)
            {
                // Fill entire screen.
                r.iquad(ivec2(0), screen_size).center().color(fvec3(0)).alpha(alpha);
            }
            else if (s.fully_extended)
            {
                // Fill only the thin stripe around the frame (which should be fully extended at this point).
                constexpr int width = 5; // Depends on the frame texture.
                // Top
                r.iquad(-screen_size/2, ivec2(screen_size.x, width)).color(fvec3(0)).alpha(alpha);
                // Bottom
                r.iquad(ivec2(-screen_size.x/2, screen_size.y/2 - width), ivec2(screen_size.x, width)).color(fvec3(0)).alpha(alpha);
                // Left
                r.iquad(ivec2(-screen_size.x/2, -screen_size.y/2 + width), ivec2(width, screen_size.y - width*2)).color(fvec3(0)).alpha(alpha);
                // Right
                r.iquad(ivec2(screen_size.x/2 - width, -screen_size.y/2 + width), ivec2(width, screen_size.y - width*2)).color(fvec3(0)).alpha(alpha);
            }
        }

        // Background
        if (s.partially_extended)
            r.iquad(s.frame_offset, s.window_size_with_panel).center().color(fvec3(0));

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

        // Minimap
        if (s.partially_extended)
        {
            constexpr fvec3 color_bg(0,0,0), color_border(0,0.5,1), color_marker_border(0.75), color_marker_bg(0.2);

            ivec2 minimap_size(s.panel_h - 5);
            minimap_size.x = minimap_size.x * s.area_size.x / s.area_size.y;

            ivec2 minimap_pos(s.frame_offset.x + s.window_size_with_panel.x/2 - minimap_size.x - 3, s.frame_offset.y - s.window_size_with_panel.y/2 + 3);

            // Border
            r.iquad(minimap_pos-1, minimap_size+2).color(color_border);
            // Background
            r.iquad(minimap_pos, minimap_size).color(color_bg);

            ivec2 rect_size = iround(s.window_size / fvec2(s.area_size) * minimap_size);

            fvec2 relative_view_offset = (s.view_offset - s.min_view_offset) / fvec2(s.max_view_offset -s.min_view_offset);
            ivec2 rect_pos = iround((minimap_size - rect_size) * relative_view_offset);
            r.iquad(minimap_pos + rect_pos, rect_size).color(color_marker_border);
            r.iquad(minimap_pos + rect_pos+1, rect_size-2).color(color_marker_bg);
        }

        // Frame
        if (s.partially_extended)
            r.iquad(s.frame_offset, s.atlas.editor_frame).center();
    }

}
