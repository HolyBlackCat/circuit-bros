#include "game/main.h"

Interface::Window window("Circuit Bros", screen_size * 2, Interface::windowed, adjust_(Interface::WindowSettings{}, min_size = screen_size));
static Graphics::DummyVertexArray dummy_vao = nullptr;

static const Graphics::ShaderConfig shader_config = Graphics::ShaderConfig::Core();
static Interface::ImGuiController gui_controller(Poly::derived<Interface::ImGuiController::GraphicsBackend_Modern>, adjust_(Interface::ImGuiController::Config{}, shader_header = shader_config.common_header));

Graphics::Font &font_main()
{
    static Graphics::Font ret;
    return ret;
}

Graphics::TextureAtlas &texture_atlas()
{
    static Graphics::FontFile font_file_main("assets/Cat12.ttf", 12);
    static Graphics::TextureAtlas ret(ivec2(2048), "assets/_images", "assets/atlas.png", "assets/atlas.refl");
    Graphics::Image &image = ret.GetImage();
    auto region = ret.Get("font_storage.png");
    Unicode::CharSet ranges({Unicode::Ranges::Basic_Latin, Unicode::Ranges::Cyrillic});
    std::vector<Graphics::FontAtlasEntry> entries = {
        Graphics::FontAtlasEntry(font_main(), font_file_main, ranges, Graphics::FontFile::monochrome | Graphics::FontFile::hinting_mode_light, Graphics::FontAtlasEntry::no_line_gap),
    };
    Graphics::MakeFontAtlas(image, region.pos, region.size, entries);
    return ret;
}
Graphics::Texture texture_main = Graphics::Texture(nullptr).Wrap(Graphics::clamp).Interpolation(Graphics::nearest).SetData(texture_atlas().GetImage());

AdaptiveViewport adaptive_viewport(shader_config, screen_size);
Render r = adjust_(Render(0x2000, shader_config), SetTexture(texture_main), SetMatrix(adaptive_viewport.GetDetails().MatrixCentered()));

Input::Mouse mouse;

Random rng(std::random_device{}());

const InterfaceStrings &interface_strings()
{
    static InterfaceStrings ret("assets/strings.refl");
    return ret;
}

static State::StateManager state_manager;

struct ProgramState : Program::DefaultBasicState
{
    void Resize()
    {
        adaptive_viewport.Update();
        mouse.SetMatrix(adaptive_viewport.GetDetails().MouseMatrixCentered());
    }

    int last_second = -1;
    int tick_counter = 0, frame_counter = 0;
    Metronome metronome = Metronome(60);

    Metronome *GetTickMetronome() override
    {
        return &metronome;
    }

    int GetFpsCap() override
    {
        return 60 * NeedFpsCap();
    }

    void EndFrame() override
    {
        int cur_second = SDL_GetTicks() / 1000;
        if (cur_second == last_second)
            return;

        last_second = cur_second;
        std::cout << "TPS: " << tick_counter << "\n";
        std::cout << "FPS: " << frame_counter << "\n\n";
        tick_counter = 0;
        frame_counter = 0;
    }

    void Tick() override
    {
        tick_counter++;

        // window.ProcessEvents();
        window.ProcessEvents({gui_controller.EventHook()});

        if (window.Resized())
        {
            Resize();
            Graphics::Viewport(window.Size());
        }
        if (window.ExitRequested())
            Program::Exit();

        gui_controller.PreTick();
        HighLevelTick();
    }

    void Render() override
    {
        frame_counter++;

        gui_controller.PreRender();
        adaptive_viewport.BeginFrame();
        HighLevelRender();
        adaptive_viewport.FinishFrame();
        Graphics::CheckErrors();
        gui_controller.PostRender();

        window.SwapBuffers();
    }


    void Init()
    {
        ImGui::StyleColorsDark();

        // Load various small fonts
        // auto monochrome_font_flags = ImGuiFreeType::Monochrome | ImGuiFreeType::MonoHinting;

        gui_controller.LoadFont("assets/Cat12.ttf", 12.0f, adjust(ImFontConfig{}, RasterizerFlags = ImGuiFreeType::Monochrome | ImGuiFreeType::LightHinting));
        gui_controller.LoadDefaultFont();
        gui_controller.RenderFontsWithFreetype();

        Graphics::Blending::Enable();
        Graphics::Blending::FuncNormalPre();
    }

    void HighLevelTick()
    {
        state_manager.Tick();
    }

    void HighLevelRender()
    {
        state_manager.Render();
    }
};

int _main_(int, char **)
{
    state_manager.SetState(State::Tag("Game"));

    ProgramState loop_state;
    loop_state.Init();
    loop_state.Resize();
    loop_state.RunMainLoop();
    return 0;
}
