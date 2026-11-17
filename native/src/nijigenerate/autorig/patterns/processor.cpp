#include "processor.hpp"
#include "core.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <stdexcept>
#include <algorithm>

namespace nijigenerate::autorig::patterns {
namespace {
using namespace framework;
template<std::size_t N> Text t(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
AutoRigPortSpec port(const char* name, AutoRigValueKind kind, bool required = true) {
    return {Text(std::string(name)), kind, required,
        kind == AutoRigValueKind::Json ? t("application/json") : t("application/octet-stream"), {}, {}};
}
AutoRigPortSpec objectPort(const char* name) { return port(name, AutoRigValueKind::Object); }
AutoRigPortSpec blob(const char* name) { return port(name, AutoRigValueKind::Blob); }
AutoRigPortSpec jsonPort(const char* name, bool required = true) {
    return port(name, AutoRigValueKind::Json, required);
}
Json patternObject(const framework::Json& value) {
    using Type = core::settings::SettingsJsonType;
    switch (value.sourceType()) {
    case Type::Null: return nullptr;
    case Type::True: return true;
    case Type::False: return false;
    case Type::String: return Json(value.as_string().view());
    case Type::Integer: return value.as_int64();
    case Type::UInteger: return value.as_uint64();
    case Type::Floating: return value.as_double();
    case Type::Array: {
        boost::json::array result;
        for (const auto& item : value.sourceArrayValue()) result.push_back(patternObject(item));
        return result;
    }
    case Type::Object: {
        boost::json::object result;
        for (const auto& entry : value.sourceObjectRef()) result.emplace(entry.key(), patternObject(entry.item));
        return result;
    }
    }
    throw std::invalid_argument("Unsupported structural evidence value");
}
framework::Json settingsObject(const Json& value) {
    using boost::json::kind;
    using core::settings::SettingsString;
    switch (value.kind()) {
    case kind::null: return {};
    case kind::bool_: return value.as_bool();
    case kind::string: return SettingsString(Text(std::string(value.as_string())));
    case kind::int64: return value.as_int64();
    case kind::uint64: return value.as_uint64();
    case kind::double_: return value.as_double();
    case kind::array: {
        core::settings::SettingsJsonArray result;
        for (const auto& item : value.as_array()) result.append(settingsObject(item));
        return result;
    }
    case kind::object: {
        core::settings::SettingsJsonObject result;
        for (const auto& entry : value.as_object())
            result[SettingsString(Text(std::string(entry.key())))] = settingsObject(entry.value());
        return result;
    }
    }
    throw std::invalid_argument("Unsupported structural evidence value");
}
Json readJson(const std::shared_ptr<AutoRigTaskContext>& context, const char* name) {
    if (!context->hasInput(name)) return boost::json::object{};
    return patternObject(context->input(name).json);
}
void publishJson(const std::shared_ptr<AutoRigTaskContext>& context, const char* name, const Json& value) {
    context->publishJson(name, settingsObject(value));
}
// Match roles form an atomic unit. Filtering a single role out of a match
// would change the existing pattern algorithm and can split shared edits.
bool containsMatch(const AutoRigTargetScope& scope, const Json& match) {
    for (const auto& role : match.as_object().at("roles").as_object())
        for (const auto& node : role.value().as_array())
            if (!scope.contains(node.to_number<std::uint32_t>())) return false;
    return true;
}
std::vector<std::uint32_t> matchMembers(const Json& match) {
    std::vector<std::uint32_t> result;
    for (const auto& role : match.as_object().at("roles").as_object())
        for (const auto& node : role.value().as_array()) result.push_back(node.to_number<std::uint32_t>());
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
std::string matchTargetId(std::string_view model, std::string_view tag, const Json& match) {
    std::string result(model);
    result += '/';
    result += tag;
    for (const auto node : matchMembers(match)) result += "/" + std::to_string(node);
    return result;
}
Json restrictSelection(Json selection, const AutoRigTargetScope& scope) {
    auto& object = selection.as_object();
    for (const auto key : {"matches", "observed_matches"}) {
        if (auto* matches = object.if_contains(key)) {
            boost::json::array accepted;
            for (const auto& match : matches->as_array())
                if (containsMatch(scope, match)) accepted.push_back(match);
            *matches = std::move(accepted);
        }
    }
    for (const auto key : {"parts", "nodes"}) {
        boost::json::array accepted;
        for (const auto& node : object.at(key).as_array())
            if (scope.contains(node.to_number<std::uint32_t>())) accepted.push_back(node);
        object[key] = std::move(accepted);
    }
    // An unresolved item in a different scope cannot block this selection.
    boost::json::array unresolved;
    for (const auto& item : object.at("unresolved").as_array()) {
        const auto node = item.as_object().if_contains("uuid");
        if (node && scope.contains(node->to_number<std::uint32_t>())) unresolved.push_back(item);
    }
    object["unresolved"] = std::move(unresolved);
    object["restricted_selection"] = true;
    object.erase("plan");
    return selection;
}
InxDocument readModel(const std::shared_ptr<AutoRigTaskContext>& context, const char* name) {
    auto value = context->input(name);
    const auto bytes = value.borrowTaskBlob();
    return ngReadPatternInx({bytes.data(), bytes.size()});
}
void publishModel(const std::shared_ptr<AutoRigTaskContext>& context, const InxDocument& source, const Json& value) {
    auto bytes = ngWritePatternInx(source, value);
    compat::Slice<std::uint8_t> copy(bytes.begin(), bytes.end());
    context->publishBlob("model", std::move(copy));
}
}
StructuralPatternProcessor::StructuralPatternProcessor(std::string pattern) : pattern_(std::move(pattern)) {
    (void)ngPatternSpec(pattern_);
}
framework::Text StructuralPatternProcessor::procId() { return framework::Text("structural-" + pattern_); }
framework::Text StructuralPatternProcessor::displayName() {
    return framework::Text(pattern_ + ": " + ngPatternSpec(pattern_).label);
}
compat::Slice<framework::AutoRigTaskSpec> StructuralPatternProcessor::tasks() {
    compat::Slice<AutoRigTaskSpec> result{
        {t("detect"), t("Detect structural targets"), {}, {blob("model"), jsonPort("selection", false)},
            {jsonPort("targets"), objectPort("target-evidence"), blob("model"), objectPort("target-associations")}, {}},
        {t("deform"), t("Generate deformation controls"), {},
            {blob("model"), objectPort("targets"), objectPort("target-associations"), jsonPort("recipe", false)},
            {blob("model"), jsonPort("receipt"), objectPort("receipt-evidence"),
                port("verified-selection", AutoRigValueKind::Object, false)},
            {{t("model"), t("detect"), t("model")}, {t("targets"), t("detect"), t("target-evidence")},
                {t("target-associations"), t("detect"), t("target-associations")}}},
        {t("verify"), t("Verify generated model"), {},
            {blob("before"), blob("after"), objectPort("targets"), objectPort("receipt"),
                port("verified-selection", AutoRigValueKind::Object, false)},
            {blob("model"), jsonPort("report"), jsonPort("targets")}, {{t("before"), t("detect"), t("model")}, {t("after"), t("deform"), t("model")},
                {t("targets"), t("detect"), t("target-evidence")}, {t("receipt"), t("deform"), t("receipt-evidence")},
                {t("verified-selection"), t("deform"), t("verified-selection")}}}
    };
    result[0].phase = AutoRigTaskPhase::Collect;
    result[1].phase = AutoRigTaskPhase::Apply;
    result[2].phase = AutoRigTaskPhase::Verify;
    for (auto& task : result) task.supportsTargetScope = true;
    return result;
}
compat::Slice<framework::AutoRigWorkflowSpec> StructuralPatternProcessor::workflows() {
    framework::AutoRigWorkflowSpec workflow;
    workflow.id = "rig-pattern";
    workflow.label = displayName();
    workflow.description = "Detect or manually select Part/Node roles, generate controls, verify INX.";
    workflow.inputs = {blob("model"), jsonPort("selection", false), jsonPort("recipe", false)};
    workflow.outputs = {blob("model"), jsonPort("targets"), jsonPort("report")};
    // A selected task includes its dependency closure. Use one step so detect/deform
    // are not duplicated with unbound model inputs in separate step instances.
    workflow.steps = {{t("rig"), procId(), t("verify"), {}}};
    workflow.inputBindings = {{t("model"), t("rig"), t("detect"), t("model")},
        {t("selection"), t("rig"), t("detect"), t("selection")}, {t("recipe"), t("rig"), t("deform"), t("recipe")}};
    workflow.outputBindings = {{t("model"), t("rig"), t("model")}, {t("targets"), t("rig"), t("targets")},
        {t("report"), t("rig"), t("report")}};
    workflow.preferred = true;
    return {workflow};
}
void StructuralPatternProcessor::executeTask(framework::Text task, std::shared_ptr<framework::AutoRigTaskContext> context) {
    try {
    const auto checkpoint = [context] {
        if (context->isCanceled()) throw std::runtime_error("Structural pattern task canceled");
    };
    checkpoint();
    const auto read = [&](const char* port) {
        auto model = readModel(context, port);
        if (const auto scope = context->targetScope()) {
            const auto uuid = model.document.as_object().at("nodes").as_object().at("uuid").to_number<std::uint32_t>();
            if (scope->modelIdentity.view() != std::to_string(uuid))
                throw std::invalid_argument("Scoped structural evidence belongs to another model");
        }
        return model;
    };
    if (task.view() == "detect") {
        auto model = read("model");
        auto targets = ngDetectPattern(pattern_, model.document, readJson(context, "selection"), checkpoint);
        if (auto scope = context->targetScope()) targets = restrictSelection(std::move(targets), *scope);
        const auto root = model.document.as_object().at("nodes").as_object().at("uuid").to_number<std::uint32_t>();
        const auto modelId = std::to_string(root);
        AutoRigTargetAssociations recognized{Text(modelId), {}};
        const auto tag = std::string("pattern/") + pattern_;
        for (const auto& match : targets.as_object().at("matches").as_array()) {
            const Text targetId(matchTargetId(modelId, tag, match));
            for (const auto node : matchMembers(match)) recognized.members.push_back({Text(tag), targetId, node});
        }
        context->publishEvidence("target-associations", std::move(recognized));
        publishJson(context, "targets", targets);
        context->publishEvidence("target-evidence", std::move(targets));
        context->publishBlob("model", context->input("model").readBlob());
    } else if (task.view() == "deform") {
        auto model = read("model");
        auto targets = *context->inputEvidence<Json>("targets");
        const auto recognized = context->inputEvidence<AutoRigTargetAssociations>("target-associations");
        const auto root = model.document.as_object().at("nodes").as_object().at("uuid").to_number<std::uint32_t>();
        if (recognized->modelIdentity.view() != std::to_string(root))
            throw std::invalid_argument("Pattern associations belong to another model");
        const auto tag = std::string("pattern/") + pattern_;
        for (const auto& match : targets.as_object().at("matches").as_array()) {
            const auto targetId = matchTargetId(recognized->modelIdentity.view(), tag, match);
            for (const auto node : matchMembers(match)) {
                const bool certified = std::ranges::any_of(recognized->members, [&](const auto& member) {
                    return member.tag.view() == tag && member.targetId.view() == targetId && member.node == node;
                });
                if (!certified) throw std::invalid_argument("Pattern selection has an unrecognized target");
            }
        }
        auto recipe = readJson(context, "recipe");
        if (auto scope = context->targetScope()) {
            targets = restrictSelection(std::move(targets), *scope);
            if (targets.as_object().at("matches").as_array().empty()) {
                context->recordUnit({{}, AutoRigTaskState::Blocked, "No confirmed pattern match in scope"});
                throw std::invalid_argument("No confirmed pattern match in scope");
            }
            if (recipe.as_object().empty()) {
                const auto plan = ngPlanPattern(pattern_, model.document, targets, checkpoint);
                for (const auto& match : plan.as_object().at("matches").as_array())
                    if (!containsMatch(*scope, match))
                        throw std::invalid_argument("Pattern plan writes outside the selected targets");
            }
        }
        const bool manualPartial = targets.as_object().at("mode") == "manual" &&
            !targets.as_object().at("matches").as_array().empty() &&
            !targets.as_object().at("unresolved").as_array().empty();
        if (manualPartial) {
            for (const auto& rejected : targets.as_object().at("unresolved").as_array())
                context->recordUnit({{}, AutoRigTaskState::Blocked,
                    Text(std::string(rejected.as_object().at("reason").as_string()))});
            targets.as_object()["unresolved"] = boost::json::array{};
        }
        auto result = ngDeformPattern(pattern_, model.document, targets, recipe, checkpoint);
        if (manualPartial)
            for (const auto& match : targets.as_object().at("matches").as_array())
                context->recordUnit({matchMembers(match), AutoRigTaskState::Succeeded, {}});
        checkpoint();
        auto receipt = result.as_object().at("receipt");
        publishJson(context, "receipt", receipt);
        context->publishEvidence("receipt-evidence", std::move(receipt));
        publishModel(context, model, result.as_object().at("document"));
        if (context->targetScope() || manualPartial) context->publishEvidence("verified-selection", std::move(targets));
    } else if (task.view() == "verify") {
        auto before = read("before"), after = read("after");
        if (before.prefix != after.prefix || before.payload != after.payload)
            throw std::invalid_argument("Texture payload changed during structural rigging");
        const auto targets = context->inputEvidence<Json>(
            context->hasInput("verified-selection") ? "verified-selection" : "targets");
        auto report = ngVerifyPattern(pattern_, before.document, after.document, *targets,
            *context->inputEvidence<Json>("receipt"), checkpoint);
        checkpoint();
        publishJson(context, "report", report);
        publishJson(context, "targets", *targets);
        publishModel(context, after, after.document);
    } else throw std::invalid_argument("Unknown structural pattern task");
    } catch (const compat::ThrowableException&) {
        throw;
    } catch (const std::exception& error) {
        throw compat::ThrowableException(framework::Text(std::string(error.what())),
            framework::Text::fromStaticLiteral("nijigenerate/autorig/patterns/processor.cpp"), 0);
    }
}
}
