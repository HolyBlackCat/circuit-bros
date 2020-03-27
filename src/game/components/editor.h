#pragma once

#include <memory>

namespace Components
{
    class Editor
    {
        struct State;
        std::unique_ptr<State> state;

      public:
        Editor();
        Editor(const Editor &);
        Editor(Editor &&) noexcept;
        Editor &operator=(Editor) noexcept;
        ~Editor();

        [[nodiscard]] bool IsOpen() const;
        void SetOpen(bool is_open, bool immediately = false);

        void Tick();
        void Render() const;
    };
}
