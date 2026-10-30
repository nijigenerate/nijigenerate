// One-to-one port of tests/autorig_solver.d. No additional test cases.
#include "autorig_solver.hpp"
#include <autorig_pipeline.hpp>
#include <nijigenerate/autorig/solver/quadratic.hpp>
#include <nijigenerate/autorig/deterministic/face.hpp>
#include <nijigenerate/autorig/deterministic/processor.hpp>
#include <nijigenerate/autorig/deterministic/templates.hpp>
#include <nijigenerate/autorig/deterministic/geometry.hpp>
#include <nijigenerate/autorig/deterministic/linear.hpp>
#include <nijigenerate/autorig/deterministic/surface.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/deterministic/program.hpp>
#include <nijigenerate/autorig/deterministic/controls.hpp>
#include <nijigenerate/autorig/deterministic/observation.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/utf8.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace autorig_solver {
namespace {
using namespace nijigenerate;
using namespace autorig::deterministic;
namespace framework = autorig::framework;
namespace rig = autorig::deterministic;
using Json = core::settings::SettingsJsonValue;
using Array = core::settings::SettingsJsonArray;
using Text = compat::RetainedString;
using nicxlive::core::d_runtime::SourceAllocator;

template<std::size_t N> Text text(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
template<class Operation> auto allocateStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
// Keep Source new-expression arguments inside the actual allocated construction.
template<class Operation> struct DeferredText {
    Operation operation;
    operator Text() && { return operation(); }
};
Json object(std::initializer_list<std::pair<const char*, Json>> entries) {
    core::settings::SettingsJsonObject result;
    for (const auto& [key, value] : entries) result[core::settings::SettingsString(key)] = value;
    return Json(result);
}
Json array(std::initializer_list<Json> values) { return Json(Array(values)); }
Json get(Json value, const char* key) { return value.sourceIndexValue(key); }
void put(Json& value, const char* key, Json replacement) {
    value.sourceIndexAssignSlot(core::settings::SettingsString(key)) = std::move(replacement);
}
void line(std::string_view value) { providers::mcp::stdio::standardOutput().writeln(value); }

#ifndef NDEBUG
#define SOURCE_ASSERT(Expression, Line) do { if (!(Expression)) \
    throw compat::ngStaticError<compat::AssertError>(text("Assertion failure"), \
        text("tests/autorig_solver.d"), Line); } while (false)
#else
#define SOURCE_ASSERT(Expression, Line) ((void)0)
#endif

Json pointJson(contracts::Point2 point) { return array({point[0], point[1]}); }
Json numbersJson(compat::Slice<double> values) {
    Array result;
    for (const auto value : values) result.append(Json(value));
    return Json(result);
}
} // namespace

