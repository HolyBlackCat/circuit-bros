#pragma once

#include <exception>
#include <string>

#include "game/main.h"
#include "gameutils/tiled_map.h"
#include "meta/misc.h"
#include "program/errors.h"
#include "reflection/full.h"
#include "stream/readonly_data.h"
#include "utils/json.h"
#include "utils/multiarray.h"

class Map
{
  public:
    static constexpr int tile_size = 12;

    enum class TileType
    {
        air,
        stone,
        _count,
    };

    REFL_SIMPLE_STRUCT( Tile
        REFL_DECL(TileType REFL_INIT=TileType::air) mid
        VERBATIM unsigned char random = 0;
    )

    using array_t = Array2D<Tile>;

  private:
    array_t tiles;

    static const Graphics::TextureAtlas::Region &AtlasRegion()
    {
        static Graphics::TextureAtlas::Region ret = texture_atlas().Get("tiles.png");
        return ret;
    }

  public:
    Map() {}

    Map(std::string file_name)
    {
        try
        {
            Json json(Stream::ReadOnlyData(file_name).string(), 32);

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

            for (auto pos : index_vec2(0) <= vector_range < tiles.size())
                tiles.unsafe_at(pos).random = rng.integer();
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

    template <int LayerIndex>
    void Render(Meta::value_tag<LayerIndex>, ivec2 camera_pos, ivec2 viewport_size = screen_size) const
    {
        ivec2 corner_a = div_ex(camera_pos - viewport_size/2, tile_size);
        ivec2 corner_b = div_ex(camera_pos + viewport_size/2, tile_size);

        auto DrawTile = [&](ivec2 screen_pixel_pos, ivec2 tex_pos)
        {
            r.iquad(screen_pixel_pos, AtlasRegion().region(tex_pos * tile_size, ivec2(tile_size)));
        };

        for (ivec2 pos : corner_a <= vector_range <= corner_b)
        {
            ivec2 pixel_pos = pos * tile_size - camera_pos;

            auto tile_stack = tiles.try_get(pos);
            TileType tile = Refl::Class::Member<LayerIndex>(tile_stack);
            unsigned char random = tile_stack.random;

            auto SameAs = [&](ivec2 offset)
            {
                return tile == Refl::Class::Member<LayerIndex>(tiles.try_get(pos + offset));
            };

            switch (tile)
            {
              case TileType::air:
              case TileType::_count:
                // Nothing.
                break;
              case TileType::stone:
                {
                    bool inner = true;
                    for (ivec2 offset : {ivec2(1,0), ivec2(1,1), ivec2(0,1), ivec2(-1,1), ivec2(-1,0), ivec2(-1,-1), ivec2(0,-1), ivec2(1,-1)})
                    {
                        if (!SameAs(offset))
                        {
                            inner = false;
                            break;
                        }
                    }

                    int r;
                    if (inner)
                        r = std::array{0,1,2,3,3}[random % 5];
                    else
                        r = std::array{0,0,0,1,1,2}[random % 6];

                    DrawTile(pixel_pos, ivec2(0, 1 + r));
                }
                break;
            }
        }
    }
};
