#include "detail/native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nodes/projectable.hpp>
#include <array>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
std::array<float, 4> multiply(const nicxlive::core::math::Mat4& matrix, std::array<float, 4> input) {
    std::array<float, 4> result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            result[row] = nicxlive::core::math::detail::inmathAccumulate(result[row],
                nicxlive::core::math::detail::inmathProduct(matrix[row][column], input[column]));
    return result;
}
}
Json registerDomainParents(Json state, Json program, Task task) {
    NativeIntegerMap<std::uint32_t, framework::Text> charts;
    for (auto carrier : program.sourceIndexValue("carriers").sourceArrayValue()) {
        const auto key = uuid(carrier.sourceIndexValue("part"));
        const auto owner=carrier.sourceIndexValue("owner").as_string();
        const auto chart=ngRigString(carrier,stringLiteralEvidence("chart"),
            carrier.sourceIndexValue("role").as_string());
        const auto value=nativeText({owner.view(),"/",chart.view()});
        charts[key]=value;
    }
    auto materials = state.sourceIndexValue("materials").sourceArrayValue().duplicate();
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        core::settings::SettingsJsonArray nodes;
        NativeIntegerMap<std::uint32_t, double> sourceOrders;
        const auto readOrder = [&](Json value) {
            if (auto order = value.sourceObjectRef().if_contains("source_order")) {
                const auto key = uuid(value.sourceIndexValue("uuid"));
                const auto number = ngRigNumber(*order);
                sourceOrders[key] = number;
            }
        };
        for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) readOrder(material);
        for (auto group : state.sourceIndexValue("groups").sourceArrayValue()) readOrder(group);
        const auto sourceOrder = [&](auto&& self, NodeRef node) -> double {
            if (auto known = sourceOrders.find(node->getUUID()); known != sourceOrders.end()) return known->second;
            double result = std::numeric_limits<double>::infinity();
            for (auto childRange = node->childrenList(); auto child : childRange) result = dmin(result, self(self,child));
            return result;
        };
        std::size_t order = 0;
        const auto snapshot = [&](auto&& self, NodeRef node) -> void {
            auto record = objectEvidence({{stringLiteralEvidence("uuid"), node->getUUID()}, {stringLiteralEvidence("type"), node->typeId()},
                {stringLiteralEvidence("parent"), node == puppet->root || !node->parentPtr() ? Json{} :
                    Json(compat::ngRequireObject(node->parentPtr())->getUUID())},
                {stringLiteralEvidence("source_order"), [&] {
                    const auto source = sourceOrder(sourceOrder,node);
                    const auto fallback = static_cast<double>(1000000 + order++);
                    return dmin(source,fallback);
                }()}});
            if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(node)) {
                record.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("opacity")) = drawable->opacity;
                record.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("blend_mode")) = drawable->blendMode == nicxlive::core::nodes::BlendMode::Normal ? "Normal" : "Other";
            }
            nodes.append(record);
            for (auto childRange = node->childrenList(); auto child : childRange) self(self,child);
        };
        snapshot(snapshot,puppet->root); state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("hierarchy_nodes")) = Json(nodes);
        const auto uniform = [&](auto&& self, NodeRef node,framework::Text chart) -> bool {
            if (node->typeId() == "Part") {
                const auto known = charts.find(node->getUUID());
                if (known == charts.end() || known->second != chart) return false;
            }
            for (auto childRange = node->childrenList(); auto child : childRange) if (!self(self,child, chart)) return false;
            return true;
        };
        for (auto& material : materials) {
            if (material.sourceIndexValue("static").as_bool()) continue;
            auto part = puppet->findNodeById(uuid(material.sourceIndexValue("uuid")));
            auto unit = part;
            // The source AA lookup is conditional on both parent tests.
            while (unit->parentPtr() && unit->parentPtr() != puppet->root) {
                const auto sourceParent = unit->parentPtr();
                const auto chart = readNativeMap(charts,part->getUUID(),865);
                if (!uniform(uniform,sourceParent,chart)) break;
                unit = unit->parentPtr();
            }
            material.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("carrier_unit")) = Json(unit->getUUID());
            material.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("parent")) = Json(compat::ngRequireObject(unit->parentPtr())->getUUID());
            const auto rootInverse=core::math::source_matrix::inverse(puppet->root->transform().matrix());
            const auto parentMatrix=compat::ngRequireObject(unit->parentPtr())->transform().matrix();
            const auto matrix=rootInverse*parentMatrix;
            const auto x = multiply(matrix, {1, 0, 0, 0});
            const auto y = multiply(matrix, {0, 1, 0, 0});
            const auto origin = multiply(matrix, {0, 0, 0, 1});
            material.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("parent_to_root")) = array({x[0], y[0], origin[0], x[1], y[1], origin[1]});
        }
    });
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("materials")) = Json(materials);
    return state;
}
}