void ngMain(compat::Slice<Text> args) {
    if (args.size() == 5 && args[1].view() == "--audit-owned-state") {
        auto state = autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(args[2]));
        auto program = autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(args[3]));
        auto controls = rig::controls::ngRigCompileControls(state);
        auto corrections = rig::controls::ngRigCompileCheekCorrections(state, program);
        auto shoulders = observation::ngRigPrepareShoulders(state, program);
        auto output = compat::ngJsonStringify(object({{"controls", controls},
            {"corrections", corrections}, {"shoulders", shoulders}}));
        compat::phobos::ngFileWrite(args[4], output.view());
        line("Replayed original D compilers on owned imported-model state");
        return;
    }
    if (args.size() == 4 && args[1].view() == "--compile-observation") {
        auto source = autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(args[2]));
        auto observation = rig::evidence::ngRigClassifyMaterials(source, get(source, "options"));
        auto evidence = rig::evidence::ngRigDeriveEvidence(observation);
        auto program = rig::program::ngRigCompileProgram(observation, evidence);
        auto output = compat::ngJsonStringify(object({{"evidence", evidence}, {"program", program}}));
        compat::phobos::ngFileWrite(args[3], output.view());
        return;
    }
    if (args.size() == 3 && args[1].view() == "--classify-observation") {
        auto source = autorig::json::detail::parseJSON(compat::phobos::ngReadUtf8Text(args[2]));
        const auto started = compat::SourceMonotonicClock::now();
        auto result = rig::evidence::ngRigClassifyMaterials(source, Json(core::settings::SettingsJsonObject{}));
        std::size_t active = 0, automatic = 0;
        for (const auto material : get(result, "materials").sourceArrayValue()) {
            if (get(material, "static").as_bool()) continue;
            ++active;
            SOURCE_ASSERT(!get(material, "role").as_string().empty() &&
                material.sourceObjectNoRef().contains(core::settings::SettingsString("owner")), 57);
            if (get(material, "semantic_source").as_string().view() == "alpha_proximal_support_candidate")
                ++automatic;
        }
        const auto elapsed = (compat::SourceMonotonicClock::now() - started).count() / 10'000;
        providers::mcp::stdio::standardOutput().withTextWriter([&](auto& writer) {
            const auto number = [&](auto value) {
                char buffer[32];
                const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
                writer.put(std::string_view(buffer, converted.ptr));
            };
            writer.put("Classified ");
            number(active);
            writer.put(" active materials; alpha support fallback: ");
            number(automatic);
            writer.put("; elapsed ms: ");
            number(elapsed);
            writer.put(static_cast<unsigned char>('\n'));
        });
        return;
    }
    autorig_pipeline::ngTestRigPipeline();
    for (int index = 0; index < 1000; ++index) {
        auto value = Json(std::sin(index * .123) * 10000);
        SOURCE_ASSERT(autorig::json::ngParseAutoRigJson(compat::ngJsonStringify(value)).as_double() ==
            value.as_double(), 68);
    }
    auto exactJson = object({{"escaped\"key", array({-3147.01493669930278, -0.0, true, "1.25"})},
        {"empty", Json(Array{})}});
    SOURCE_ASSERT(contracts::ngRigDigest(autorig::json::ngParseAutoRigJson(compat::ngJsonStringify(exactJson))) ==
        contracts::ngRigDigest(exactJson), 72);
    templates::ngRigValidateEmbeddedTemplates();
    SOURCE_ASSERT(templates::ngRigTemplateNames().size() == 19, 74);
    auto embedded = templates::ngRigTemplate(text("face_head"));
    put(embedded, "id", Json("changed"));
    SOURCE_ASSERT(get(templates::ngRigTemplate(text("face_head")), "id").as_string().view() == "face_head", 77);
    for (const auto id : templates::ngRigTemplateNames()) {
        auto request = object({{"template", Json(core::settings::SettingsString(id.retainedString()))},
            {"bounds", array({0., 0., 100., 100.})}, {"sourceFit", false}});
        if (id.view() == "surface_layer") {
            auto definition = templates::ngRigTemplate(id);
            const auto count = get(get(definition, "guide_grid"), "rows").sourceArrayValue().size() *
                get(get(definition, "guide_grid"), "columns").sourceArrayValue().size();
            auto host = compat::Slice<double>(allocateStorage([&] {
                return std::vector<double>(count, std::numeric_limits<double>::quiet_NaN());
            }));
            for (auto& value : host) value = 0;
            put(request, "hostDepth", numbersJson(host));
        }
        auto surface = rig::surface::ngRigEvaluateTemplate(request);
        SOURCE_ASSERT(get(surface, "neutral") == get(surface, "projected"), 90);
        for (const auto z : get(surface, "depth").sourceArrayValue())
            SOURCE_ASSERT(std::isfinite(contracts::ngRigNumber(z)), 91);
        put(request, "pose", array({12., 8., 7.}));
        surface = rig::surface::ngRigEvaluateTemplate(request);
        SOURCE_ASSERT(get(get(surface, "posed_validation"), "local_orientation_preserved").as_bool(), 94);
    }
    auto solved = linear::ngRigLeastSquares({{1., 0.}, {0., 1.}, {1., 1.}}, {{2.}, {3.}, {5.}});
    SOURCE_ASSERT(std::abs(solved[0][0] - 2) < 1e-12 && std::abs(solved[1][0] - 3) < 1e-12, 97);
    compat::Slice<contracts::Point2> guideUV{{0., 0.}, {1., 0.}, {0., 1.}, {1., 1.}};
    compat::Slice<contracts::Point2> guideXY{{2., 3.}, {4., 3.}, {2., 6.}, {4., 6.}};
    auto guide = geometry::ngRigFitGuide(guideUV, {0., 0., 1., 1.}, guideUV, guideXY);
    for (std::size_t index = 0; index < guide.size(); ++index)
        for (std::size_t axis = 0; axis < 2; ++axis)
            SOURCE_ASSERT(std::abs(guide[index][axis] - guideXY[index][axis]) < 1e-12, 101);
    SOURCE_ASSERT(get(geometry::ngRigValidateGrid(guide, 2), "local_orientation_preserved").as_bool(), 102);
    auto midpoint = geometry::ngRigSampleGrid(guide, {0., 1.}, {0., 1.}, {{.5, .5}})[0];
    SOURCE_ASSERT(std::abs(midpoint[0] - 3) < 1e-12 && std::abs(midpoint[1] - 4.5) < 1e-12, 104);
    auto rotated = geometry::ngRigRotate({{1., 0., 0.}}, {0., 0., 0.}, {0., 0., 90.})[0];
    SOURCE_ASSERT(std::abs(rotated[0]) < 1e-12 && std::abs(rotated[1] - 1) < 1e-12, 106);
    using namespace autorig::solver::quadratic;
    QuadraticProblem problem;
    problem.variables = 1;
    problem.constraints = 1;
    problem.objective = {{0, 0, 2}};
    problem.linear = {-4.};
    problem.matrix = {{0, 0, 1}};
    problem.lower = {0.};
    problem.upper = {1.};
    auto result = ngSolveQuadratic(problem);
    SOURCE_ASSERT(result.solved() && std::abs(result.solution[0] - 1) < 1e-5 &&
        result.constraintViolation < 1e-5, 116);
    problem.objective = {};
    problem.linear = {-1.};
    result = ngSolveQuadratic(problem);
    SOURCE_ASSERT(result.solved() && std::abs(result.solution[0] - 1) < 1e-5, 120);
    problem.constraints = 2;
    problem.matrix = {{0, 0, 1}, {1, 0, 1}};
    problem.lower = {0., 2.};
    problem.upper = {1., 3.};
    result = ngSolveQuadratic(problem);
    SOURCE_ASSERT(!result.solved() && result.solution.empty(), 126);
    compat::Slice<double> xs{0., 1.}, ys{0., 1.};
    compat::Slice<face::RigPoint> uv{{0., 0.}, {1., 0.}, {0., 1.}, {1., 1.}};
    auto projection = face::ngPreserveFaceOrientation(uv, xs, ys, uv, {1., 0.}, 1);
    SOURCE_ASSERT(projection.maximumCorrection == 0 && projection.rawMinimum == 1, 131);
    compat::Slice<face::RigPoint> thin{{0., 0.}, {.04, 0.}, {0., 1.}, {.04, 1.}};
    projection = face::ngPreserveFaceOrientation(thin, xs, ys, uv, {1., 0.}, 1);
    SOURCE_ASSERT(projection.correctedMinimum >= .0549 && projection.maximumCorrection <= .04001, 134);
    auto fitted = face::ngFitGridDepth(xs, ys, {1., 1., 1., 1.}, uv,
        {1.01, 1.01, 1.01, 1.01}, .02, 4);
    SOURCE_ASSERT(fitted.size() == 4, 137);
    for (const auto value : fitted) SOURCE_ASSERT(std::abs(value - 1.01) < 1e-5, 138);
    auto canceled = [] { return true; };
    bool rejected = false;
    try { (void)ngSolveQuadratic(problem, SolverOptions{}, canceled); }
    catch (const compat::ThrowableException&) { rejected = true; }
    SOURCE_ASSERT(rejected, 143);
    auto applied = std::allocate_shared<bool>(SourceAllocator<bool>{}, false);
    auto processor = std::allocate_shared<rig::processor::AnimeFrontViewRigProcessor>(
        SourceAllocator<rig::processor::AnimeFrontViewRigProcessor>{},
        [applied](Json projection, Json target, std::shared_ptr<framework::AutoRigTaskContext>) {
            SOURCE_ASSERT(get(projection, "correctedMinimumJacobian").as_double() >= .0549, 147);
            SOURCE_ASSERT(get(target, "nodeId").as_int64() == 123, 148);
            *applied = true;
            return object({{"applied", true}});
        });
    auto sessions = std::allocate_shared<framework::AutoRigSessionManager>(
        SourceAllocator<framework::AutoRigSessionManager>{}, DeferredText{[] {
            const auto root = compat::phobos::ngTempDirectory();
            const auto uuid = framework::ngAutoRigRandomUuid();
            const auto suffix = allocateStorage([&] {
                return Text(std::string("autorig-solver-") + std::string(uuid.view()));
            });
            const std::array pathParts{root, suffix};
            return compat::phobos::ngBuildPath(pathParts);
        }});
    sessions->registerProcessor(processor);
    SOURCE_ASSERT(processor->workflows().empty(), 154);
    auto run = sessions->create(processor->procId());
    Array thinJson, uvJson;
    for (const auto point : thin) thinJson.append(pointJson(point));
    for (const auto point : uv) uvJson.append(pointJson(point));
    run->setInput(text("preserve-face-orientation"), text("request"), framework::AutoRigValue::jsonValue(
        object({{"xs", numbersJson(xs)}, {"ys", numbersJson(ys)}, {"points", Json(thinJson)},
            {"uv", Json(uvJson)}, {"depthDirection", array({1., 0.})}, {"width", 1.}})));
    run->execute(text("preserve-face-orientation"));
    run->setInput(text("apply-face-projection"), text("projection"),
        run->outputContext(text("preserve-face-orientation"))->value(text("result")));
    run->setInput(text("apply-face-projection"), text("target"),
        framework::AutoRigValue::jsonValue(object({{"nodeId", 123}})));
    run->execute(text("apply-face-projection"));
    SOURCE_ASSERT(*applied && get(run->outputContext(text("apply-face-projection"))->value(text("result")).json,
        "applied").as_bool(), 166);
    SOURCE_ASSERT(run->task(text("apply-face-projection")).state == framework::AutoRigTaskState::Succeeded, 167);
    SOURCE_ASSERT(!compat::phobos::ngFileExists(run->directory().view()), 169);
    sessions->close(run->id());
    auto restored = sessions->reopenWithProcessor(processor, run->id());
    restored->restoreCommittedOutputs();
    SOURCE_ASSERT(restored->task(text("apply-face-projection")).state == framework::AutoRigTaskState::Succeeded, 173);
    SOURCE_ASSERT(get(restored->outputContext(text("apply-face-projection"))->value(text("result")).json,
        "applied").as_bool(), 174);
    SOURCE_ASSERT(!compat::phobos::ngFileExists(run->directory().view()), 175);
    auto restartedSessions = std::allocate_shared<framework::AutoRigSessionManager>(
        SourceAllocator<framework::AutoRigSessionManager>{},
        DeferredText{[sessions] { return sessions->rootDirectory(); }});
    bool rejectedRestart = false;
    try { (void)restartedSessions->reopenWithProcessor(processor, run->id()); }
    catch (const compat::ThrowableException&) { rejectedRestart = true; }
    SOURCE_ASSERT(rejectedRestart, 180);
    line("AutoRig OSQP and face projection checks passed");
}
#undef SOURCE_ASSERT
} // namespace autorig_solver
