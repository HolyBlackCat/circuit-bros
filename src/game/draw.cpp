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
}
