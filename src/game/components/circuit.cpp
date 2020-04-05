#include "circuit.h"

#include "game/draw.h"
#include "game/main.h"

namespace Components
{
    namespace Nodes
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) nodes
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        static Atlas atlas;

        STRUCT( Or EXTENDS BasicNode )
        {
            UNNAMED_MEMBERS()

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(3);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(0, 2 + 7*powered), ivec2(7))).center(ivec2(3));
            }
        };

        STRUCT( And EXTENDS BasicNode )
        {
            UNNAMED_MEMBERS()

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(5);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(7, 11*powered), ivec2(11))).center(ivec2(5));
            }
        };
    }

    void BasicNode::DrawConnection(ivec2 window_offset, ivec2 pos_src, ivec2 pos_dst, bool is_inverted, bool is_powered, float src_visual_radius)
    {
        // +4 is for a good measure, because `src_visual_radius` is floor-ed, and we also draw some decoration at src.
        bool src_visible = (pos_src.abs() <= screen_size/2 + src_visual_radius + int(src_visual_radius) + 4).all();

        if (pos_src == pos_dst)
            return;

        pos_src += window_offset;
        pos_dst += window_offset;

        fvec2 a = pos_src + 0.5;
        fvec2 b = pos_dst + 0.5;

        fvec2 src_deco_pos;

        if (src_visible)
        {
            fvec2 dir = b - a;
            float dist = dir.len();
            dir /= dist;

            float max_src_visual_radius = dist / 2;
            clamp_var_max(src_visual_radius, max_src_visual_radius);

            a += dir * src_visual_radius;
            src_deco_pos = a;
            if (is_inverted)
                a += dir * 2.3;
        }

        Draw::Line(a, b, 1).tex(Nodes::atlas.nodes.pos + fvec2(0,is_powered+0.5), fvec2(5,0));

        if (src_visible)
        {
            r.iquad(iround(src_deco_pos-0.5), Nodes::atlas.nodes.region(ivec2(0,16+5*is_powered+10*is_inverted), ivec2(5))).center(fvec2(2));
        }
    }
}
