#include "world.h"

#include "game/components/game/map.h"

#include "strings/format.h"

namespace Components
{
    struct World::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) sky_background, player, particle
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;

        Game::Map map;

        struct Player
        {
            static constexpr float max_vel_x = 3, max_vel_y_down = 4, max_vel_y_up = 2.5, walk_acc = 0.13, walk_dec = 0.2, walk_acc_air = 0.1, walk_dec_air = 0.1, jump_acc = 0.2;
            static constexpr int ticks_per_walk_anim_frame = 3, jump_max_len = 20, camera_offset_y = 24;

            ivec2 pos{}, prev_pos{};
            fvec2 vel{}, prev_vel{};
            fvec2 vel_lag{};
            bool on_ground = false, prev_on_ground = false;
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

        fvec2 camera_pos_float{};
        fvec2 camera_vel{};
        ivec2 camera_pos{}; // This is computed based on `camera_pos_float`.

        struct Particle
        {
            fvec2 pos{};
            fvec2 vel{};
            fmat2 vel_m{};
            std::optional<fmat2> vel_m_ground{};
            fvec3 color0 = fvec3(1), color1 = fvec3(1);
            float alpha0 = 1, alpha1 = 1;
            float beta0 = 1, beta1 = 1;
            float size0 = 5, size1 = 5;
            int cur_age = 0;
            int life = 10;
        };
        std::deque<Particle> particles;

        void PartileEffect_Rocket(int count, fvec2 start_pos, fvec2 start_area_half_size, fvec2 base_vel, fvec2 vel_max_abs_delta)
        {
            while (count-- > 0)
            {
                fvec2 pos, vel;
                for (int m = 0; m < 2; m++)
                {
                    pos[m] = start_pos[m] + (-start_area_half_size[m] <= rng.real() <= start_area_half_size[m]);
                    vel[m] = base_vel[m] + (-vel_max_abs_delta[m] <= rng.real() <= vel_max_abs_delta[m]);
                }

                static const fmat2 matrix = fmat2::scale(fvec2(0.95)), matrix_gr = fmat2::scale(fvec2(0.6)) * fmat2::rotate(0.1);

                particles.push_back(adjust(State::Particle{}, pos = pos, vel = vel, vel_m = matrix, vel_m_ground = (rng.boolean() ? matrix_gr : matrix_gr.transpose()),
                                           life = 5 <= rng.integer() <= 20, color0 = fvec3(1,0.4 <= rng.real() <= 1,0), color1 = fvec3(1), alpha0 = 1, alpha1 = 0, beta0 = 0.9, beta1 = 1,
                                           size0 = 1 <= rng.real() <= 2, size1 = 4 <= rng.real() <= 9));
            }
        }

        void ParticleEffect_Jump(fvec2 base_pos)
        {
            base_pos.y += 9;

            for (int i = 0; i < 20; i++)
            {
                fvec2 pos = base_pos;
                pos.x += (3 <= rng.real() <= 6) * rng.sign();

                fvec2 vel;
                vel.x = -1 <= rng.real() <= 1;
                vel.y = -0.3 <= rng.real() <= 0;

                static const fmat2 matrix = fmat2::scale(fvec2(0.98));
                particles.push_back(adjust(State::Particle{}, pos = pos, vel = vel, vel_m = matrix, life = 12 <= rng.integer() <= 35, color0 = fvec3(0.55 <= rng.real() <= 0.9), color1 = _.color0,
                                           alpha0 = 1, alpha1 = 0, size0 = 2 <= rng.real() <= 3, size1 = 4 <= rng.real() <= 7));
            }
        }
    };

    World::World(std::string level_name) : state(std::make_unique<State>())
    {
        State &s = *state;
        s.map = Game::Map("assets/maps/{}.json"_format(level_name));
        s.p.pos = s.p.prev_pos = s.map.Points().GetSinglePoint("player") with(y -= 4);
        s.p.on_ground = s.p.prev_on_ground = s.p.SolidAtOffset(s.map, ivec2(0,1));
        clamp_var(s.camera_pos = s.camera_pos_float = s.p.pos with(y -= s.p.camera_offset_y), screen_size/2, s.map.Tiles().size() * s.map.tile_size - screen_size/2);
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

            if (!s.p.on_ground && hc != 0 && s.p.jump_ticks_left > 0)
            {
                s.PartileEffect_Rocket(3, s.p.pos with(x += 5 * -hc, y += 2), fvec2(0.2, 0.5), s.p.vel with(x += 2.3 * -hc), fvec2(1.5,0.5));
            }


            if (hc == 0)
            {
                s.p.walk_anim_timer = 0;
            }

            if (hc)
            {
                s.p.vel.x += hc * (s.p.on_ground ? s.p.walk_acc : s.p.walk_acc_air);
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
            else if (abs(s.p.vel.x) > (s.p.on_ground ? s.p.walk_dec : s.p.walk_dec_air))
            {
                s.p.vel.x -= sign(s.p.vel.x) * (s.p.on_ground ? s.p.walk_dec : s.p.walk_dec_air);
            }
            else
            {
                s.p.vel.x = 0;
            }
        }

        { // Jump controls and gravity
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
                s.PartileEffect_Rocket(3, s.p.pos with(y += 4), fvec2(1.5, 0.5), s.p.vel with(y += 2.55), fvec2(0.35,1));
            }
            else
            {
                static constexpr float gravity = 0.1;
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
            ivec2 vel_int = iround(nexttoward(s.p.vel + s.p.vel_lag, 0));
            s.p.vel_lag += s.p.vel - vel_int;

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
            s.p.on_ground = s.p.SolidAtOffset(s.map, ivec2(0,1));

            // Clamp velocity if touching wall
            for (int m = 0; m < 2; m++)
            for (int sg = -1; sg <= 1; sg += 2)
            {
                ivec2 delta{};
                delta[m] = sg;
                if (s.p.SolidAtOffset(s.map, delta))
                    s.p.ClampVel(m, sg, 0);
            }

            // Jump and landing particles
            if (s.p.on_ground && !s.p.prev_on_ground && s.map.PixelIsSolid(s.p.pos with(y += 14)) && s.p.prev_vel.y > 1.3)
                s.ParticleEffect_Jump(s.p.pos);
        }

        { // Move camera
            fvec2 target = s.p.pos with(y -= s.p.camera_offset_y);

            fvec2 delta = target - s.camera_pos_float;
            float dist = delta.len();
            if (dist > 0.001)
            {
                fvec2 dir = delta / dist;
                s.camera_vel += dir * pow(dist / 100, 1.5) * 0.5;
            }
            s.camera_vel *= 1 - 0.085;

            s.camera_pos_float += s.camera_vel;

            { // Clamp camera pos
                fvec2 min_pos = screen_size/2;
                fvec2 max_pos = s.map.Tiles().size() * s.map.tile_size - screen_size/2;

                for (int m = 0; m < 2; m++)
                {
                    if (s.camera_pos_float[m] < min_pos[m])
                    {
                        s.camera_pos_float[m] = min_pos[m];
                        clamp_var_min(s.camera_vel[m], 0);
                    }
                    if (s.camera_pos_float[m] > max_pos[m])
                    {
                        s.camera_pos_float[m] = max_pos[m];
                        clamp_var_max(s.camera_vel[m], 0);
                    }
                }
            }

            // Compute final camera pos
            s.camera_pos = iround(s.camera_pos_float);
        }

        { // Update particles
            for (State::Particle &par : s.particles)
            {
                par.pos += par.vel;

                if (par.vel_m_ground)
                    par.vel = (s.map.PixelIsSolid(par.pos) ? *par.vel_m_ground : par.vel_m) * par.vel;
                else
                    par.vel = par.vel_m * par.vel;

                par.cur_age++;
            }
            std::erase_if(s.particles, [](const State::Particle &par){return par.cur_age >= par.life;});
        }

        { // Update `prev_*` variables
            s.p.prev_pos = s.p.pos;
            s.p.prev_vel = s.p.vel;
            s.p.prev_on_ground = s.p.on_ground;
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

        // Particles
        for (const State::Particle &par : s.particles)
        {
            float t = par.cur_age / float(par.life);
            fvec3 color = mix(t, par.color0, par.color1);
            float alpha = mix(t, par.alpha0, par.alpha1);
            float beta = mix(t, par.beta0, par.beta1);
            float size = mix(t, par.size0, par.size1);
            r.fquad(par.pos - s.camera_pos, fvec2(size)).center().tex(s.atlas.particle.pos, s.atlas.particle.size).color(color).mix(0).alpha(alpha).beta(beta);
        }
    }
}
