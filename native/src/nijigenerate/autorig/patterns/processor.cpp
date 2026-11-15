#include "processor.hpp"
#include "core.hpp"
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <stdexcept>

namespace nijigenerate::autorig::patterns {
namespace {
using namespace framework;
template<std::size_t N> Text t(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
AutoRigPortSpec port(const char* name, AutoRigValueKind kind, bool required = true) {
    return {Text(std::string(name)), kind, required,
        kind == AutoRigValueKind::Json ? t("application/json") : t("application/octet-stream"), {}, {}};
}
AutoRigPortSpec blob(const char* name) { return port(name, AutoRigValueKind::Blob); }
AutoRigPortSpec jsonPort(const char* name, bool required = true) {
    return port(name, AutoRigValueKind::Json, required);
}
Json readJson(const std::shared_ptr<AutoRigTaskContext>& context, const char* name) {
    if (!context->hasInput(name)) return boost::json::object{};
    auto text = core::settings::ngSettingsJsonString(context->input(name).json);
    return ngParsePatternJson(text);
}
void publishJson(const std::shared_ptr<AutoRigTaskContext>& context, const char* name, const Json& value) {
    context->publishJson(name, json::ngParseAutoRigJson(Text(boost::json::serialize(value))));
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
    return {
        {t("detect"), t("Detect structural targets"), {}, {blob("model"), jsonPort("selection", false)},
            {jsonPort("targets"), blob("model")}, {}},
        {t("deform"), t("Generate deformation controls"), {t("detect")},
            {blob("model"), jsonPort("targets"), jsonPort("recipe", false)},
            {blob("model"), jsonPort("receipt")}, {{t("model"), t("detect"), t("model")}, {t("targets"), t("detect"), t("targets")}}},
        {t("verify"), t("Verify generated model"), {t("deform")},
            {blob("before"), blob("after"), jsonPort("targets"), jsonPort("receipt")},
            {blob("model"), jsonPort("report"), jsonPort("targets")}, {{t("before"), t("detect"), t("model")}, {t("after"), t("deform"), t("model")},
                {t("targets"), t("detect"), t("targets")}, {t("receipt"), t("deform"), t("receipt")}}}
    };
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
    if (task.view() == "detect") {
        auto model = readModel(context, "model");
        publishJson(context, "targets", ngDetectPattern(pattern_, model.document, readJson(context, "selection"), checkpoint));
        context->publishBlob("model", context->input("model").readBlob());
    } else if (task.view() == "deform") {
        auto model = readModel(context, "model");
        auto result = ngDeformPattern(pattern_, model.document, readJson(context, "targets"), readJson(context, "recipe"), checkpoint);
        checkpoint();
        publishJson(context, "receipt", result.as_object().at("receipt"));
        publishModel(context, model, result.as_object().at("document"));
    } else if (task.view() == "verify") {
        auto before = readModel(context, "before"), after = readModel(context, "after");
        if (before.prefix != after.prefix || before.payload != after.payload)
            throw std::invalid_argument("Texture payload changed during structural rigging");
        auto report = ngVerifyPattern(pattern_, before.document, after.document, readJson(context, "targets"),
            readJson(context, "receipt"), checkpoint);
        checkpoint();
        publishJson(context, "report", report);
        publishJson(context, "targets", readJson(context, "targets"));
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
