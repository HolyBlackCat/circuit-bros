#include "draw.h"

#include "game/main.h"

namespace Draw
{
    void RectFrame(ivec2 pos, ivec2 size, int width, bool corners, fvec3 color, float alpha, float beta)
    {
        bool no_corners = !corners;
        r.iquad(ivec2(pos.x + width*no_corners, pos.y), ivec2(size.x - width*2*no_corners, width)).color(color).alpha(alpha).beta(beta);
        r.iquad(ivec2(pos.x + width*no_corners, pos.y + size.y - width), ivec2(size.x - width*2*no_corners, width)).color(color).alpha(alpha).beta(beta);
        r.iquad(ivec2(pos.x, pos.y + width), ivec2(width, size.y - width*2)).color(color).alpha(alpha).beta(beta);
        r.iquad(ivec2(pos.x + size.x - width, pos.y + width), ivec2(width, size.y - width*2)).color(color).alpha(alpha).beta(beta);
    }

    Render::Quad_t Line(fvec2 pos_a, fvec2 pos_b, int width)
    {
        fvec2 delta = pos_b - pos_a;
        bool v = abs(delta.x) < abs(delta.y); // True if the line is closer to being vertical than horizontal.
        int dir = sign(delta[v]); // Sign of `delta.x` (if `v == false`) or `delta.y` (otherwise).
        pos_a[v] -= dir * 0.25;
        pos_b[v] += dir * 0.25;
        delta[v] += dir * 0.5;

        return r.fquad(pos_a, fvec2(1)).center(fvec2(0,0.5)).matrix(fmat2(delta, fvec2::dir4(!v) * width));
    }
}
