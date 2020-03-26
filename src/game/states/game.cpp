#include "game/main.h"
#include "game/map.h"
#include "reflection/full_with_poly.h"

namespace States
{
    STRUCT( Game EXTENDS State::BasicState )
    {
        UNNAMED_MEMBERS()

        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) sky_background
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        inline static Atlas atlas;


        Map map;

        Game() : map("assets/maps/1.json") {}

        float angle = 0;

        void Tick(const State::NextState &next_state) override
        {
            (void)next_state;

            angle += 0.01;
            // ImGui::ShowDemoWindow();
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(0), atlas.sky_background).center();

            map.Render(Meta::value_tag<0>{}, mouse.pos());

            r.iquad(mouse.pos(), ivec2(32)).center().rotate(angle).color(mouse.left.down() ? fvec3(1,0.5,0) : fvec3(0,0.5,1));

            r.Finish();
        }
    };
}
