#pragma once

#include <exception>
#include <string>

#include "game/main.h"
#include "gameutils/tiled_map.h"
#include "macros/adjust.h"
#include "meta/misc.h"
#include "program/errors.h"
#include "reflection/full.h"
#include "stream/readonly_data.h"
#include "utils/json.h"
#include "utils/multiarray.h"

namespace Components::Game
{
    class Map
    {
      public:
        static constexpr int tile_size = 12;

        enum class TileType
        {
            air,
            stone,
            spike,
            _count,
        };

        REFL_SIMPLE_STRUCT( Tile
            REFL_DECL(TileType REFL_INIT=TileType::air) mid
            REFL_VERBATIM unsigned char random = 0;
        )

        using array_t = Array2D<Tile>;

      private:
        array_t tiles;

        static const Graphics::TextureAtlas::Region &AtlasRegion()
        {
            static Graphics::TextureAtlas::Region ret = texture_atlas().Get("tiles.png");
            return ret;
        }

        Tiled::PointLayer point_layer;

      public:
        Map() {}

        Map(std::string file_name)
        {
            try
            {
                Json json(Stream::ReadOnlyData(file_name).string(), 32);

                // Load tile layers
                Meta::cexpr_for<Refl::Class::member_count<Tile>>([&](auto index)
                {
                    try
                    {
                        constexpr auto i = index.value;

                        Json::View layer_json = Tiled::FindLayer(json.GetView(), Refl::Class::MemberName<Tile>(i));
                        if (!layer_json)
                            Program::Error("Layer not found.");

                        Tiled::TileLayer layer = Tiled::LoadTileLayer(layer_json);

                        if constexpr (i == 0)
                        {
                            tiles = array_t(layer.size());
                        }
                        else
                        {
                            if (tiles.size() != layer.size())
                                Program::Error("The size of this layer doesn't match the size of the other layers.");
                        }

                        for (auto pos : index_vec2(0) <= vector_range < tiles.size())
                        {
                            int this_tile_as_int = layer.unsafe_at(pos);
                            TileType this_tile = TileType(this_tile_as_int);
                            if (this_tile > TileType::_count)
                                Program::Error("Tile at ", pos, " has invalid index #", this_tile_as_int, ".");

                            Refl::Class::Member<i>(tiles.unsafe_at(pos)) = this_tile;
                        }
                    }
                    catch (std::exception &e)
                    {
                        Program::Error("While processing layer `", Refl::Class::MemberName<Tile>(index.value), "`:\n", e.what());
                    }
                });

                // Make a random layer
                for (auto pos : index_vec2(0) <= vector_range < tiles.size())
                    tiles.unsafe_at(pos).random = rng.integer();

                // Load points
                auto point_layer_view = Tiled::FindLayer(json.GetView(), "objects");
                if (!point_layer_view)
                    Program::Error("The `objects` layer is missing.");
                point_layer = Tiled::LoadPointLayer(point_layer_view);
            }
            catch (std::exception &e)
            {
                Program::Error("While opening map `", file_name, "`:\n", e.what());
            }
        }

        [[nodiscard]] const array_t &Tiles() const
        {
            return tiles;
        }

        [[nodiscard]] const Tiled::PointLayer &Points() const
        {
            return point_layer;
        }

        template <int LayerIndex>
        void Render(Meta::value_tag<LayerIndex>, ivec2 camera_pos, ivec2 viewport_size = screen_size) const
        {
            ivec2 corner_a = div_ex(camera_pos - viewport_size/2, tile_size);
            ivec2 corner_b = div_ex(camera_pos + viewport_size/2, tile_size);

            auto DrawTile = [&](ivec2 screen_pixel_pos, ivec2 tex_pos, ivec2 tex_size = ivec2(1))
            {
                r.iquad(screen_pixel_pos, AtlasRegion().region(tex_pos * tile_size, tex_size * tile_size));
            };

            for (ivec2 pos : corner_a <= vector_range <= corner_b)
            {
                ivec2 pixel_pos = pos * tile_size - camera_pos;

                auto tile_stack = tiles.try_get(pos);
                TileType tile = Refl::Class::Member<LayerIndex>(tile_stack);
                unsigned char random = tile_stack.random;

                auto SameAs = [&](ivec2 offset, std::initializer_list<TileType> list = {})
                {
                    TileType this_tile = Refl::Class::Member<LayerIndex>(tiles.try_get(pos + offset));
                    if (list.size() == 0)
                        return this_tile == tile;
                    else
                        return std::find(list.begin(), list.end(), this_tile) != list.end();
                };

                switch (tile)
                {
                  case TileType::_count:
                  case TileType::air:
                    // Nothing.
                    break;
                  case TileType::stone:
                    {
                        unsigned char mask = 0;

                        for (int i = 0; i < 8; i++)
                            mask = mask << 1 | SameAs(ivec2::dir8(i));

                        int state = 0, variant = 0;
                        if (mask == 0b11111111)
                        {
                            variant = std::array{0,1,2,3,3}[random % 5];
                        }
                        else if ((mask & 0b10001000) == 0b10001000 && ((mask & 0b00100000) == 0 || (mask & 0b01010000) == 0))
                        {
                            state = 1;
                            variant = 2 + random % 2;
                        }
                        else if (mask & 0b10000000 && (mask & 0b01100000) != 0b01100000)
                        {
                            state = 1;
                            variant = 0;
                        }
                        else if (mask & 0b00001000 && (mask & 0b00110000) != 0b00110000)
                        {
                            state = 1;
                            variant = 1;
                        }
                        else
                        {
                            variant = std::array{0,0,0,1,1,2}[random % 6];
                        }

                        DrawTile(pixel_pos, ivec2(0 + state, 1 + variant));

                        // Grass
                        if (SameAs(ivec2(0,-1), {TileType::air, TileType::spike}))
                        {
                            bool grass_l = mask & 0b00001000 && SameAs(ivec2(-1,-1), {TileType::air, TileType::spike});
                            bool grass_r = mask & 0b10000000 && SameAs(ivec2( 1,-1), {TileType::air, TileType::spike});

                            int state = -1;
                            if (grass_l && grass_r)
                                state = random / 3 % 2;
                            else if (grass_r)
                                state = 2;
                            else if (grass_l)
                                state = 3;

                            if (state != -1)
                                DrawTile(pixel_pos with(y -= tile_size), ivec2(0 + state, 5), ivec2(1,2));
                        }
                    }
                    break;
                  case TileType::spike:
                    DrawTile(pixel_pos, ivec2(4 + bool(random & 1), 5 + bool(random & 2)));
                    break;
                }
            }
        }

        [[nodiscard]] static bool EnumIsSolid(TileType tile)
        {
            switch (tile)
            {
              case TileType::_count:
              case TileType::air:
              case TileType::spike:
                return false;
              case TileType::stone:
                return true;
            }
            return false;
        }

        [[nodiscard]] bool TileIsSolid(ivec2 pos) const
        {
            if (!tiles.pos_in_range(pos))
                return false;
            return EnumIsSolid(tiles.unsafe_at(pos).mid);
        }

        [[nodiscard]] bool PixelIsSolid(ivec2 pixel) const
        {
            return TileIsSolid(div_ex(pixel, tile_size));
        }
    };
}
