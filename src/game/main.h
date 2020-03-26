#pragma once

#include "gameutils/adaptive_viewport.h"
#include "gameutils/render.h"
#include "graphics/texture_atlas.h"
#include "graphics/texture.h"
#include "input/mouse.h"
#include "interface/window.h"
#include "utils/mat.h"

inline constexpr ivec2 screen_size(480, 270);
extern Interface::Window window;

extern Graphics::TextureAtlas texture_atlas;
extern Graphics::Texture texture_main;

extern AdaptiveViewport adaptive_viewport;
extern Render r;

extern Input::Mouse mouse;
