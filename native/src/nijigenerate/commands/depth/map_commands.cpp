#include "map_json.hpp"
#include <nijigenerate/io/depthmap_psd.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/viewport/depth/tools/operation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <nodes/grid_deformer.hpp>
#include <bit>
#include <cstddef>
#include <utility>
namespace nijigenerate::commands::depth::map {
using namespace detail;
using namespace io::depthmap_psd;
#define NG_DEPTH_MAP_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map." #Type))
namespace {
template<std::size_t N, std::size_t M>
DepthMapConstructorText translatedConstructorTexts(const char (&label)[N], const char (&description)[M]) {
    auto actualLabel = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(label));
    auto actualDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(actualLabel), std::move(actualDescription)};
}
}
#define NG_DEPTH_MAP_CONSTRUCTOR(Class, Base, Label, Description) \
    DepthMapConstructorText Class::sourceConstructorTexts() { \
        return translatedConstructorTexts(Label, Description); \
    } \
    Class::Class() : Class(sourceConstructorTexts()) {} \
    Class::Class(DepthMapConstructorText texts) : Base(std::move(texts.first), std::move(texts.second)) { \
        NG_DEPTH_MAP_CLASS_INFO(Class); \
    }
NG_DEPTH_MAP_CONSTRUCTOR(ListDepthsCommand, NativeBase, "List Depths", "List per-vertex depth values")
NG_DEPTH_MAP_CONSTRUCTOR(SetDepthsCommand, ExCommand, "Set Depths", "Set per-vertex depth values")
NG_DEPTH_MAP_CONSTRUCTOR(ClearDepthsCommand, ExCommand, "Clear Depths", "Clear per-vertex depth values")
NG_DEPTH_MAP_CONSTRUCTOR(ListDepthOpsCommand, NativeBase, "List Depth Operations", "List saved depth operations")
NG_DEPTH_MAP_CONSTRUCTOR(SetDepthOpsCommand, ExCommand, "Set Depth Operations", "Replace saved depth operations")
NG_DEPTH_MAP_CONSTRUCTOR(AddDepthOpCommand, ExCommand, "Add Depth Operation", "Add one saved depth operation")
NG_DEPTH_MAP_CONSTRUCTOR(UpdateDepthOpCommand, ExCommand, "Update Depth Operation",
    "Replace one saved depth operation")
NG_DEPTH_MAP_CONSTRUCTOR(RemoveDepthOpCommand, ExCommand, "Remove Depth Operation",
    "Remove one saved depth operation")
NG_DEPTH_MAP_CONSTRUCTOR(MoveDepthOpCommand, ExCommand, "Move Depth Operation", "Move one saved depth operation")
NG_DEPTH_MAP_CONSTRUCTOR(ClearDepthOpsCommand, ExCommand, "Clear Depth Operations", "Clear saved depth operations")
NG_DEPTH_MAP_CONSTRUCTOR(ApplyDepthOpsCommand, ExCommand, "Apply Depth Operations",
    "Bake saved depth operations into per-vertex depths")
#undef NG_DEPTH_MAP_CONSTRUCTOR
DepthMapConstructorText ImportPSDDepthsCommand::sourceConstructorTexts() {
    return translatedConstructorTexts("Import PSD Depth Map",
        "Import PSD or PNG grayscale depth data into mapped depth targets.");
}
ImportPSDDepthsCommand::ImportPSDDepthsCommand(compat::RetainedString path, bool invert, float backDepth,
    float frontDepth, float depthScale, compat::RetainedString convolution, compat::RetainedString channel,
    std::int32_t customRadius, float alphaThreshold, compat::RetainedString missingPolicy, bool matchDirectGridName)
    : ImportPSDDepthsCommand(sourceConstructorTexts(), path, invert, backDepth, frontDepth, depthScale,
        convolution, channel, customRadius, alphaThreshold, missingPolicy, matchDirectGridName) {}
ImportPSDDepthsCommand::ImportPSDDepthsCommand(DepthMapConstructorText texts,
    compat::RetainedString path, bool invert, float backDepth, float frontDepth, float depthScale,
    compat::RetainedString convolution, compat::RetainedString channel, std::int32_t customRadius,
    float alphaThreshold, compat::RetainedString missingPolicy, bool matchDirectGridName)
    : NativeBase{std::move(texts.first), std::move(texts.second),
        path, invert, backDepth, frontDepth, depthScale, convolution, channel, customRadius, alphaThreshold,
        missingPolicy, matchDirectGridName} { NG_DEPTH_MAP_CLASS_INFO(ImportPSDDepthsCommand); }
