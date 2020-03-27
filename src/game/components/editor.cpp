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


        static constexpr ivec2 window_size = screen_size - 40;


        bool want_open = false;
        float open_close_state = 0;

        bool partially_extended = false;
        bool fully_extended = false;
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

        clamp_var(s.open_close_state += open_close_state_step * (s.want_open ? 1 : -1));
        s.partially_extended = s.open_close_state > 0.001;
        s.fully_extended = s.open_close_state > 0.999;
    }
    void Editor::Render() const
    {
        const State &s = *state;

        ivec2 window_offset = ivec2(0, [&]{
            float t = 1 - s.open_close_state;
            t = smoothstep(t);
            return iround(t * screen_size.y);
        }());

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
            r.iquad(window_offset, s.window_size).center().color(fvec3(0));

        // Frame
        if (s.partially_extended)
            r.iquad(window_offset, s.atlas.editor_frame).center();
    }

}
