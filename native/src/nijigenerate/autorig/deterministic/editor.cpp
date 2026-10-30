#include "editor.hpp"
#include "contracts_file.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nodes/deformable.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::autorig::deterministic::editor {
namespace {
using Json = core::settings::SettingsJsonValue;
void require(bool condition, compat::RetainedString message, std::size_t line) {
    if (condition) return;
    // enforce uses the null fallback, otherwise msg.idup (including empty -> null).
    auto copied = message.isNull() ? compat::RetainedString::fromStaticLiteral("Enforcement failed") :
        message.empty() ? compat::RetainedString{} : contracts::detail::copyContractMessage(message.view());
    throw compat::ThrowableException(std::move(copied),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/editor.d"), line);
}
template<std::size_t N> void require(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) require(false, compat::RetainedString::fromStaticLiteral(message), line);
}
std::uint32_t identifier(Json value) {
    require(value.is_uint64() || (value.is_int64() && value.as_int64() >= 0),
        "Expected an unsigned model identifier", 15);
    const auto number = value.is_uint64() ? value.as_uint64() : static_cast<std::uint64_t>(value.as_int64());
    require(number <= std::numeric_limits<std::uint32_t>::max(), "Model identifier out of range", 18);
    return static_cast<std::uint32_t>(number);
}
double coordinate(Json value) {
    require(value.is_double() || value.is_int64() || value.is_uint64(), "Expected a deformation coordinate", 23);
    const double result = value.is_double() ? value.as_double() : value.is_int64() ?
        static_cast<double>(value.as_int64()) : static_cast<double>(value.as_uint64());
    require(std::isfinite(result), "Nonfinite deformation coordinate", 27);
    return result;
}
Json member(Json value, std::string_view key) {
    return value.sourceIndexValue(key);
}
}
Json ngApplyFaceProjection(Json projection, Json target, std::shared_ptr<framework::AutoRigTaskContext> task) {
    auto posed = member(projection, "points").as_array();
    auto rest = member(target, "restPoints").as_array();
    require(posed.size() == rest.size() && !posed.empty(), "Projection and rest point counts differ", 35);
    compat::Slice<float> offsets;
    for (std::size_t i = 0; i < posed.size(); ++i) {
        require(posed[i].as_array().size() == 2 && rest[i].as_array().size() == 2, "Expected 2D model-local points", 38);
        for (std::size_t axis = 0; axis < 2; ++axis) {
            const double posedCoordinate = coordinate(posed[i].as_array()[axis]);
            const double restCoordinate = coordinate(rest[i].as_array()[axis]);
            const double offset = posedCoordinate - restCoordinate;
            require(std::isfinite(offset) && offset >= -std::numeric_limits<float>::max() &&
                offset <= std::numeric_limits<float>::max(), "Offset exceeds model range", 41);
            offsets.append(static_cast<float>(offset));
        }
    }
    const auto rootId = identifier(member(target, "rootId")), nodeId = identifier(member(target, "nodeId"));
    const auto parameterId = identifier(member(target, "parameterId"));
    auto key = member(target, "keyPoint").as_array();
    require(key.size() == 2, "Expected a two-dimensional key point", 48);
    const compat::Vec2u keyPoint{identifier(key[0]), identifier(key[1])};
    require(!compat::ngRequireObject(task)->isCanceled(), "Face application canceled", 50);
    compat::ngRequireObject(task)->runOnMainThread([task, rest, offsets, rootId, nodeId, parameterId, keyPoint] {
        require(!compat::ngRequireObject(task)->isCanceled(), "Face application canceled", 52);
        auto puppet = project::incActivePuppet();
        require(puppet && compat::ngRequireObject(puppet->root)->getUUID() == rootId,
            "Active puppet differs from task target", 54);
        auto node = puppet->findNodeById(nodeId);
        auto parameter = puppet->findParameter(parameterId);
        require(node && parameter, "Face target or parameter no longer exists", 57);
        auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node);
        require(deformable && deformable->getVertices().size() == rest.size(), "Face target mesh changed since observation", 59);
        for (std::size_t i = 0; i < rest.size(); ++i) {
            auto vertex = deformable->getVertices().at(i);
            const double x = coordinate(rest[i].as_array()[0]), y = coordinate(rest[i].as_array()[1]);
            require(std::abs(vertex.x - x) <= 1e-4 * (1 + std::abs(x)) &&
                std::abs(vertex.y - y) <= 1e-4 * (1 + std::abs(y)), "Face rest geometry changed since observation", 64);
        }
        auto context = std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        context->puppet(puppet); context->nodes({node}); context->parameters({parameter});
        context->keyPoint(keyPoint); context->hasExplicitKeyPoint(true);
        auto command = std::allocate_shared<commands::model::set_deform_binding::SetDeformBindingCommand>(
            nicxlive::core::d_runtime::SourceAllocator<commands::model::set_deform_binding::SetDeformBindingCommand>{},
            compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("deform")), offsets);
        auto result = commands::ngRunCommand(command, context);
        if (!compat::ngRequireObject(result)->succeeded) require(false, result->message, 74);
    });
    core::settings::SettingsJsonObject result;
    result[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("applied"))] = Json(true);
    result[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("nodeId"))] = Json(std::uint64_t(nodeId));
    result[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("parameterId"))] =
        Json(std::uint64_t(parameterId));
    result[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("vertices"))] =
        Json(std::uint64_t(posed.size()));
    return Json(result);
}
}
