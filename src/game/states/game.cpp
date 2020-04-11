#include "game/components/circuit.h"
#include "game/components/editor.h"
#include "game/components/menu_controller.h"
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

        Components::Circuit circuit;
        Components::Editor editor;
        Components::TooltipController tooltip_controller;
        Components::MenuController menu_controller;

        Game() : map("assets/maps/1.json") {}

        void Tick(const State::NextState &next_state) override
        {
            (void)next_state;

            // ImGui::ShowDemoWindow();

            editor.Tick(circuit, menu_controller, tooltip_controller);
            if (Input::Button(Input::tab).pressed())
                editor.SetOpen(!editor.IsOpen());

            menu_controller.Tick(&tooltip_controller);
            tooltip_controller.Tick();
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(0), atlas.sky_background).center();

            map.Render(Meta::value_tag<0>{}, ivec2(200,50));
            editor.Render(circuit);
            menu_controller.Render();
            tooltip_controller.Render();
            editor.RenderCursor();

            r.Finish();
        }
    };
}
