#include "server_schema.hpp"
#include <nijigenerate/compat/switch_error.hpp>
#include <cstddef>

namespace nijigenerate::api::mcp::server::detail {
using providers::mcp::String;
namespace {
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
}
McpSchema::Ref depthOperationSchema() {
    auto vector=McpSchema::array(McpSchema::number())->length(2,2);
    return McpSchema::object()
        ->addProperty(literal("type"),McpSchema::enum_({literal("attached-point"),literal("ring"),literal("plane")}))
        ->addProperty(literal("index"),McpSchema::integer()->optional())
        ->addProperty(literal("amount"),McpSchema::number()->optional())
        ->addProperty(literal("p0"),vector->optional())
        ->addProperty(literal("p1"),McpSchema::array(McpSchema::number())->length(2,2)->optional())
        ->addProperty(literal("width"),McpSchema::number()->optional())
        ->addProperty(literal("hardness"),McpSchema::number()->optional())
        ->addProperty(literal("p0Angle"),McpSchema::number()->optional())
        ->addProperty(literal("p1Angle"),McpSchema::number()->optional())
        ->addProperty(literal("center"),McpSchema::array(McpSchema::number())->length(2,2)->optional())
        ->addProperty(literal("radiusX"),McpSchema::number()->optional())
        ->addProperty(literal("radiusY"),McpSchema::number()->optional())
        ->addProperty(literal("angle"),McpSchema::number()->optional())
        ->addProperty(literal("targetDepth"),McpSchema::number()->optional())
        ->addProperty(literal("flattenStrength"),McpSchema::number()->optional())->allowAdditional(true);
}
McpSchema::Ref jsonArgumentSchema(commands::CommandJsonSchema kind,String description) {
    switch(kind) {
    case commands::CommandJsonSchema::unspecified:return McpSchema::object()->allowAdditional(true)->setDescription(description);
    case commands::CommandJsonSchema::overlayObjects:
        return McpSchema::array(McpSchema::object()
            ->addProperty(literal("uuid"),McpSchema::integer()->optional()->setDescription(literal("Target Node UUID.")))
            ->addProperty(literal("overlay"),McpSchema::enum_({literal("bounds"),literal("mesh")})->optional()->setDescription(literal("Overlay kind. Use bounds or mesh.")))
            ->addProperty(literal("type"),McpSchema::enum_({literal("bounds"),literal("mesh")})->optional()->setDescription(literal("Alias of overlay.")))
            ->allowAdditional(true))->optional()->setDescription(description);
    case commands::CommandJsonSchema::depthOperation:return depthOperationSchema()->setDescription(description);
    case commands::CommandJsonSchema::depthOperations:return McpSchema::array(depthOperationSchema())->setDescription(description);
    default:throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/api/mcp/server.d"),111);
    }
}
McpSchema::Ref contextSchema() {
    return McpSchema::object()->setDescription(literal(
        "Optional execution context. Omit this property to use active app state. Use parameterValue instead of internal keyPoint indexes."))
        ->addProperty(literal("nodes"),McpSchema::array(McpSchema::integer())->optional()->setDescription(literal(
            "Node UUIDs to use as context.nodes. Obtain UUIDs from resources/find or resources/{uuid}.")))
        ->addProperty(literal("parameters"),McpSchema::array(McpSchema::integer())->optional()->setDescription(literal(
            "Parameter UUIDs to use as context.parameters.")))
        ->addProperty(literal("armedParameters"),McpSchema::array(McpSchema::integer())->optional()->setDescription(literal(
            "Parameter UUIDs for commands that explicitly use armed-parameter context. Only arm-specific commands change the GUI ArmedParameter.")))
        ->addProperty(literal("parameterValue"),McpSchema::array(McpSchema::number())->optional()->setDescription(literal(
            "Parameter-axis key values, not keypoint indexes. Use [x] for 1D or [x,y] for 2D. Values must exactly match existing parameter key values.")))
        ->addProperty(literal("bindings"),McpSchema::array(McpSchema::object()
            ->addProperty(literal("target"),McpSchema::integer()->setDescription(literal("UUID of the binding target Node or Parameter.")))
            ->addProperty(literal("name"),McpSchema::string_()->setDescription(literal("Binding name on the target, such as deform or transform.t.x."))))
            ->optional()->setDescription(literal("Binding descriptors for commands such as RemoveBinding and SetInterpolation. Requires context.parameters[0]. Bindings are resolved by parameter + target UUID + binding name; do not use Binding pseudo-UUIDs.")))
        ->allowAdditional(true);
}
}
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
namespace nijigenerate::api::mcp::server {
core::settings::SettingsJsonValue ngMcpJsonArgumentSchemaForRegression(commands::CommandJsonSchema kind) {
    return detail::jsonArgumentSchema(kind,providers::mcp::text("regression"))->toJSON();
}
void ngMcpValidateJsonArgumentForRegression(commands::CommandJsonSchema kind,core::settings::SettingsJsonValue value) {
    detail::jsonArgumentSchema(kind,providers::mcp::text("regression"))->validate(value);
}
}
#endif
