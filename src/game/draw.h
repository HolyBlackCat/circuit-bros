#pragma once

#include "utils/mat.h"

namespace Draw
{
    void RectFrame(ivec2 pos, ivec2 size, int width, bool corners, fvec3 color, float alpha = 1, float beta = 1);

    // If `width` is even, it's recommended to offset the coordinates by `0.5`.
    Render::Quad_t Line(fvec2 pos_a, fvec2 pos_b, int width);
}
