#include <optional>

#include "game/components/circuit.h"
#include "game/components/editor.h"
#include "game/components/menu_controller.h"
#include "game/components/tooltip_controller.h"
#include "game/components/world.h"
#include "game/main.h"
#include "reflection/full_with_poly.h"

namespace States
{
    STRUCT( Game EXTENDS State::BasicState )
    {
        UNNAMED_MEMBERS()

        std::optional<Components::World> world, world_copy;

        Components::Circuit circuit;
        Components::Editor editor;
        Components::TooltipController tooltip_controller;
        Components::MenuController menu_controller;

        Game()
        {
            world = world_copy = Components::World("1");
        }

        void Tick(const State::NextState &next_state) override
        {
            (void)next_state;

            // ImGui::ShowDemoWindow();

            editor.Tick(world, world_copy, circuit, menu_controller, tooltip_controller);
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

            if (world)
                world->Render();

            editor.Render(circuit);
            menu_controller.Render();
            tooltip_controller.Render();
            editor.RenderCursor();

            r.Finish();
        }
    };
}
