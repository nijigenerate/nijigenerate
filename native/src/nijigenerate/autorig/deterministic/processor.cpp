#include "processor.hpp"
#include "anatomy.hpp"
#include "face.hpp"
#include "surface.hpp"
#include "evidence.hpp"
#include "program.hpp"
#include "storage.hpp"
#include "review.hpp"
#include "templates.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <chrono>
#include <cmath>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::autorig::deterministic::processor {
using namespace framework;
using namespace contracts;
using Array = core::settings::SettingsJsonArray;
using Object = core::settings::SettingsJsonObject;
namespace {
template<std::size_t N> Text text(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<std::size_t N> Text copiedMessage(const char (&message)[N]) {
    return contracts::detail::copyContractMessage(std::string_view(message, N - 1));
}
template<std::size_t N> void require(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(copiedMessage(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/processor.d"), line);
}
template<std::size_t N> core::settings::SettingsString key(const char (&value)[N]) {
    return core::settings::SettingsString(text(value));
}
struct Field {
    core::settings::SettingsString name;
    Json value;
    template<std::size_t N> Field(const char (&name)[N], Json value)
        : name(key(name)), value(std::move(value)) {}
};
Json object(std::initializer_list<Field> fields) {
    Object result;
    for (const auto& field : fields) result[field.name] = field.value;
    return result;
}
double number(Json value) {
    require(value.is_double() || value.is_int64() || value.is_uint64(), "Expected a numeric value", 20);
    const double result = value.is_double() ? value.as_double() : value.is_int64()
        ? static_cast<double>(value.as_int64()) : static_cast<double>(value.as_uint64());
    require(std::isfinite(result), "Expected a finite number", 24);
    return result;
}
compat::Slice<double> numbers(Json value) {
    compat::Slice<double> result;
    for (auto item : value.sourceArrayValue()) result.append(number(item));
    return result;
}
compat::Slice<Point2> points(Json value) {
    compat::Slice<Point2> result;
    for (auto item : value.sourceArrayValue()) {
        auto coordinates = numbers(item);
        require(coordinates.size() == 2, "Expected a two-dimensional point", 38);
        result.append(Point2{coordinates[0], coordinates[1]});
    }
    return result;
}
Json numbersJson(compat::Slice<double> values) {
    Array result;
    for (double value : values) result.append(Json(value));
    return result;
}
Json pointsJson(compat::Slice<Point2> values) {
    Array result;
    for (auto value : values) result.append(Json(Array{Json(value[0]), Json(value[1])}));
    return result;
}
template<std::size_t N> AutoRigPortSpec port(const char (&id)[N]) { return {text(id), AutoRigValueKind::Json}; }
}
AnimeFrontViewRigProcessor::AnimeFrontViewRigProcessor(Projection projection, pipeline::RigNativeStage stage)
    : applyProjection_(std::move(projection)), nativeStage_(std::move(stage)) {
    prepareSourceClassInfo();
}
void AnimeFrontViewRigProcessor::prepareSourceClassInfo() {
    static const bool registered = [] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AnimeFrontViewRigProcessor),
                Text::fromStaticLiteral("nijigenerate.autorig.deterministic.processor.AnimeFrontViewRigProcessor"));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
Text AnimeFrontViewRigProcessor::procId() { return Text::fromStaticLiteral("anime-front-view-rig"); }
Text AnimeFrontViewRigProcessor::displayName() {
    const auto receiver = weak_from_this().lock();
    return ngAutoRigMessage(compat::RetainedStringSlice(
        Text::fromStaticLiteral("Anime Front View Rig"))).retainedString();
}
compat::Slice<AutoRigTaskSpec> AnimeFrontViewRigProcessor::tasks() {
    const auto receiver = weak_from_this().lock();
    compat::Slice<AutoRigTaskSpec> result;
    // Evaluate every translated value before iterating the source AA descriptor.
    Object entries;
    entries[key("preserve-face-orientation")] =
        Json(core::settings::SettingsString(ngAutoRigMessage(text("Preserve face orientation"))));
    entries[key("fit-grid-depth")] =
        Json(core::settings::SettingsString(ngAutoRigMessage(text("Fit grid depth"))));
    entries[key("solve-scaffold")] =
        Json(core::settings::SettingsString(ngAutoRigMessage(text("Solve anatomical scaffold"))));
    entries[key("sample-anatomical-depth")] =
        Json(core::settings::SettingsString(ngAutoRigMessage(text("Sample anatomical depth"))));
    entries[key("evaluate-template")] =
        Json(core::settings::SettingsString(ngAutoRigMessage(text("Evaluate surface template"))));
    for (const auto& entry : entries)
        result.append(AutoRigTaskSpec{entry.name.retainedString(), entry.item.as_string().retainedString(), {},
            {port("request")}, {port("result")}, {}});
    if (applyProjection_)
        result.append(AutoRigTaskSpec{text("apply-face-projection"), ngAutoRigMessage(text("Apply face projection")),
            {}, {port("projection"), port("target")}, {port("result")}, {}});
    if (nativeStage_) {
        auto entries = pipeline::ngRigPipelineTasks();
        for (auto& entry : entries) entry.reclaimScratchMemory = true;
        result.append(entries);
    }
    return result;
}
compat::Slice<AutoRigWorkflowSpec> AnimeFrontViewRigProcessor::workflows() {
    const auto receiver = weak_from_this().lock();
    if (!nativeStage_) return {};
    compat::Slice<AutoRigWorkflowSpec> result;
    result.resize(1);
    result[0] = pipeline::ngRigModelWorkflow(procId());
    return result;
}
void AnimeFrontViewRigProcessor::executeTask(Text taskId, std::shared_ptr<AutoRigTaskContext> context) {
    const auto receiverFrame = weak_from_this().lock();
    if (taskId == "observe-model") {
        require(bool(nativeStage_), "Model observation is unavailable", 92);
        auto stage = nativeStage_;
        auto options = compat::ngRequireObject(context)->input("options").json;
        auto state = stage(taskId, options, Json{}, {}, context);
        Array materials;
        for (auto material : state.sourceIndexValue("materials").sourceArrayValue())
            materials.append(object({{"name", material.sourceIndexValue("name")}, {"path", material.sourceIndexValue("path")},
                {"active", material.sourceIndexValue("active")}}));
        compat::ngRequireObject(context)->publishJson("materials", Json(materials));
        compat::ngRequireObject(context)->publishJson("review", review::ngRigClassificationReview(state, Json{}));
        auto store = storage::ngRigStateStorage(context);
        auto snapshot = compat::ngRequireObject(store)->snapshot(state, true);
        compat::ngRequireObject(context)->publishJson("observation", compat::ngRequireObject(store)->artifact(snapshot, "materials"));
        compat::ngRequireObject(context)->publishJson("state", snapshot);
        return;
    }
    if (taskId == "compile-rig") {
        const auto start = compat::SourceMonotonicClock::now();
        const auto phase = [&](Text name) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                compat::SourceMonotonicClock::now() - start).count();
            compat::ngRequireObject(context)->previewJson("compile-profile", object({{"phase", Json(core::settings::SettingsString(name))},
                {"elapsed_ms", Json(static_cast<std::int64_t>(elapsed))}}));
        };
        phase(text("restore-input"));
        auto store = storage::ngRigStateStorage(context);
        auto state = compat::ngRequireObject(store)->restore(compat::ngRequireObject(context)->input("state").json);
        phase(text("classify-materials"));
        auto observation = evidence::ngRigClassifyMaterials(state, compat::ngRequireObject(context)->input("options").json, context);
        ngRigCheckpoint(context);
        phase(text("derive-evidence"));
        auto measured = evidence::ngRigDeriveEvidence(observation);
        phase(text("compile-program"));
        auto compiled = program::ngRigCompileProgram(observation, measured);
        compat::ngRequireObject(context)->publishJson("review", review::ngRigClassificationReview(observation, compiled));
        state = observation; state.sourceIndexAssignSlot(key("evidence")) = measured;
        phase(text("retain-artifacts"));
        compat::ngRequireObject(context)->publishJson("evidence", measured);
        compat::ngRequireObject(context)->publishJson("program", compiled);
        compat::ngRequireObject(context)->publishJson("state", compat::ngRequireObject(store)->snapshot(state));
        phase(text("completed"));
        return;
    }
    for (auto entry : pipeline::ngRigPipelineTasks()) if (entry.id.view() == taskId.view()) {
        require(bool(nativeStage_), "Native rig application is unavailable", 130);
        auto store = storage::ngRigStateStorage(context);
        auto state = compat::ngRequireObject(store)->restore(compat::ngRequireObject(context)->input("state").json);
        Json result;
        try {
            // Retain the immutable owner throughout the native-stage call.
            auto stage = nativeStage_;
            auto programInput = compat::ngRequireObject(context)->input("program").json;
            auto model = compat::ngRequireObject(context)->input("model").borrowTaskBlob();
            result = stage(taskId, state, programInput, model, context);
            if (taskId == "compile-domain-layout") {
                auto layout = program::ngRigCompileProgram(result, result.sourceIndexValue("evidence"));
                compat::ngRequireObject(context)->publishJson("program", layout);
                compat::ngRequireObject(context)->previewJson("review-layout", review::ngRigClassificationReview(result, layout));
            }
            if (entry.retainFailureOutputs && taskId != "verify-saved-rig") {
                auto attempts = ngRigGet(state, text("finish_stages"), Json(Array{})).sourceArrayValue().duplicate();
                attempts.append(object({{"stage", Json(taskId)}, {"succeeded", Json(true)}}));
                result.sourceIndexAssignSlot(key("finish_stages")) = Json(attempts);
            }
        } catch (const compat::ThrowableException& error) {
            if (!entry.retainFailureOutputs || compat::ngRequireObject(context)->isCanceled()) throw;
            auto attempts = ngRigGet(state, text("finish_stages"), Json(Array{})).sourceArrayValue().duplicate();
            attempts.append(object({{"stage", Json(taskId)}, {"succeeded", Json(false)}, {"error", Json(core::settings::SettingsString(error.msg))}}));
            if (taskId == "verify-saved-rig")
                result = object({{"passed", Json(false)}, {"finish_stages", Json(attempts)},
                    {"visual_review_required", Json(true)}, {"error", Json(core::settings::SettingsString(error.msg))},
                    {"source_sha256", state.sourceIndexValue("source_sha256")}});
            else {
                result = state; result.sourceIndexAssignSlot(key("finish_stages")) = Json(attempts);
                compat::ngRequireObject(context)->publishBlob("model", compat::ngRequireObject(context)->input("model").readBlob());
            }
            compat::ngRequireObject(context)->reportFailure(error.msg);
        }
        if (taskId == "verify-saved-rig") {
            auto attempts = ngRigGet(result, text("finish_stages"),
                ngRigGet(state, text("finish_stages"), Json(Array{}))).sourceArrayValue().duplicate();
            bool recorded = false;
            const bool verified = ngRigGet(result, text("readback_verified"), result.sourceIndexValue("passed")).as_bool();
            for (auto attempt : attempts)
                if (attempt.sourceIndexValue("stage").as_string().view() == taskId.view()) recorded = true;
            if (!recorded) attempts.append(object({{"stage", Json(taskId)}, {"succeeded", Json(verified)}}));
            result.sourceIndexAssignSlot(key("finish_stages")) = Json(attempts);
            bool complete = verified;
            for (auto attempt : attempts) complete = complete && attempt.sourceIndexValue("succeeded").as_bool();
            result.sourceIndexAssignSlot(key("all_finish_stages_succeeded")) = Json(complete);
            result.sourceIndexAssignSlot(key("numerical_stages_passed")) = Json(complete && result.sourceIndexValue("passed").as_bool());
            result.sourceIndexAssignSlot(key("rig_complete")) = Json(false);
            if (!complete) compat::ngRequireObject(context)->reportFailure("One or more finishing stages failed; inspect the completion report");
        }
        compat::ngRequireObject(context)->publishJson(taskId == "verify-saved-rig" ? "report" : "state",
            taskId == "verify-saved-rig" ? result : compat::ngRequireObject(store)->snapshot(result));
        return;
    }
    if (taskId == "apply-face-projection") {
        require(bool(applyProjection_), "Editor application is unavailable", 182);
        auto receiver = compat::ngRequireObject(context);
        auto projection = applyProjection_;
        auto projectionInput = compat::ngRequireObject(context)->input("projection").json;
        auto targetInput = compat::ngRequireObject(context)->input("target").json;
        receiver->publishJson("result", projection(projectionInput, targetInput, context));
        return;
    }
    auto request = compat::ngRequireObject(context)->input("request").json;
    if (taskId == "evaluate-template") {
        ngRigCheckpoint(context);
        auto result = surface::ngRigEvaluateTemplate(request);
        ngRigCheckpoint(context); compat::ngRequireObject(context)->publishJson("result", result); return;
    }
    if (taskId == "solve-scaffold" || taskId == "sample-anatomical-depth") {
        ngRigCheckpoint(context);
        auto prior = templates::ngRigHumanoidPrior();
        Json result;
        if (taskId == "solve-scaffold") {
            result = anatomy::ngRigSolveScaffold(request.sourceIndexValue("evidence"), prior);
        } else {
            auto query = ngRigPoints(request.sourceIndexValue("query"));
            auto domain = request.sourceIndexValue("domain");
            auto scaffold = request.sourceIndexValue("scaffold");
            result = numbersJson(anatomy::ngRigDepthField(query, domain, scaffold, prior));
        }
        ngRigCheckpoint(context); compat::ngRequireObject(context)->publishJson("result", result); return;
    }
    auto xs = numbers(request.sourceIndexValue("xs")), ys = numbers(request.sourceIndexValue("ys"));
    auto canceled = [context] { return compat::ngRequireObject(context)->isCanceled(); };
    Object output;
    if (taskId == "preserve-face-orientation") {
        auto direction = numbers(request.sourceIndexValue("depthDirection"));
        require(direction.size() == 2, "Expected a two-dimensional depth direction", 210);
        compat::Slice<bool> protectedVertices;
        const auto& requestObject = request.sourceObjectRef();
        if (auto protection = requestObject.if_contains("protected"))
            for (auto item : protection->sourceArrayValue()) protectedVertices.append(item.as_bool());
        auto sourcePoints = points(request.sourceIndexValue("points"));
        auto sourceUv = points(request.sourceIndexValue("uv"));
        Point2 depthDirection{direction[0], direction[1]};
        auto width = number(request.sourceIndexValue("width"));
        auto projection = face::ngPreserveFaceOrientation(sourcePoints, xs, ys,
            sourceUv, depthDirection, width,
            protectedVertices, canceled);
        output[key("points")] = pointsJson(projection.points);
        output[key("rawMinimumJacobian")] = projection.rawMinimum;
        output[key("correctedMinimumJacobian")] = projection.correctedMinimum;
        output[key("maximumCorrection")] = projection.maximumCorrection;
    } else if (taskId == "fit-grid-depth") {
        const double count = number(request.sourceIndexValue("anchorCount"));
        // Check the exact representable domain before converting to avoid C++ UB.
        require(count >= 0 && count < std::ldexp(1., std::numeric_limits<std::size_t>::digits)
            && count == std::trunc(count), "Invalid anchor count", 223);
        auto base = numbers(request.sourceIndexValue("base"));
        auto query = points(request.sourceIndexValue("query"));
        auto truth = numbers(request.sourceIndexValue("truth"));
        auto tolerance = number(request.sourceIndexValue("tolerance"));
        auto fitted = face::ngFitGridDepth(xs, ys, base, query,
            truth, tolerance, static_cast<std::size_t>(count), canceled);
        output[key("passed")] = fitted.size() != 0;
        output[key("depth")] = numbersJson(fitted);
    } else throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Unknown deterministic rig task"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/processor.d"), 228);
    require(!compat::ngRequireObject(context)->isCanceled(), "Deterministic rig task canceled", 229);
    compat::ngRequireObject(context)->publishJson("result", Json(output));
}
}
