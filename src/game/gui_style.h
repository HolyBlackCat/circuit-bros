#pragma once

#include "utils/mat.h"

namespace GuiStyle
{
    inline constexpr fvec3 color_text(1), color_text_inactive(0.6), color_bg(0), color_bg_active(0.1,0.25,1), color_border(0.2,0.5,1);
    inline constexpr float alpha_text = 1, alpha_bg = 0.8, alpha_border = 0.8;

    inline constexpr ivec2 padding_around_text_a = ivec2(2, 1), /* top-left */ padding_around_text_b = ivec2(1, 1); /* bottom-right */
    inline constexpr int popup_min_dist_to_screen_edge = 2; // This doesn't affect the 1px border around popups, so the actual distance is 1px less than this value.
}
