#include "circuit.h"

#include "game/main.h"

namespace Components
{
    namespace Nodes
    {
        SIMPLE_STRUCT( Atlas
            DECL(Graphics::TextureAtlas::Region) nodes
            VERBATIM Atlas() {texture_atlas().InitRegions(*this, ".png");}
        )
        static Atlas atlas;

        STRUCT( Or EXTENDS BasicNode )
        {
            UNNAMED_MEMBERS()

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(3);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(0, 2 + 7*powered), ivec2(7))).center(ivec2(3));
            }
        };

        STRUCT( And EXTENDS BasicNode )
        {
            UNNAMED_MEMBERS()

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(5);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(7, 11*powered), ivec2(11))).center(ivec2(5));
            }
        };
    }
}