#undef NG_DEPTH_MAP_CLASS_INFO
#define NG_DEPTH_MAP_PREPARE(Type, Base) \
    void Type::prepareSourceClassInfo() { \
        Base::prepareSourceClassInfo(); \
        ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
    }
NG_DEPTH_MAP_PREPARE(ListDepthsCommand, NativeBase)
NG_DEPTH_MAP_PREPARE(SetDepthsCommand, ExCommand<SetDepthsArguments>)
NG_DEPTH_MAP_PREPARE(ClearDepthsCommand, ExCommand<ClearDepthsArguments>)
NG_DEPTH_MAP_PREPARE(ListDepthOpsCommand, NativeBase)
NG_DEPTH_MAP_PREPARE(SetDepthOpsCommand, ExCommand<SetDepthOpsArguments>)
NG_DEPTH_MAP_PREPARE(AddDepthOpCommand, ExCommand<AddDepthOpArguments>)
NG_DEPTH_MAP_PREPARE(UpdateDepthOpCommand, ExCommand<UpdateDepthOpArguments>)
NG_DEPTH_MAP_PREPARE(RemoveDepthOpCommand, ExCommand<RemoveDepthOpArguments>)
NG_DEPTH_MAP_PREPARE(MoveDepthOpCommand, ExCommand<MoveDepthOpArguments>)
NG_DEPTH_MAP_PREPARE(ClearDepthOpsCommand, ExCommand<ClearDepthOpsArguments>)
NG_DEPTH_MAP_PREPARE(ApplyDepthOpsCommand, ExCommand<ApplyDepthOpsArguments>)
NG_DEPTH_MAP_PREPARE(ImportPSDDepthsCommand, NativeBase)
#undef NG_DEPTH_MAP_PREPARE
std::shared_ptr<ListDepthsCommand::Result> ListDepthsCommand::runTyped(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto values = requireDepthMapped(target)->copyDepths();
    Object result;
    result[compat::RetainedString::fromStaticLiteral("target")] = Json(compat::ngRequireObject(target)->getUUID());
    result[compat::RetainedString::fromStaticLiteral("depths")] = depthsToJson(values);
    result[compat::RetainedString::fromStaticLiteral("count")] =
        Json(values.isNull() ? std::int64_t(-1) : std::bit_cast<std::int64_t>(values.size()));
    return ExCommandResult<Json>::opCall(true, Json(result));
}
std::shared_ptr<CommandResult> SetDepthsCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto mapped = requireDepthMapped(target);
    auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(target);
    if (!deformable) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("target must be a Deformable"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1219);
    if (depths.size() != deformable->getVertices().size()) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("depths length must match target vertices"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1220);
    replaceDepthsWithUndo(target, depths, compat::RetainedString::fromStaticLiteral("Set Depths"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ClearDepthsCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    replaceDepthsWithUndo(target, {}, compat::RetainedString::fromStaticLiteral("Clear Depths"));
    return CommandResult::opCall(true);
}
std::shared_ptr<ListDepthOpsCommand::Result> ListDepthOpsCommand::runTyped(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto ops = requireDepthOperated(target)->copyDepthOps();
    Object result;
    result[compat::RetainedString::fromStaticLiteral("target")] = Json(compat::ngRequireObject(target)->getUUID());
    result[compat::RetainedString::fromStaticLiteral("operations")] = depthOpsToJson(ops);
    result[compat::RetainedString::fromStaticLiteral("count")] = Json(std::bit_cast<std::int64_t>(ops.size()));
    return ExCommandResult<Json>::opCall(true, Json(result));
}
std::shared_ptr<CommandResult> SetDepthOpsCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    replaceDepthOpsWithUndo(target, depthOpsFromJson(operations),
        compat::RetainedString::fromStaticLiteral("Set Depth Operations"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> AddDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto operated = requireDepthOperated(target);
    auto ops = operated->copyDepthOps();
    auto op = depthOpFromJson(operation);
    if (index < 0 || std::size_t(index) >= ops.size()) ops.append(op);
    else {
        const auto i = std::size_t(index);
        compat::Slice<ext::nodes::ExDepthOp> next;
        next.append(ops.slice(0, i)); next.append(op); next.append(ops.slice(i, ops.size())); ops = next;
    }
    replaceDepthOpsWithUndo(target, ops, compat::RetainedString::fromStaticLiteral("Add Depth Operation"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> UpdateDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto operated = requireDepthOperated(target); auto ops = operated->copyDepthOps();
    if (!(index >= 0 && std::size_t(index) < ops.size())) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operation index out of range"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1297);
    auto& destination = ops[std::size_t(index)];
    const auto replacement = depthOpFromJson(operation);
    destination = replacement;
    replaceDepthOpsWithUndo(target, ops, compat::RetainedString::fromStaticLiteral("Update Depth Operation"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> RemoveDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto operated = requireDepthOperated(target); auto ops = operated->copyDepthOps();
    if (!(index >= 0 && std::size_t(index) < ops.size())) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operation index out of range"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1314);
    const auto i = std::size_t(index);
    compat::Slice<ext::nodes::ExDepthOp> next;
    next.append(ops.slice(0, i)); next.append(ops.slice(i + 1, ops.size())); ops = next;
    replaceDepthOpsWithUndo(target, ops, compat::RetainedString::fromStaticLiteral("Remove Depth Operation"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> MoveDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto operated = requireDepthOperated(target); auto ops = operated->copyDepthOps();
    if (!(fromIndex >= 0 && std::size_t(fromIndex) < ops.size())) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("fromIndex out of range"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1333);
    if (!(toIndex >= 0 && std::size_t(toIndex) < ops.size())) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("toIndex out of range"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 1334);
    const auto op = ops[std::size_t(fromIndex)];
    const auto from = std::size_t(fromIndex);
    compat::Slice<ext::nodes::ExDepthOp> removed;
    removed.append(ops.slice(0, from)); removed.append(ops.slice(from + 1, ops.size())); ops = removed;
    const auto to = std::size_t(toIndex);
    compat::Slice<ext::nodes::ExDepthOp> next;
    next.append(ops.slice(0, to)); next.append(op); next.append(ops.slice(to, ops.size())); ops = next;
    replaceDepthOpsWithUndo(target, ops, compat::RetainedString::fromStaticLiteral("Move Depth Operation"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ClearDepthOpsCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    replaceDepthOpsWithUndo(target, {}, compat::RetainedString::fromStaticLiteral("Clear Depth Operations"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ApplyDepthOpsCommand::run(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto grid = requireDepthGrid(target); auto operated = requireDepthOperated(target);
    const auto ops = operated->copyDepthOps();
    replaceDepthsWithUndo(target, viewport::depth::tools::operation::ngComputeDepthsFromOps(
        grid, ops, operated->copyDepthOpBaseDepths()),
        compat::RetainedString::fromStaticLiteral("Apply Depth Operations"));
    return CommandResult::opCall(true);
}
std::shared_ptr<ImportPSDDepthsCommand::Result> ImportPSDDepthsCommand::runTyped(std::shared_ptr<Context>) {
    const auto commandOwner = weak_from_this().lock();
    auto puppet = project::incActivePuppet();
    if (!puppet) return ExCommandResult<Json>::opCall(false, Json(nullptr),
        compat::RetainedString::fromStaticLiteral("No active puppet"));
    if (path.empty()) return ExCommandResult<Json>::opCall(false, Json(nullptr),
        compat::RetainedString::fromStaticLiteral("Path not provided"));
    PsdDepthImportSettings settings;
    settings.invert = invert; settings.backDepth = backDepth; settings.frontDepth = frontDepth;
    settings.depthScale = depthScale; settings.alphaThreshold = alphaThreshold;
    settings.convolution = ngPsdDepthConvolutionFromString(convolution);
    settings.channel = ngPsdDepthChannelFromString(channel); settings.customRadius = customRadius;
    settings.missingPolicy = ngPsdDepthMissingPolicyFromString(missingPolicy);
    settings.matchDirectGridName = matchDirectGridName;
    auto imported = ngBuildPsdDepthsFromSource(puppet, path, settings);
    PsdDepthComposedView composed; compat::RetainedString error;
    if (!ngComposePsdDepthImportResult(imported, composed, error)) {
        const auto summary = ngPsdDepthImportSummaryToJson(imported, 0);
        const auto message = compat::RetainedString(
            std::string("PSD depth map composition failed: ") + std::string(error.view()));
        return ExCommandResult<Json>::opCall(false, summary, message);
    }
    return ngApplyPsdDepthImportResult(composed);
}
void ngInitDepthMapCommands() {
    initialization::Program::run<initialization::Table>();
}
}

