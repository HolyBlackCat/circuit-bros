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

        void LoadMap(std::string name)
        {
            *this = Game();
            world = world_copy = Components::World(name);
        }

        void Tick(const State::NextState &next_state) override
        {
            (void)next_state;

            // ImGui::ShowDemoWindow();

            { // Debug "save/load circuit" window
                ImGui::SetNextWindowPos(ivec2(0), ImGuiCond_Appearing);

                ImGui::Begin("Save/load circuit", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                FINALLY( ImGui::End(); )

                for (int i = 1; i <= 5; i++)
                {
                    if (ImGui::Button("Save #{}"_format(i).c_str()))
                    {
                        try
                        {
                            Stream::Output out("saved_circuit_{}.refl"_format(i));
                            Refl::ToString(circuit, out, Refl::ToStringOptions::Pretty());
                        }
                        catch (std::exception &e)
                        {
                            Interface::MessageBox("Error", "Unable to save:\n{}"_format(e.what()));
                        }
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Load #{}"_format(i).c_str()))
                    {
                        try
                        {
                            Stream::Input in("saved_circuit_{}.refl"_format(i));
                            LoadMap("1");
                            Refl::FromString(circuit, in);
                        }
                        catch (std::exception &e)
                        {
                            Interface::MessageBox("Error", "Unable to load:\n{}"_format(e.what()));
                        }
                    }
                }
            }

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
