#pragma once

#include "gameutils/adaptive_viewport.h"
#include "gameutils/interface_strings.h"
#include "gameutils/render.h"
#include "graphics/texture_atlas.h"
#include "graphics/texture.h"
#include "input/mouse.h"
#include "interface/window.h"
#include "utils/mat.h"
#include "utils/random.h"

inline constexpr ivec2 screen_size(480, 270);
extern Interface::Window window;

Graphics::Font &font_main();
Graphics::TextureAtlas &texture_atlas();
extern Graphics::Texture texture_main;

extern AdaptiveViewport adaptive_viewport;
extern Render r;

extern Input::Mouse mouse;

extern Random<> rng;

const InterfaceStrings &interface_strings();
