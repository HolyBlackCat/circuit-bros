#include "world.h"

#include "game/components/game/map.h"

#include "strings/format.h"

namespace Components
{
    struct World::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) sky_background, player
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;

        Game::Map map;

        struct Player
        {
            static constexpr float max_vel_x = 2, max_vel_y_down = 2, max_vel_y_up = 1.35, walk_acc = 0.08, walk_dec = 0.1, jump_acc = 0.2;
            static constexpr int ticks_per_walk_anim_frame = 4, jump_max_len = 20;

            ivec2 pos{};
            fvec2 vel{};
            fvec2 vel_lag{};
            bool on_ground = false;
            int jump_ticks_left = 0;

            bool facing_left = false;
            int anim_frame = 0;
            int walk_anim_timer = 0;

            static constexpr ivec2 hitbox_offsets[] =
            {
                ivec2(-6,-4), ivec2( 5,-4),
                ivec2(-6, 0), ivec2( 5, 5),
                ivec2(-6, 9), ivec2( 5, 9),
            };

            bool SolidAtOffset(const Game::Map &map, ivec2 offset) const
            {
                for (ivec2 point : hitbox_offsets)
                    if (map.PixelIsSolid(pos + offset + point))
                        return true;
                return false;
            }
            void ClampVel(bool m/*1 to clamp y*/, int vel_sign, float max_abs_value)
            {
                DebugAssertNameless(vel_sign != 0);

                if (vel[m] * vel_sign >=/*sic*/ max_abs_value)
                {
                    vel[m] = max_abs_value * vel_sign;

                    if (sign(vel_lag[m]) == vel_sign)
                        vel_lag[m] = 0;
                }
            }
        };
        Player p;

        ivec2 camera_pos{};

        State()
        {
            p.pos = ivec2(100,-50);
        }
    };

    World::World(std::string level_name) : state(std::make_unique<State>())
    {
        state->map = Game::Map("assets/maps/{}.json"_format(level_name));
    }
    World::World(const World &other) : state(std::make_unique<State>(*other.state)) {}
    World::World(World &&other) = default;
    World &World::operator=(const World &other) {*state = *other.state; return *this;}
    World &World::operator=(World &&other) = default;
    World::~World() = default;

    void World::Tick()
    {
        State &s = *state;

        { // Walk controls
            int hc = Input::Button(Input::right).down() - Input::Button(Input::left).down();

            if (hc == 0)
            {
                s.p.walk_anim_timer = 0;
            }

            if (hc)
            {
                s.p.vel.x += hc * s.p.walk_acc;
                s.p.facing_left = hc < 0;

                s.p.walk_anim_timer++;
                if (s.p.walk_anim_timer >= s.p.ticks_per_walk_anim_frame)
                {
                    s.p.walk_anim_timer = 0;
                    s.p.anim_frame++;
                    if (s.p.anim_frame >= 4)
                        s.p.anim_frame = 0;
                }
            }
            else if (abs(s.p.vel.x) > s.p.walk_dec)
            {
                s.p.vel.x -= sign(s.p.vel.x) * s.p.walk_dec;
            }
            else
            {
                s.p.vel.x = 0;
                s.p.vel_lag *= 0.9;
            }
        }

        { // Jump controls and gravity
            s.p.on_ground = s.p.SolidAtOffset(s.map, ivec2(0,1));
            if (s.p.on_ground)
                s.p.jump_ticks_left = s.p.jump_max_len;

            Input::Button jump_button = Input::up;
            if (jump_button.up())
                s.p.jump_ticks_left = 0;
            else if (s.p.jump_ticks_left > 0)
                s.p.jump_ticks_left--;

            if (s.p.jump_ticks_left > 0)
            {
                s.p.vel.y -= s.p.jump_acc;
            }
            else
            {
                static constexpr float gravity = 0.07;
                s.p.vel.y += gravity;
            }
        }

        { // Clamp velocity
            s.p.ClampVel(0, -1, s.p.max_vel_x);
            s.p.ClampVel(0, 1, s.p.max_vel_x);
            s.p.ClampVel(1, -1, s.p.max_vel_y_up);
            s.p.ClampVel(1, 1, s.p.max_vel_y_down);
        }

        { // Move player
            ivec2 vel_int = iround(s.p.vel + s.p.vel_lag);
            s.p.vel_lag += s.p.vel - vel_int;

            s.p.pos += vel_int;

            while (vel_int != 0)
            {
                for (int m = 0; m < 2; m++)
                {
                    if (vel_int[m] == 0)
                        continue;
                    ivec2 delta{};
                    delta[m] = sign(vel_int[m]);
                    if (s.p.SolidAtOffset(s.map, delta))
                    {
                        s.p.ClampVel(m, delta[m], 0);
                    }
                    else
                    {
                        s.p.pos += delta;
                    }

                    vel_int[m] -= sign(vel_int[m]);
                }
            }

            // Clamp velocity if touching wall
            for (int m = 0; m < 2; m++)
            for (int sg = -1; sg <= 1; sg += 2)
            {
                ivec2 delta{};
                delta[m] = sg;
                if (s.p.SolidAtOffset(s.map, delta))
                    s.p.ClampVel(m, sg, 0);
            }
        }
    }

    void World::Render() const
    {
        const State &s = *state;

        // Sky background
        r.iquad(ivec2(0), s.atlas.sky_background).center();

        // Map
        state->map.Render(Meta::value_tag<0>{}, s.camera_pos);

        // Player
        static constexpr ivec2 player_sprite_size(24,24);
        r.iquad(s.p.pos - s.camera_pos, s.atlas.player.region(ivec2(player_sprite_size.x * s.p.anim_frame, 0), player_sprite_size)).flip_x(s.p.facing_left).center();
    }
}
