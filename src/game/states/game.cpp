#include "game/components/editor.h"
#include "game/components/tooltip_controller.h"
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

        Components::Editor editor;
        Components::TooltipController tooltip_controller;

        Game() : map("assets/maps/1.json") {}

        float angle = 0;

        void Tick(const State::NextState &next_state) override
        {
            (void)next_state;

            // ImGui::ShowDemoWindow();

            angle += 0.01;

            editor.Tick(tooltip_controller);
            if (Input::Button(Input::tab).pressed())
                editor.SetOpen(!editor.IsOpen());

            tooltip_controller.Tick();
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(0), atlas.sky_background).center();

            map.Render(Meta::value_tag<0>{}, ivec2(200,50));
            r.iquad(mouse.pos(), ivec2(32)).center().rotate(angle).color(mouse.left.down() ? fvec3(1,0.5,0) : fvec3(0.5));
            editor.Render();

            tooltip_controller.Render();


            r.Finish();
        }
    };
}
