#include "circuit.h"

#include "game/draw.h"
#include "game/main.h"
#include "macros/adjust.h"
#include "meta/misc.h"

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
            inline static const PointInfo point_info = adjust_(PointInfo::Default(), visual_radius = 3.18);

            MEMBERS(
                DECL(InPoint INIT=&point_info) in
                DECL(OutPoint INIT=&point_info) out
            )

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(3);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(0, 2 + 7*powered), ivec2(7))).center(ivec2(3));
            }

            int InPointCount() const override {return 1;}
            int OutPointCount() const override {return 1;}
            InPoint &GetInPointLow(int index) override {(void)index; return in;}
            OutPoint &GetOutPointLow(int index) override {(void)index; return out;}
            int GetOutPointOverlappingInPoint(int in_point_index) const override {(void)in_point_index; return 0;}
            int GetInPointOverlappingOutPoint(int in_point_index) const override {(void)in_point_index; return 0;}
        };

        STRUCT( And EXTENDS BasicNode )
        {
            inline static const PointInfo point_info = adjust_(PointInfo::Default(), visual_radius = 5.18);

            MEMBERS(
                DECL(InPoint INIT=&point_info) in
                DECL(OutPoint INIT=&point_info) out
            )

            ivec2 GetVisualHalfExtent() const override
            {
                return ivec2(5);
            }

            void Render(ivec2 offset) const override
            {
                r.iquad(pos + offset, atlas.nodes.region(ivec2(7, 11*powered), ivec2(11))).center(ivec2(5));
            }

            int InPointCount() const override {return 1;}
            int OutPointCount() const override {return 1;}
            InPoint &GetInPointLow(int index) override {(void)index; return in;}
            OutPoint &GetOutPointLow(int index) override {(void)index; return out;}
            int GetOutPointOverlappingInPoint(int in_point_index) const override {(void)in_point_index; return 0;}
            int GetInPointOverlappingOutPoint(int in_point_index) const override {(void)in_point_index; return 0;}
        };
    }

    void BasicNode::DrawConnection(ivec2 window_offset, ivec2 pos_src, ivec2 pos_dst, bool is_inverted, bool is_powered, float src_visual_radius, float dst_visual_radius)
    {
        constexpr int extra_visible_space = 4; // For a good measure.
        for (int m = 0; m <= 2; m++)
        {
            if (pos_src[m] > screen_size[m]/2 && pos_dst[m] > screen_size[m]/2)
                return;
            if (pos_src[m] < -screen_size[m]/2 && pos_dst[m] < -screen_size[m]/2)
                return;
        }

        bool src_visible = (pos_src.abs() <= screen_size/2 + src_visual_radius + int(src_visual_radius) + extra_visible_space).all();
        bool dst_visible = (pos_dst.abs() <= screen_size/2 + src_visual_radius + int(dst_visual_radius) + extra_visible_space).all();

        if (pos_src == pos_dst)
            return;

        pos_src += window_offset;
        pos_dst += window_offset;

        fvec2 a = pos_src + 0.5;
        fvec2 b = pos_dst + 0.5;

        fvec2 src_deco_pos;

        if (src_visible || dst_visible)
        {
            fvec2 dir = b - a;
            float dist = dir.len();
            dir /= dist;

            float max_visual_radius = dist * 0.45;

            if (src_visible)
            {
                clamp_var_max(src_visual_radius, max_visual_radius);

                a += dir * src_visual_radius;
                src_deco_pos = a;
                if (is_inverted)
                    a += dir * 2.3;
            }

            if (dst_visible)
            {
                clamp_var_max(dst_visual_radius, max_visual_radius);

                b -= dir * dst_visual_radius;
            }
        }

        Draw::Line(a, b, 1).tex(Nodes::atlas.nodes.pos + fvec2(0,is_powered+0.5), fvec2(5,0));

        if (src_visible)
        {
            r.iquad(iround(src_deco_pos-0.5), Nodes::atlas.nodes.region(ivec2(0,16+5*is_powered+10*is_inverted), ivec2(5))).center(fvec2(2));
        }
    }

    void BasicNode::Connect(int src_out_point_index, BasicNode &target, int dst_in_point_index, bool &is_inverted)
    {
        DebugAssertNameless(src_out_point_index < OutPointCount());
        DebugAssertNameless(dst_in_point_index < target.InPointCount());

        OutPoint &src_point = GetOutPoint(src_out_point_index);
        InPoint &dst_point = target.GetInPoint(dst_in_point_index);
        NodeAndPointId src_ids{id, src_out_point_index};
        NodeAndPointId dst_ids{target.id, dst_in_point_index};

        // Destroy the old connection, if any.
        auto HasIdsEqualTo = [](BasicNode::NodeAndPointId ids){return [ids](const auto &obj){return obj.ids == ids;};};
        bool already_had_con = std::erase_if(src_point.connections, HasIdsEqualTo(dst_ids)) > 0
                            && std::erase_if(dst_point.connections, HasIdsEqualTo(src_ids)) > 0;
        // If already had a connection, flip the inverted-ness.
        if (already_had_con)
            is_inverted = !is_inverted;

        // Destroy the overlapping connection in the opposite direction, if any.
        int src_ov_in_point_index = GetInPointOverlappingOutPoint(src_out_point_index);
        int dst_ov_out_point_index = target.GetOutPointOverlappingInPoint(dst_in_point_index);
        if (src_ov_in_point_index != -1 && dst_ov_out_point_index != -1)
        {
            NodeAndPointId src_ov_ids{id, src_ov_in_point_index};
            NodeAndPointId dst_ov_ids{target.id, dst_ov_out_point_index};

            InPoint &src_ov_point = GetInPoint(src_ov_in_point_index);
            OutPoint &dst_ov_point = target.GetOutPoint(dst_ov_out_point_index);
            std::erase_if(src_ov_point.connections, HasIdsEqualTo(dst_ov_ids));
            std::erase_if(dst_ov_point.connections, HasIdsEqualTo(src_ov_ids));
        }

        // Add the connection.
        src_point.connections.push_back(OutPointCon(dst_ids, is_inverted));
        dst_point.connections.push_back(InPointCon(src_ids));
    }

    void BasicNode::Disconnect(Circuit &circuit, int src_point_index, bool src_point_is_out, int src_con_index)
    {
        Meta::with_cexpr_flags(src_point_is_out) >> [&](auto is_out_tag)
        {
            constexpr bool is_out = is_out_tag.value;

            auto &src_point = GetInOrOutPoint<is_out>(src_point_index);

            DebugAssertNameless(src_con_index < int(src_point.connections.size()));
            auto &src_con = src_point.connections[src_con_index];

            const NodeAndPointId src_ids{id, src_point_index};
            const NodeAndPointId &dst_ids = src_con.ids;

            BasicNode &dst_node = *circuit.FindNodeOrThrow(dst_ids.node);
            auto &dst_point = dst_node.GetInOrOutPoint<!is_out>(dst_ids.point);

            auto HasIdsEqualTo = [](BasicNode::NodeAndPointId ids){return [ids](const auto &obj){return obj.ids == ids;};};
            std::erase_if(src_point.connections, HasIdsEqualTo(dst_ids));
            std::erase_if(dst_point.connections, HasIdsEqualTo(src_ids));
        };
    }
}
