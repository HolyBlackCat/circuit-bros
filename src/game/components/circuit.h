#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "macros/maybe_const.h"
#include "meta/misc.h"
#include "program/errors.h"
#include "reflection/full_with_poly.h"
#include "reflection/short_macros.h"
#include "utils/mat.h"

namespace Components
{
    class Circuit;

    STRUCT( BasicNode POLYMORPHIC )
    {
        using id_t = unsigned int;

        MEMBERS(
            DECL(id_t INIT=0) id // Unique node id.
            DECL(ivec2 INIT{}) pos
        )

        virtual void Tick(const Circuit &circuit) = 0; // Should recalculate 'powered' state of connection points.
        virtual void Render(ivec2 offset) const = 0;
        virtual ivec2 GetVisualHalfExtent() const = 0;

        [[nodiscard]] bool VisuallyContainsPoint(ivec2 point, ivec2 radius = ivec2(0)) const
        {
            ivec2 half_extent = GetVisualHalfExtent();
            ivec2 delta = point - pos;

            return (delta > -half_extent - radius).all() && (delta < half_extent + radius).all();
        }

        static void DrawConnection(ivec2 window_offset, ivec2 pos_src, ivec2 pos_dst, bool is_inverted, bool is_powered, float src_visual_radius, float dst_visual_radius);


        SIMPLE_STRUCT_WITHOUT_NAMES( NodeAndPointId
            DECL(id_t INIT=0) node // `id` (NOT index!) of the target node.
            DECL(int INIT=0) point // Index of the target point.
            VERBATIM
            [[nodiscard]] bool operator==(const NodeAndPointId &other) const {return node == other.node && point == other.point;}
            [[nodiscard]] bool operator!=(const NodeAndPointId &other) const {return !(*this == other);}
        )

        struct PointInfo
        {
            ivec2 offset_to_node{};
            ivec2 half_extent = ivec2(16);
            float visual_radius = 4;
            float extra_out_visual_radius = 3; // This is added to visual radius of 'out' connections.

            [[nodiscard]] static const PointInfo &Default()
            {
                // I wanted to make this a static variable in `BasicNode`, but Clang has a bug that doesn't let you do that.
                static constexpr PointInfo ret{};
                return ret;
            }
        };

        SIMPLE_STRUCT_WITHOUT_NAMES( InPointCon
            DECL(NodeAndPointId) ids
            DECL(bool INIT=false) is_inverted
            VERBATIM
            InPointCon() {} // Reflection needs a default constructor.
            InPointCon(const NodeAndPointId &ids, bool is_inverted) : ids(ids), is_inverted(is_inverted) {}
            bool ConnectionIsPowered(const Circuit &circuit) const; // Checks if the connection is powered. This function has to find the remote node each time, so you should cache the result.
        )
        SIMPLE_STRUCT_WITHOUT_NAMES( InPoint
            DECL(std::vector<InPointCon>) connections
            VERBATIM
            const PointInfo *info = &PointInfo::Default();
            InPoint() {}
            InPoint(const PointInfo *info) : info(info) {}
        )

        SIMPLE_STRUCT_WITHOUT_NAMES( OutPointCon
            DECL(NodeAndPointId) ids
            VERBATIM
            OutPointCon() {} // Reflection needs a default constructor.
            OutPointCon(const NodeAndPointId &ids) : ids(ids) {}
        )
        SIMPLE_STRUCT_WITHOUT_NAMES( OutPoint
            DECL(std::vector<OutPointCon>) connections
            DECL(bool INIT=false) is_powered
            VERBATIM
            bool was_previously_powered = false; // For internal use, don't touch!
            bool was_powered_before_simulation_started = false; // For internal use, don't touch!
            const PointInfo *info = &PointInfo::Default();
            OutPoint() {}
            OutPoint(const PointInfo *info) : info(info) {}
        )

        virtual int InPointCount() const = 0;
        virtual int OutPointCount() const = 0;
      private:
        virtual InPoint &GetInPointLow(int index) = 0;
        virtual OutPoint &GetOutPointLow(int index) = 0;
      public:
        MAYBE_CONST(
            CV InPoint &GetInPoint(int index) CV
            {
                DebugAssert("Node connection point index is out of range.", index < InPointCount());
                return const_cast<BasicNode *>(this)->GetInPointLow(index);
            }
            CV OutPoint &GetOutPoint(int index) CV
            {
                DebugAssert("Node connection point index is out of range.", index < OutPointCount());
                return const_cast<BasicNode *>(this)->GetOutPointLow(index);
            }
            template <bool IsOut> CV std::conditional_t<IsOut, OutPoint, InPoint> &GetInOrOutPoint(int index) CV
            {
                if constexpr (IsOut)
                    return GetOutPoint(index);
                else
                    return GetInPoint(index);
            }
        )
        // This function can be used to determine if some 'in' and 'out' points visually overlap.
        // Given an index of an 'in' point, this returns the index of the overlapping 'out' point, or -1 if there is no overlap.
        virtual int GetOutPointOverlappingInPoint(int in_point_index) const {(void)in_point_index; return -1;}
        // Given an index of an 'out' point, this returns the index of the overlapping 'in' point, or -1 if there is no overlap.
        // The overlapping-ness relationship MUST be symmetric.
        virtual int GetInPointOverlappingOutPoint(int out_point_index) const {(void)out_point_index; return -1;}

        void Connect(int src_point_index, BasicNode &target, int dst_point_index, bool &is_inverted);
        void Disconnect(Circuit &circuit, int src_point_index, bool src_point_is_out, int src_con_index);

        // Returns point index, or -1 on failure.
        enum class Dir {in, out, in_out};
        template <Dir PointDir>
        [[nodiscard]] int GetClosestConnectionPoint(ivec2 point, bool *closest_point_dir_is_out = nullptr) const
        {
            ivec2 offset_to_node = point - pos;

            int closest_index = -1;
            int closest_dist_sqr = std::numeric_limits<int>::max();

            for (int mode_is_out = 0; mode_is_out < 2; mode_is_out++)
            {
                if (PointDir == Dir::in && mode_is_out == true) continue;
                if (PointDir == Dir::out && mode_is_out == false) continue;

                for (int count = mode_is_out ? OutPointCount() : InPointCount(), i = 0; i < count; i++)
                {
                    const PointInfo &info = mode_is_out ? *GetOutPoint(i).info : *GetInPoint(i).info;

                    ivec2 delta = offset_to_node - info.offset_to_node;
                    if ((delta.abs() > info.half_extent).any())
                        continue;

                    int this_dist_sqr = delta.len_sqr();
                    if (this_dist_sqr < closest_dist_sqr)
                    {
                        closest_dist_sqr = this_dist_sqr;
                        closest_index = i;
                        if (closest_point_dir_is_out)
                            *closest_point_dir_is_out = mode_is_out;
                    }
                }
            }

            return closest_index;
        }
    };

    using NodeStorage = Refl::PolyStorage<BasicNode>;

    class Circuit
    {
      public:
        // Nodes MUST be sorted by `id`.
        std::vector<NodeStorage> nodes;

        MAYBE_CONST(
            // Returns null if no such node.
            CV NodeStorage *FindNodeIfExists(BasicNode::id_t id) CV
            {
                auto it = std::lower_bound(nodes.begin(), nodes.end(), id, [](const NodeStorage &node, BasicNode::id_t id){return node->id < id;});
                if (it == nodes.end() || (*it)->id != id)
                    return nullptr;
                return &*it;
            }
            // Throws if no such node.
            CV NodeStorage &FindNodeOrThrow(BasicNode::id_t id) CV
            {
                CV NodeStorage *ret = FindNodeIfExists(id);
                if (!ret)
                    Program::Error("Invalid node id: ", id, ".");
                return *ret;
            }
        )

        void Tick();
        void SaveState();
        void RestoreState();
    };
}
