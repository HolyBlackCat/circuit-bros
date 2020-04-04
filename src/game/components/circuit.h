#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "macros/maybe_const.h"
#include "meta/misc.h"
#include "reflection/full_with_poly.h"
#include "reflection/short_macros.h"
#include "utils/mat.h"

namespace Components
{
    STRUCT( BasicNode POLYMORPHIC )
    {
        MEMBERS(
            DECL(ivec2 INIT{}) pos
            DECL(bool INIT=false) powered
        )

        virtual ivec2 GetVisualHalfExtent() const = 0;
        virtual void Render(ivec2 offset) const = 0;

        [[nodiscard]] bool VisuallyContainsPoint(ivec2 point, ivec2 radius = ivec2(0)) const
        {
            ivec2 half_extent = GetVisualHalfExtent();
            ivec2 delta = point - pos;

            return (delta > -half_extent - radius).all() && (delta < half_extent + radius).all();
        }
    };

    using NodeStorage = Refl::PolyStorage<BasicNode>;

    class Circuit
    {
      public:
        std::vector<NodeStorage> nodes;
    };
}
