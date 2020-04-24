#include "world.h"

#include "game/components/circuit.h"
#include "game/components/game/map.h"

#include "strings/format.h"

namespace Components
{
    struct World::State
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) sky_background, player, particle, nodes
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;

        static constexpr bool allow_debug_controls =
        #ifndef NDEBUG
            true;
        #else
            false;
        #endif

        Game::Map map;

        struct Player
        {
            static constexpr float max_vel_x = 3, max_vel_y_down = 4, max_vel_y_up = 2.5, walk_acc = 0.13, walk_dec = 0.2, walk_acc_air = 0.1, walk_dec_air = 0.1, jump_acc = 0.2, gravity = 0.1;
            static constexpr int ticks_per_walk_anim_frame = 3, jump_max_len = 20, camera_offset_y = 24, ticks_before_explosion_on_death = 20;

            ivec2 pos{}, prev_pos{};
            fvec2 vel{}, prev_vel{};
            fvec2 vel_lag{};
            bool on_ground = false, prev_on_ground = false;
            int jump_ticks_left = 0;

            bool facing_left = false;
            int anim_frame = 0;
            int walk_anim_timer = 0;

            int death_timer = 0;

            bool IsDead() const
            {
                return death_timer > 0;
            }
            void Kill()
            {
                if (death_timer == 0)
                    death_timer = 1;
            }

            static constexpr int hitbox_x_min = -6, hitbox_x_max = 5;
            static constexpr ivec2 hitbox_offsets[] =
            {
                ivec2(hitbox_x_min,-4), ivec2(hitbox_x_max,-4),
                ivec2(hitbox_x_min, 0), ivec2(hitbox_x_max, 5),
                ivec2(hitbox_x_min, 9), ivec2(hitbox_x_max, 9),
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
        ivec2 camera_shake{}; // Set to small positive values to shake the camera.

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

        struct ScrapParticle
        {
            static constexpr float gravity = Player::gravity;

            fvec2 pos{};
            fvec2 vel{};
            Graphics::TextureAtlas::Region tex;
            int cur_age = 0;
            int life = 10;

            bool fire_trail = false;
            bool collision = false;

            bool sprite_flip_x = rng.boolean();
        };
        std::deque<ScrapParticle> scrap_particles;

        struct CircuitIO
        {
            class Input
            {
                bool reset_at_next_assignment = true;
                bool value = false;

              public:
                Input() {}

                void ResetAtNextAssignment()
                {
                    reset_at_next_assignment = true;
                }
                void Assign(bool new_value)
                {
                    if (reset_at_next_assignment)
                    {
                        reset_at_next_assignment = false;
                        value = false;
                    }
                    if (new_value)
                        value = true;
                }

                bool Get() const
                {
                    return value;
                }
                explicit operator bool() const
                {
                    return value;
                }
            };

            UNNAMED_MEMBERS(
                DECL(bool INIT=false) out_at_least_one_tick_executed
                DECL(bool[4] INIT{}) out_solid_dir

                DECL(Input) in_control_left, in_control_right, in_control_jump
            )
        };
        CircuitIO circuit_io;

        void ParticleEffect_Rocket(int count, fvec2 start_pos, fvec2 start_area_half_size, fvec2 base_vel, fvec2 vel_max_abs_delta)
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

        void ParticleEffect_FireTrail(int count, fvec2 start_pos, fvec2 start_area_half_size, fvec2 base_vel, fvec2 vel_max_abs_delta)
        {
            while (count-- > 0)
            {
                fvec2 pos, vel;
                for (int m = 0; m < 2; m++)
                {
                    pos[m] = start_pos[m] + (-start_area_half_size[m] <= rng.real() <= start_area_half_size[m]);
                    vel[m] = base_vel[m] + (-vel_max_abs_delta[m] <= rng.real() <= vel_max_abs_delta[m]);
                }

                static const fmat2 matrix = fmat2::scale(fvec2(0.815));

                particles.push_back(adjust(State::Particle{}, pos = pos, vel = vel, vel_m = matrix, life = 7 <= rng.integer() <= 30,
                                           color0 = fvec3(1,0.4 <= rng.real() <= 1,0), color1 = fvec3(1), alpha0 = 1, alpha1 = 0, beta0 = 0.9, beta1 = 1,
                                           size0 = 2 <= rng.real() <= 3, size1 = 5 <= rng.real() <= 11));
            }
        }

        void ParticleEffect_Fire(int count, fvec2 start_pos, fvec2 start_area_half_size, fvec2 base_vel, fvec2 vel_max_abs_delta)
        {
            while (count-- > 0)
            {
                fvec2 pos, vel;
                for (int m = 0; m < 2; m++)
                {
                    pos[m] = start_pos[m] + (-start_area_half_size[m] <= rng.real() <= start_area_half_size[m]);
                    vel[m] = base_vel[m] + (-vel_max_abs_delta[m] <= rng.real() <= vel_max_abs_delta[m]);
                }

                static const fmat2 matrix = fmat2::scale(fvec2(0.93));

                particles.push_back(adjust(State::Particle{}, pos = pos, vel = vel, vel_m = matrix, life = 15 <= rng.integer() <= 60,
                                           color0 = fvec3(1,0.4 <= rng.real() <= 1,0), color1 = fvec3(1), alpha0 = 1, alpha1 = 0, beta0 = 0.9, beta1 = 1,
                                           size0 = 2 <= rng.real() <= 3, size1 = 8 <= rng.real() <= 20));
            }
        }

        void ParticleEffect_PlayerScrapExplosion(fvec2 base_pos, fvec2 base_vel)
        {
            int iterations = 4;
            while (iterations-- > 0)
            {
                constexpr int piece_count = 5;
                int indices[piece_count];
                for (int i = 0; i < piece_count; i++)
                    indices[i] = i;
                std::shuffle(std::begin(indices), std::end(indices), rng.generator());

                float base_angle = rng.angle();
                const float angle_step = f_pi * 2 / piece_count, angle_max_abs_change = angle_step * 0.9 / 2;

                for (int i = 0; i < 5; i++)
                {
                    float angle = base_angle + angle_step * i + (-angle_max_abs_change <= rng.real() <= angle_max_abs_change);
                    fvec2 dir = fvec2::dir(angle);

                    fvec2 pos = base_pos + dir * float(3 <= rng.real() <= 6);
                    fvec2 vel = base_vel + dir * float(0.15 <= rng.real() <= 4.65);

                    scrap_particles.push_back(adjust(ScrapParticle{}, pos = pos, vel = vel, tex = atlas.player.region(ivec2(12 * i, 24), ivec2(12)),
                                                     life = 160 <= rng.integer() <= 300, fire_trail = true, collision = int(0 <= rng.integer() < 3) != 0));
                }
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

    void World::CopyPersistentStateFrom(const World &other)
    {
        State &s = *state;
        const State &other_s = *other.state;

        s.camera_pos_float = other_s.camera_pos_float;
        s.camera_vel       = other_s.camera_vel      ;
        s.camera_pos       = other_s.camera_pos      ;
        s.camera_shake     = other_s.camera_shake    ;
    }

    MAYBE_CONST(
        CV World::State &World::GetState() CV
        {
            return *state;
        }
    )

    void World::Tick()
    {
        State &s = *state;

        { // Walk controls
            if (s.allow_debug_controls)
            {
                s.circuit_io.in_control_left.Assign(Input::Button(Input::left).down());
                s.circuit_io.in_control_right.Assign(Input::Button(Input::right).down());
            }

            int hc = s.p.IsDead() ? 0 : s.circuit_io.in_control_right.Get() - s.circuit_io.in_control_left.Get();

            if (!s.p.on_ground && hc != 0 && s.p.jump_ticks_left > 0)
            {
                s.ParticleEffect_Rocket(3, s.p.pos with(x += 5 * -hc, y += 2), fvec2(0.2, 0.5), s.p.vel with(x += 2.3 * -hc), fvec2(1.5,0.5));
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
            if (s.allow_debug_controls)
            {
                s.circuit_io.in_control_jump.Assign(Input::Button(Input::up).down());
            }

            if (s.p.on_ground)
                s.p.jump_ticks_left = s.p.jump_max_len;

            if (!s.circuit_io.in_control_jump.Get() || s.p.IsDead())
                s.p.jump_ticks_left = 0;
            else if (s.p.jump_ticks_left > 0)
                s.p.jump_ticks_left--;

            if (s.p.jump_ticks_left > 0)
            {
                s.p.vel.y -= s.p.jump_acc;
                s.ParticleEffect_Rocket(3, s.p.pos with(y += 4), fvec2(1.5, 0.5), s.p.vel with(y += 2.55), fvec2(0.35,1));
            }
            else
            {
                s.p.vel.y += s.p.gravity;
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

        { // Spike collisions
            if (!s.p.IsDead() && (s.map.PixelIsSpike(s.p.pos + s.p.hitbox_x_min) || s.map.PixelIsSpike(s.p.pos + s.p.hitbox_x_max)))
                s.p.Kill();
        }

        { // Death timer and effects
            if (s.p.death_timer == 3)
                s.ParticleEffect_Fire(30, s.p.pos, fvec2(8,6), s.p.vel with(y -= 0.6), fvec2(1.4,1.2));
            if (s.p.death_timer > 0 && s.p.death_timer < s.p.ticks_before_explosion_on_death)
                s.ParticleEffect_Fire(3, s.p.pos, fvec2(8,6), s.p.vel with(y -= 1.2), fvec2(0.6,1.1));
            if (s.p.death_timer == s.p.ticks_before_explosion_on_death)
            {
                s.ParticleEffect_PlayerScrapExplosion(s.p.pos, (s.p.vel * 0.7) with(y -= 1.5));
                s.camera_shake = ivec2(1);
            }

            if (s.p.death_timer > 0)
                s.p.death_timer++;
        }

        { // Update particles
            { // Regular
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

            { // Scrap
                for (State::ScrapParticle &par : s.scrap_particles)
                {
                    if (!par.collision)
                    {
                        par.pos += par.vel;
                    }
                    else
                    {
                        ivec2 int_vel(par.vel);
                        fvec2 frac_vel = par.vel - int_vel;

                        while (int_vel != 0)
                        {
                            for (int m = 0; m < 2; m++)
                            {
                                int sg = sign(int_vel[m]);
                                ivec2 offset{};
                                offset[m] = sg;
                                if (s.map.PixelIsSolid(iround(par.pos) + offset))
                                {
                                    par.vel[m] *= -0.2;
                                    par.vel[!m] *= 0.5;

                                    if (abs(par.vel[m]) < 0.3)
                                        par.vel[m] = 0;

                                    int_vel = ivec2(0);
                                    frac_vel = fvec2(0);
                                }
                                else
                                {
                                    par.pos[m] += sg;
                                    int_vel[m] -= sg;
                                }
                            }
                        }

                        if (!s.map.PixelIsSolid(iround(par.pos) + sign(frac_vel)))
                            par.pos += frac_vel;
                    }

                    par.vel.y += par.gravity;
                    par.cur_age++;

                    if (par.fire_trail)
                    {
                        float p = pow(1 - par.cur_age / float(par.life), 4);
                        if (float(0 <= rng.real() <= 1) < p)
                            s.ParticleEffect_FireTrail(1, par.pos, fvec2(1.5), par.vel with(y -= 1), fvec2(0.2));
                    }
                }
                std::erase_if(s.scrap_particles, [](const State::ScrapParticle &par){return par.cur_age >= par.life;});
            }
        }

        { // Update circuit nodes
            { // Output
                s.circuit_io.out_at_least_one_tick_executed = true;

                for (int i = 0; i < 4; i++)
                {
                    s.circuit_io.out_solid_dir[i] = s.p.SolidAtOffset(s.map, ivec2::dir4(i));
                }
            }

            { // Input
                Meta::cexpr_for<Refl::Class::member_count<State::CircuitIO>>([&](auto index)
                {
                    constexpr auto i = index.value;
                    if constexpr (std::is_same_v<State::CircuitIO::Input, Refl::Class::member_type<State::CircuitIO, i>>)
                    {
                        Refl::Class::Member<i>(s.circuit_io).ResetAtNextAssignment();
                    }
                });
            }
        }

        { // Update `prev_*` variables
            s.p.prev_pos = s.p.pos;
            s.p.prev_vel = s.p.vel;
            s.p.prev_on_ground = s.p.on_ground;
        }
    }
    void World::PersistentTick()
    {
        State &s = *state;

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

            ivec2 shake{};
            if ((s.camera_shake > 0).any())
            {
                for (int m = 0; m < 2; m++)
                {
                    if (s.camera_shake[m] == 0)
                        continue;

                    shake[m] = (1 <= rng.integer() <= s.camera_shake[m]) * rng.sign();
                }
                s.camera_shake -= sign(s.camera_shake);
            }

            // Compute final camera pos
            s.camera_pos = iround(s.camera_pos_float) + shake;
        }
    }

    void World::Render() const
    {
        const State &s = *state;

        // Sky background
        r.iquad(ivec2(0), s.atlas.sky_background).center();

        // Map
        state->map.Render(Meta::value_tag<0>{}, s.camera_pos);

        { // Player
            static constexpr ivec2 player_sprite_size(24,24);
            float alpha = 1 - clamp((s.p.death_timer - s.p.ticks_before_explosion_on_death) / float(20));

            r.iquad(s.p.pos - s.camera_pos, s.atlas.player.region(ivec2(player_sprite_size.x * s.p.anim_frame, 0), player_sprite_size)).flip_x(s.p.facing_left).center().alpha(alpha);
        }

        { // Particles
            // Regular
            for (const State::Particle &par : s.particles)
            {
                float t = par.cur_age / float(par.life);
                fvec3 color = mix(t, par.color0, par.color1);
                float alpha = mix(t, par.alpha0, par.alpha1);
                float beta = mix(t, par.beta0, par.beta1);
                float size = mix(t, par.size0, par.size1);
                r.fquad(par.pos - s.camera_pos, fvec2(size)).center().tex(s.atlas.particle.pos, s.atlas.particle.size).color(color).mix(0).alpha(alpha).beta(beta);
            }

            // Scrap
            for (const State::ScrapParticle &par : s.scrap_particles)
            {
                float t = par.cur_age / float(par.life);
                float alpha = 1 - pow(t, 5);
                r.fquad(par.pos - s.camera_pos, par.tex).center().alpha(alpha).flip_x(par.sprite_flip_x);
            }
        }
    }


    namespace CustomNodes
    {
        // Input nodes
        STRUCT( SimStarted EXTENDS BasicCustomInputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "Simulation has started";
            }
            int GetPositionInNodeList() const override
            {
                return 1;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Simulation started"));
                return ret;
            }
            bool Custom_ReadValue(const World &world) const override
            {
                return world.GetState().circuit_io.out_at_least_one_tick_executed;
            }
        };

        STRUCT( Solid_Right EXTENDS BasicCustomInputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "Touching wall on the right";
            }
            int GetPositionInNodeList() const override
            {
                return 3;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Wall >"));
                return ret;
            }
            bool Custom_ReadValue(const World &world) const override
            {
                return world.GetState().circuit_io.out_solid_dir[0];
            }
        };
        STRUCT( Solid_Down EXTENDS BasicCustomInputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "Have ground below";
            }
            int GetPositionInNodeList() const override
            {
                return 4;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "On ground"));
                return ret;
            }
            bool Custom_ReadValue(const World &world) const override
            {
                return world.GetState().circuit_io.out_solid_dir[1];
            }
        };
        STRUCT( Solid_Left EXTENDS BasicCustomInputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "Touching wall on the left";
            }
            int GetPositionInNodeList() const override
            {
                return 2;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Wall <"));
                return ret;
            }
            bool Custom_ReadValue(const World &world) const override
            {
                return world.GetState().circuit_io.out_solid_dir[2];
            }
        };
        STRUCT( Solid_Up EXTENDS BasicCustomInputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "Touching ceiling";
            }
            int GetPositionInNodeList() const override
            {
                return 5;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Touching ceiling"));
                return ret;
            }
            bool Custom_ReadValue(const World &world) const override
            {
                return world.GetState().circuit_io.out_solid_dir[3];
            }
        };

        // Custom input nodes
        enum class GridMode {solid, spike};
        STRUCT( GenericTileGrid EXTENDS BasicNode
            PARAM(GridMode) Mode
            PARAM(int) SizeX
            PARAM(int) SizeY )
        {
            static constexpr ivec2 size = ivec2(SizeX, SizeY);
            static_assert(size % 2 == 1, "The size must be odd.");

            static constexpr int cell_size = 8;

            // Given `0 <= index < size.prod()`, returns `-size/2 <= offset <= size/2`.
            static ivec2 PointIndexToOffset(int index)
            {
                return ivec2(index % size.x, index / size.x) - size/2;
            }

            using point_info_array_t = std::array<PointInfo, size.prod()>;
            static const point_info_array_t &GetPointInfoArray()
            {
                static point_info_array_t ret = []{
                    point_info_array_t ret;
                    for (size_t i = 0; i < size.prod(); i++)
                    {
                        ivec2 pos = PointIndexToOffset(i);
                        ret[i] = adjust(PointInfo::Default(), visual_radius = 0, extra_out_visual_radius = 0, half_extent = ivec2(8), offset_to_node = pos * cell_size);
                    }
                    return ret;
                }();
                return ret;
            }

            using point_array_t = std::array<OutPoint, size.prod()>;
            static point_array_t MakePointArray()
            {
                const point_info_array_t &info_arr = GetPointInfoArray();

                point_array_t ret;
                for (int i = 0; i < size.prod(); i++)
                    ret[i] = &info_arr[i];

                return ret;
            }

            MEMBERS(
                DECL(point_array_t INIT=MakePointArray()) out_list
            )

            void Tick(World &world, const Circuit &) override
            {
                const auto &s = world.GetState();

                if (!s.circuit_io.out_at_least_one_tick_executed)
                {
                    for (OutPoint &point : out_list)
                        point.is_powered = false;
                    return;
                }

                ivec2 base_tile_pos = div_ex(s.p.pos, s.map.tile_size);

                for (size_t i = 0; i < size.prod(); i++)
                {
                    ivec2 tile_pos = base_tile_pos + PointIndexToOffset(i);

                    switch (Mode)
                    {
                      case GridMode::solid:
                        out_list[i].is_powered = s.map.TileIsSolid(tile_pos);
                        break;
                      case GridMode::spike:
                        out_list[i].is_powered = s.map.TileIsSpike(tile_pos);
                        break;
                    }
                }
            }

            void Render(ivec2 offset) const override
            {
                int shape = 0;
                switch (Mode)
                {
                  case GridMode::solid:
                    shape = 0;
                    break;
                  case GridMode::spike:
                    shape = 1;
                    break;
                }

                for (int powered = 0; powered < 2; powered++)
                {
                    for (size_t i = 0; i < size.prod(); i++)
                    {
                        if (out_list[i].is_powered != powered)
                            continue;

                        ivec2 point_offset = PointIndexToOffset(i);
                        r.iquad(pos + offset + point_offset * cell_size, World::State::atlas.nodes.region(ivec2(31+9*shape, 9*powered), ivec2(9))).center(ivec2(4));
                    }
                }
            }

            ivec2 GetVisualHalfExtent() const override
            {
                return size * cell_size / 2 + 1;
            }

            int InPointCount() const override {return 0;}
            int OutPointCount() const override {return out_list.size();}
            InPoint &GetInPointLow(int) override {std::terminate();}
            OutPoint &GetOutPointLow(int index) override {return out_list[index];}
        };
        STRUCT( Grid_Solid7x7 EXTENDS GenericTileGrid<GridMode::solid,7,7> )
        {
            MEMBERS()

            std::string GetName() const override {return "Wall detector";}
            int GetPositionInNodeList() const override {return 10;}
        };
        STRUCT( Grid_Spike7x7 EXTENDS GenericTileGrid<GridMode::spike,7,7> )
        {
            MEMBERS()

            std::string GetName() const override {return "Spike detector";}
            int GetPositionInNodeList() const override {return 11;}
        };

        // Output nodes
        STRUCT( Control_Left EXTENDS BasicCustomOutputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "* Move to the left";
            }
            int GetPositionInNodeList() const override
            {
                return 20;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Move left"));
                return ret;
            }
            void Custom_WriteValue(World &world, bool value) const override
            {
                world.GetState().circuit_io.in_control_left.Assign(value);
            }
        };
        STRUCT( Control_Right EXTENDS BasicCustomOutputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "* Move to the right";
            }
            int GetPositionInNodeList() const override
            {
                return 21;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Move right"));
                return ret;
            }
            void Custom_WriteValue(World &world, bool value) const override
            {
                world.GetState().circuit_io.in_control_right.Assign(value);
            }
        };
        STRUCT( Control_Jump EXTENDS BasicCustomOutputNode )
        {
            MEMBERS()

            std::string GetName() const override
            {
                return "* Jump";
            }
            int GetPositionInNodeList() const override
            {
                return 22;
            }
            const CustomNodeInfo &Custom_GetInfo() const override
            {
                static CustomNodeInfo ret(Graphics::Text(font_main(), "Jump"));
                return ret;
            }
            void Custom_WriteValue(World &world, bool value) const override
            {
                world.GetState().circuit_io.in_control_jump.Assign(value);
            }
        };
    }
}
