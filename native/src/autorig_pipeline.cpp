#include "autorig_pipeline.hpp"
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/deterministic/program.hpp>
#include <nijigenerate/autorig/deterministic/controls.hpp>
#include <nijigenerate/autorig/deterministic/observation.hpp>
#include <nijigenerate/autorig/deterministic/geometry.hpp>
#include <nijigenerate/autorig/deterministic/processor.hpp>
#include <nijigenerate/autorig/deterministic/review.hpp>
#include <nijigenerate/autorig/deterministic/presentation.hpp>
#include <nijigenerate/autorig/deterministic/physics.hpp>
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/autorig/workflow.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <array>
#include <concepts>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <cmath>

namespace autorig_pipeline {
namespace {
using Json = nijigenerate::core::settings::SettingsJsonValue;
using Array = nijigenerate::core::settings::SettingsJsonArray;
using Object = nijigenerate::core::settings::SettingsJsonObject;
using Text = nijigenerate::core::settings::SettingsString;
using namespace nijigenerate::autorig::deterministic::contracts;
using namespace nijigenerate::autorig::deterministic::evidence;
using namespace nijigenerate::autorig::deterministic::program;
using namespace nijigenerate::autorig::deterministic::review;
using namespace nijigenerate::autorig::deterministic::presentation;
nijigenerate::compat::RetainedStringSlice text(const char* literal) {
    // Every pointer passed here denotes a literal in this private module.
    return nijigenerate::compat::RetainedString::fromBorrowedView(std::string_view(literal));
}
nijigenerate::compat::RetainedStringSlice text(nijigenerate::compat::RetainedStringSlice value) { return value; }
nijigenerate::compat::RetainedString retained(const char* literal) { return text(literal).retainedString(); }
struct LiteralJson {
    Json value;
    LiteralJson(Json input) : value(input) {}
    LiteralJson(const char* literal) : value(Text(text(literal))) {}
    template<class Number> requires std::is_arithmetic_v<Number>
    LiteralJson(Number input) : value(input) {}
};
Json object(std::initializer_list<std::pair<const char*, LiteralJson>> entries) {
    Object result;
    for (const auto& entry : entries) result[Text(text(entry.first))] = entry.second.value;
    return Json(result);
}
Json array(std::initializer_list<LiteralJson> entries) {
    Array result; result.resize(entries.size());
    std::size_t index = 0; for (const auto& entry : entries) result[index++] = entry.value;
    return Json(result);
}
using LiteralStrings = nijigenerate::compat::Slice<nijigenerate::compat::RetainedStringSlice>;
LiteralStrings strings(std::initializer_list<const char*> values) {
    LiteralStrings result; result.resize(values.size());
    std::size_t index = 0; for (auto value : values) result[index++] = text(value);
    return result;
}
template<class Role, class Semantic> bool materialForcedStatic(bool active, Role role, Semantic semantic) {
    return nijigenerate::autorig::deterministic::review::ngRigMaterialForcedStatic(active,text(role),text(semantic));
}
auto roleCandidates(const char* name) {
    return nijigenerate::autorig::deterministic::evidence::ngRigMaterialRoleCandidates(text(name));
}
template<class Role, class Feature, class Side>
Json editedOverride(Json previous, std::uint8_t fields, Role role, bool stationary, Feature feature, Side side) {
    return nijigenerate::autorig::deterministic::review::ngRigEditedMaterialOverride(previous,fields,
        text(role),stationary,text(feature),text(side));
}
auto reviewOperationId(const char* kind, Json operation, Text name) {
    return nijigenerate::autorig::deterministic::review::ngRigReviewOperationId(text(kind),operation,name);
}
nijigenerate::compat::RetainedStringSlice reviewName(RigReviewMaterialNames names, std::uint64_t key) {
    if (names) {
        const auto found = names->find(key);
        if (found != names->end()) return found->second;
    }
    throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
        retained("tests/autorig_pipeline.d"),490);
}
Json copyObject(Json value) { return Json(value.sourceObjectNoRef().duplicate()); }
nijigenerate::compat::RetainedString sourceWorkspace(const char* prefix) {
    const auto temporary = nijigenerate::compat::phobos::ngTempDirectory();
    const auto uuid = nijigenerate::autorig::framework::ngAutoRigRandomUuid();
    const auto name = nijigenerate::autorig::framework::operator+(text(prefix).retainedString(), uuid);
    const std::array<nijigenerate::compat::RetainedString, 2> segments{temporary, name};
    return nijigenerate::compat::phobos::ngBuildPath(segments);
}
Json roundTrip(Json value) {
    const auto text = nijigenerate::compat::ngJsonStringify(value);
    return nijigenerate::autorig::json::detail::parseJSON(nijigenerate::compat::RetainedStringSlice(text));
}
Json get(Json value, const char* key) { return value.sourceIndexValue(key); }
Json get(Json value, std::size_t index) { return value.sourceIndexValue(index); }
template<std::integral Integer> Json get(Json value, Integer index) {
    return value.sourceIndexValue(static_cast<std::size_t>(index));
}
void put(Json& value, const char* key, Json replacement) {
    value.sourceIndexAssignSlot(Text(text(key))) = replacement;
}
void put(Json& value, const char* key, const char* replacement) { put(value,key,Json(Text(text(replacement)))); }
bool contains(Json value, const char* key) { return value.sourceObjectNoRef().if_contains(key) != nullptr; }
std::size_t length(Json value) { return value.sourceArrayValue().size(); }
template<class Key, class... Keys> Json read(Json value, Key key, Keys... keys) {
    if constexpr (sizeof...(keys) == 0) return get(value, key);
    else return read(get(value, key), keys...);
}
Json last(Json value) { auto entries = value.sourceArrayValue(); return entries[entries.size() - 1]; }
template<class Left, class Right> bool equal(const Left& left, const Right& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if constexpr (requires { left[i] == right[i]; }) {
            if (!(left[i] == right[i])) return false;
        } else if (!equal(left[i], right[i])) return false;
    }
    return true;
}
#ifndef NDEBUG
#define NG_ORIGINAL_ASSERT(Expression, Line) do { if (!(Expression)) \
    throw nijigenerate::compat::ngStaticError<nijigenerate::compat::AssertError>( \
        "Assertion failure", "tests/autorig_pipeline.d", Line); } while (false)
#else
#define NG_ORIGINAL_ASSERT(Expression, Line) ((void)0)
#endif
void testReviewPlan() {
    auto key = object({{"offsets", array({0., 1.})}, {"key", array({0, 0})}});
    auto operation = object({{"part", 41}, {"keys", array({key})}});
    auto plan = object({{"mechanisms", array({object({{"name", "Eye::Blink"},
        {"axisX", array({0., 1.})}, {"axisY", array({0., 1.})}, {"operations", array({operation})}})})},
        {"masks", array({object({{"part", 41}, {"source", 42}})})},
        {"draw_order", array({object({{"part", 41}, {"relative_zsort", 2.}})})}});
    Json operations;
    auto filtered = ngRigReviewControls(plan,
        object({{"disabled", array({"control:Eye::Blink:41", "mask:41:42"})}}), operations);
    NG_ORIGINAL_ASSERT(length(get(filtered, "mechanisms")) == 0 && length(get(filtered, "masks")) == 0, 58);
    NG_ORIGINAL_ASSERT(length(get(filtered, "draw_order")) == 1, 59);
    NG_ORIGINAL_ASSERT(length(get(plan, "mechanisms")) == 1 && length(get(plan, "masks")) == 1, 60);
    NG_ORIGINAL_ASSERT(length(operations) == 4 && !get(get(operations, 1), "enabled").as_bool(), 61);
    NG_ORIGINAL_ASSERT(!contains(get(get(operations, 1), "plan"), "offsets"), 62);
    filtered = ngRigReviewControls(plan, Json{}, operations);
    NG_ORIGINAL_ASSERT(filtered == plan, 64);
    auto changes = ngRigReviewChanges(array({object({{"id", "node:1"}, {"name", "old"}})}),
        array({object({{"id", "node:1"}, {"name", "new"}}), object({{"id", "node:2"}, {"name", "added"}})}));
    NG_ORIGINAL_ASSERT(length(changes) == 2 && get(get(changes, 0), "change").as_string() == text("modified") &&
        get(get(get(changes, 0), "before"), "name").as_string() == text("old") &&
        get(get(changes, 1), "change").as_string() == text("added"), 68);
    auto removed = ngRigReviewChanges(array({object({{"id", "node:1"}})}), Json(Array{}));
    NG_ORIGINAL_ASSERT(get(get(removed, 0), "change").as_string() == text("removed"), 72);
    auto tables = ngRigReviewTables(object({{"changes", changes}}));
    NG_ORIGINAL_ASSERT(tables.size() == 1 && tables[0].columns.size() == 5, 75);
    const auto changesRow = strings({"new", "Modified", "Name", "old", "new"});
    NG_ORIGINAL_ASSERT(equal(tables[0].rows[0], changesRow), 76);
    NG_ORIGINAL_ASSERT(tables[0].rows[1][0] == text("added") && tables[0].rows[1][1] == text("Added"), 77);
    auto classification = object({{"classification", array({object({{"uuid", 41}, {"name", "Eye white R"},
        {"active", true}, {"static", false}, {"role", "face_feature"}, {"feature", "sclera"},
        {"semantic_source", "imported_clipping_receiver"}, {"content_sha256", "internal-secret"}})})}});
    tables = ngRigReviewTables(classification);
    const auto classificationRow = strings({
        "Eye white R", "Facial feature", "Eye white", "Enabled", "Inherited from clipping receiver"});
    NG_ORIGINAL_ASSERT(equal(tables[0].rows[0], classificationRow), 83);
    for (auto row : tables[0].rows) for (auto cell : row) NG_ORIGINAL_ASSERT(cell != text("internal-secret"), 85);
    auto bones = object({{"scaffold", object({{"bones", array({object({{"id", "Head"}, {"parent", "Neck"},
        {"head", array({0., 1., 2.})}, {"tail", array({3., 4., 5.})}, {"rest_roll", 0.}})})}})}});
    tables = ngRigReviewTables(bones);
    NG_ORIGINAL_ASSERT(tables[0].rows[0][0] == text("Head") && tables[0].rows[0][1] == text("Neck"), 90);
    NG_ORIGINAL_ASSERT(tables[0].rows[0][2] == text("0.00, 1.00, 2.00"), 91);
    auto previousWelding = object({{"target", 41}, {"target_name", "Eye white R"}, {"paired_vertices", 3},
        {"weight", .5}, {"indices_sha256", "previous"}});
    auto updatedWelding = copyObject(previousWelding);
    put(updatedWelding, "indices_sha256", "updated");
    auto weldingChanges = ngRigReviewChanges(array({object({{"id", "node:1"}, {"name", "old"},
        {"welding", array({previousWelding})}})}), array({object({{"id", "node:1"}, {"name", "old"},
        {"welding", array({updatedWelding})}})}));
    tables = ngRigReviewTables(object({{"changes", weldingChanges}}));
    NG_ORIGINAL_ASSERT(tables[0].rows[0][3] != tables[0].rows[0][4], 102);
    NG_ORIGINAL_ASSERT(tables[0].rows[0][4] == text("Eye white R / 3 Paired vertices / Weight 0.50 / Correspondence updated"), 103);
    auto previousBinding = object({{"target", 41}, {"target_name", "Eye white R"}, {"property", "opacity"},
        {"authored_keys", 3}, {"maximum_offset", 1.}, {"content_sha256", "previous"}});
    auto updatedBinding = copyObject(previousBinding);
    put(updatedBinding, "content_sha256", "updated");
    auto bindingChanges = ngRigReviewChanges(array({object({{"id", "parameter:1"}, {"name", "Opacity"},
        {"bindings", array({previousBinding})}})}), array({object({{"id", "parameter:1"}, {"name", "Opacity"},
        {"bindings", array({updatedBinding})}})}));
    tables = ngRigReviewTables(object({{"changes", bindingChanges}}));
    NG_ORIGINAL_ASSERT(tables[0].rows[0][3] != tables[0].rows[0][4], 115);
    NG_ORIGINAL_ASSERT(tables[0].rows[0][4] == text("Eye white R / opacity / 3 keys / 1.00 / Updated values"), 116);
    NG_ORIGINAL_ASSERT(!materialForcedStatic(true, "torso", ""), 117);
    NG_ORIGINAL_ASSERT(materialForcedStatic(true, "background", ""), 118);
    NG_ORIGINAL_ASSERT(materialForcedStatic(false, "torso", ""), 119);
    NG_ORIGINAL_ASSERT(materialForcedStatic(true, "ear", "full_body_backdrop_alpha_perimeter"), 120);
    NG_ORIGINAL_ASSERT(!materialForcedStatic(true, "torso", "full_body_backdrop_alpha_perimeter"), 121);
    auto material = object({{"uuid", 41}, {"name", "test"}, {"path", "/test"},
        {"support_candidates", array({previousBinding, updatedBinding})}});
    auto compact = ngRigClassificationReview(object({{"materials", array({material})}}), Json{});
    NG_ORIGINAL_ASSERT(!contains(get(get(compact, "classification"), 0), "support_candidates"), 125);
    NG_ORIGINAL_ASSERT(length(get(material, "support_candidates")) == 2, 126);
    auto observation = ngTestModelObservation();
    auto& face = get(observation, "materials").as_array()[0];
    put(face, "name", "background"); put(face, "path", "/background");
    auto facePath = get(face, "path").as_string();
    Json backgroundOverride(Object{});
    Object overrides; overrides[facePath] = backgroundOverride;
    auto classified = ngRigClassifyMaterials(observation, object({{"materials", Json(overrides)}}));
    auto background = get(get(classified, "materials"), 0);
    NG_ORIGINAL_ASSERT(get(background, "static").as_bool() && !ngRigMaterialStaticPreference(background), 135);
    auto faceOverride = editedOverride(backgroundOverride,
        static_cast<std::uint8_t>(RigMaterialField::role), "face", false, "", "");
    overrides = Object{}; overrides[facePath] = faceOverride;
    classified = ngRigClassifyMaterials(observation, object({{"materials", Json(overrides)}}));
    NG_ORIGINAL_ASSERT(!get(get(get(classified, "materials"), 0), "static").as_bool(), 139);
    faceOverride = editedOverride(faceOverride,
        static_cast<std::uint8_t>(RigMaterialField::stationary), "face", true, "", "");
    overrides = Object{}; overrides[facePath] = faceOverride;
    classified = ngRigClassifyMaterials(observation, object({{"materials", Json(overrides)}}));
    NG_ORIGINAL_ASSERT(get(get(get(classified, "materials"), 0), "static").as_bool(), 143);
    NG_ORIGINAL_ASSERT(ngRigMaterialStaticPreference(get(get(classified, "materials"), 0)), 144);
    observation = ngTestModelObservation();
    nijigenerate::compat::Slice<Point2> backdropCloud{{-100., 0.}, {100., 0.}, {100., 160.}, {-100., 160.}};
    const auto previousMaterials = get(observation, "materials").sourceArrayValue();
    const auto backdropMaterial = object({{"uuid", 19}, {"name", "ear_large"}, {"path", "/ear_large"},
        {"active", true}, {"cloud", ngRigPointsJson(backdropCloud)}, {"opaque_perimeter_coverage", 1.}});
    Array materialArray; materialArray.resize(previousMaterials.size()+1);
    for (std::size_t i=0; i<previousMaterials.size(); ++i) materialArray[i] = previousMaterials[i];
    materialArray[previousMaterials.size()] = backdropMaterial;
    put(observation, "materials", Json(materialArray));
    classified = ngRigClassifyMaterials(observation, get(observation, "options"));
    auto backdrop = last(get(classified, "materials"));
    NG_ORIGINAL_ASSERT(get(backdrop, "static").as_bool() &&
        get(backdrop, "semantic_source").as_string() == text("full_body_backdrop_alpha_perimeter"), 153);
    NG_ORIGINAL_ASSERT(!ngRigMaterialStaticPreference(backdrop), 154);
    for (const auto role : {"face", "ear"}) {
        auto overrideRole = editedOverride(Json{}, static_cast<std::uint8_t>(RigMaterialField::role),
            role, false, "", "");
        classified = ngRigClassifyMaterials(observation,
            object({{"materials", object({{"/ear_large", overrideRole}})}}));
        auto actualStatic = get(last(get(classified, "materials")), "static").as_bool();
        NG_ORIGINAL_ASSERT(actualStatic == materialForcedStatic(true, role,
            get(backdrop, "semantic_source").as_string()), 160);
    }
}
}

Json ngTestModelObservation() {
    Array materials;
    const auto rectangle = [&](const char* name, nijigenerate::compat::FixedArray<double, 4> bounds) {
        nijigenerate::compat::Slice<Point2> points;
        for (int y = static_cast<int>(bounds[1]); y < static_cast<int>(bounds[3]) + 1; ++y)
            for (int x = static_cast<int>(bounds[0]); x < static_cast<int>(bounds[2]) + 1; ++x)
                points.append(Point2(static_cast<double>(x), static_cast<double>(y)));
        const auto path = nijigenerate::autorig::deterministic::contracts::detail::allocateContractStorage([&] {
            return Text(std::string("/") + name);
        });
        materials.append(object({{"name", Json(Text(text(name)))}, {"path", Json(path)},
            {"uuid", Json(static_cast<std::uint64_t>(materials.size() + 1))}, {"active", Json(true)},
            {"bounds", array({Json(bounds[0]), Json(bounds[1]), Json(bounds[2]), Json(bounds[3])})},
            {"cloud", ngRigPointsJson(points)}}));
    };
    using Bounds = nijigenerate::compat::FixedArray<double, 4>;
    rectangle("face", Bounds(-12., 0., 12., 20.));
    rectangle("neck", Bounds(-3., 19., 3., 31.));
    rectangle("torso", Bounds(-15., 30., 15., 65.));
    rectangle("arm_r", Bounds(-30., 29., -20., 65.));
    rectangle("hand_r", Bounds(-30., 65., -20., 76.));
    rectangle("arm_l", Bounds(20., 29., 30., 65.));
    rectangle("hand_l", Bounds(20., 65., 30., 76.));
    rectangle("leg_r", Bounds(-12., 65., -4., 105.));
    rectangle("shoe_r", Bounds(-12., 105., -3., 112.));
    rectangle("leg_l", Bounds(4., 65., 12., 105.));
    rectangle("shoe_l", Bounds(3., 105., 12., 112.));
    rectangle("sclera_r", Bounds(-9., 7., -3., 11.));
    rectangle("sclera_l", Bounds(3., 7., 9., 11.));
    rectangle("iris_r", Bounds(-7., 7., -5., 11.));
    rectangle("iris_l", Bounds(5., 7., 7., 11.));
    rectangle("mouth", Bounds(-4., 14., 4., 16.));
    rectangle("nose", Bounds(-1., 11., 1., 13.));
    return object({{"rootId", Json(std::uint32_t(100))}, {"source_sha256", Json(Text(text("synthetic-test")))},
        {"options", Json(Object{})}, {"materials", Json(materials)}});
}

void ngTestRigPipeline() {
    using namespace nijigenerate::autorig::deterministic::geometry;
    using namespace nijigenerate::autorig::deterministic::controls;
    using namespace nijigenerate::autorig::deterministic::observation;
    using namespace nijigenerate::autorig::deterministic::physics;
    using nijigenerate::compat::Slice;
    using Strings = Slice<nijigenerate::compat::RetainedStringSlice>;
    struct Capture {
        Strings stages, attempts;
        bool failControls = false;
    };
    auto capture = std::allocate_shared<Capture>(nicxlive::core::d_runtime::SourceAllocator<Capture>{});
    testReviewPlan();
    Slice<Point2> narrow{{0.,0.},{.0002,0.},{10.,0.},{0.,1.},{.0002,1.},{10.,1.}};
    auto narrowCheck = ngRigValidateGrid(narrow, 3, .02, narrow);
    NG_ORIGINAL_ASSERT(ngRigNumber(get(narrowCheck, "min_area_ratio")) == 1, 169);
    auto folded = narrow.duplicate(); folded[4] = Point2(-.0002, 1.);
    bool rejected = false;
    try { ngRigValidateGrid(folded, 3, .02, narrow); }
    catch (const nijigenerate::compat::ThrowableException&) { rejected = true; }
    NG_ORIGINAL_ASSERT(rejected, 173);
    auto original = roundTrip(ngTestModelObservation());
    NG_ORIGINAL_ASSERT(equal(roleCandidates("hair-curl-r"), strings({"hair_side"})), 175);
    NG_ORIGINAL_ASSERT(equal(roleCandidates("plush-head"), strings({"pelvis_accessory"})), 176);
    NG_ORIGINAL_ASSERT(equal(roleCandidates("earring-l"), strings({"headwear"})), 177);
    NG_ORIGINAL_ASSERT(equal(roleCandidates("eye-white-r"), strings({"face_feature"})), 178);
    NG_ORIGINAL_ASSERT(roleCandidates("*# eywhite").empty(), 179);
    NG_ORIGINAL_ASSERT(equal(roleCandidates("*# eyewhite"), strings({"face_feature"})), 180);
    auto unknownSource = ngTestModelObservation();
    auto unknown = copyObject(read(unknownSource, "materials", std::size_t(0)));
    put(unknown, "uuid", Json(std::uint32_t(999)));
    put(unknown, "name", Json(Text(text("影_髪から顔への落ち影"))));
    put(unknown, "path", Json(Text(text("/MODEL/unknown-shadow"))));
    auto unknownMaterials = get(unknownSource, "materials").sourceArrayValue();
    unknownMaterials.append(unknown); put(unknownSource, "materials", Json(unknownMaterials));
    auto inherited = ngRigClassifyMaterials(unknownSource, get(unknownSource, "options"));
    auto shadow = last(get(inherited, "materials"));
    NG_ORIGINAL_ASSERT(get(shadow, "role").as_string() == text("face") &&
        get(shadow, "semantic_source").as_string() == text("alpha_proximal_support_candidate"), 189);
    put(unknown, "receiver", Json(std::uint32_t(3)));
    unknownMaterials[unknownMaterials.size()-1] = unknown;
    inherited = ngRigClassifyMaterials(unknownSource, get(unknownSource, "options"));
    NG_ORIGINAL_ASSERT(get(last(get(inherited, "materials")), "role").as_string() == text("torso"), 193);
    auto observation = ngRigClassifyMaterials(original, get(original, "options"));
    auto classifiedReview = ngRigClassificationReview(observation, Json{});
    NG_ORIGINAL_ASSERT(length(get(classifiedReview, "classification")) == length(get(original, "materials")), 196);
    NG_ORIGINAL_ASSERT(read(classifiedReview, "classification", std::size_t(0), "role").as_string() == text("face"), 197);
    NG_ORIGINAL_ASSERT(!contains(read(classifiedReview, "classification", std::size_t(0)), "cloud"), 198);
    auto corrected = ngRigClassifyMaterials(original, object({{"materials", object({{"/iris_r",
        object({{"role", "face_feature"}, {"feature", "upper"}, {"side", "L"}})}})}}));
    NG_ORIGINAL_ASSERT(read(corrected, "materials", std::size_t(13), "feature").as_string() == text("upper"), 202);
    NG_ORIGINAL_ASSERT(read(corrected, "materials", std::size_t(13), "side_hint").as_string() == text("L"), 203);
    auto correctedPlan = ngRigCompileProgram(corrected, ngRigDeriveEvidence(corrected));
    NG_ORIGINAL_ASSERT(read(correctedPlan, "carriers", std::size_t(13), "side").as_string() == text("L"), 205);
    auto clippedObservation = ngTestModelObservation();
    put(get(clippedObservation, "materials").as_array()[13], "receiver", Json(12));
    auto featureCleared = ngRigClassifyMaterials(clippedObservation,
        object({{"materials", object({{"/iris_r", object({{"feature", ""}})}})}}));
    NG_ORIGINAL_ASSERT(read(featureCleared, "materials", std::size_t(13), "feature").as_string() == text(""), 210);
    auto receiverSide = ngRigClassifyMaterials(clippedObservation, object({{"materials", object({
        {"/sclera_r", object({{"side", "L"}})}, {"/iris_r", object({{"feature", "iris"}})}})}}));
    NG_ORIGINAL_ASSERT(!contains(read(receiverSide, "materials", std::size_t(13)), "side_override"), 214);
    auto receiverSidePlan = ngRigCompileProgram(receiverSide, ngRigDeriveEvidence(receiverSide));
    NG_ORIGINAL_ASSERT(read(receiverSidePlan, "carriers", std::size_t(11), "side").as_string() == text("L"), 216);
    NG_ORIGINAL_ASSERT(read(receiverSidePlan, "carriers", std::size_t(13), "side").as_string() == text("L"), 217);
    put(unknown, "receiver", 12); unknownMaterials[unknownMaterials.size()-1] = unknown;
    auto edited = editedOverride(Json{}, static_cast<std::uint8_t>(RigMaterialField::side), "", false, "", "L");
    NG_ORIGINAL_ASSERT(edited.sourceObjectNoRef().size() == 1 && get(edited, "side").as_string() == text("L"), 221);
    const auto unknownOptions = [&] {
        Object materials; materials[get(unknown, "path").as_string()] = edited;
        return object({{"materials", Json(materials)}});
    };
    auto sideOnly = ngRigClassifyMaterials(unknownSource, unknownOptions());
    NG_ORIGINAL_ASSERT(get(last(get(sideOnly, "materials")), "feature").as_string() == text("sclera"), 224);
    edited = editedOverride(edited, static_cast<std::uint8_t>(RigMaterialField::stationary), "", true, "", "");
    NG_ORIGINAL_ASSERT(edited.sourceObjectNoRef().size() == 2 && !contains(edited, "feature"), 226);
    edited = editedOverride(edited, static_cast<std::uint8_t>(RigMaterialField::feature), "", true, "", "");
    auto explicitlyCleared = ngRigClassifyMaterials(unknownSource, unknownOptions());
    NG_ORIGINAL_ASSERT(get(last(get(explicitlyCleared, "materials")), "feature").as_string() == text(""), 230);
    put(edited, "role", "face_feature");
    edited = editedOverride(edited, static_cast<std::uint8_t>(RigMaterialField::role), "", false, "iris", "R");
    NG_ORIGINAL_ASSERT(!contains(edited, "role") && get(edited, "side").as_string() == text("L") && get(edited, "static").as_bool(), 234);
    auto landmarkOptions = object({{"landmarks", object({{"head_top", array({-1., -1.})}})}});
    auto landmarkObservation = ngRigClassifyMaterials(original, landmarkOptions);
    NG_ORIGINAL_ASSERT(equal(ngRigPoint(read(ngRigDeriveEvidence(landmarkObservation), "landmarks", "head_top", "xy")), Point2(-1.,-1.)), 238);
    auto stationary = ngRigClassifyMaterials(original,
        object({{"materials", object({{"/face", object({{"static", true}})}})}}));
    NG_ORIGINAL_ASSERT(read(stationary, "materials", std::size_t(0), "static").as_bool(), 241);
    auto evidence = ngRigDeriveEvidence(observation);
    auto program = ngRigCompileProgram(observation, evidence);
    NG_ORIGINAL_ASSERT(length(read(program, "scaffold", "bones")) == 21, 244);
    NG_ORIGINAL_ASSERT(length(get(program, "carriers")) == length(get(original, "materials")), 245);
    NG_ORIGINAL_ASSERT(length(get(program, "domains")) < length(get(program, "carriers")), 246);
    NG_ORIGINAL_ASSERT(length(get(program, "domains")) == 6, 247);
    NG_ORIGINAL_ASSERT(length(read(program, "hierarchy", "groups")) == 6, 248);
    NG_ORIGINAL_ASSERT(ngRigUnsigned(read(program, "hierarchy", "body_origin")) == 3, 249);
    NG_ORIGINAL_ASSERT(ngRigUnsigned(read(program, "hierarchy", "face_origin")) == 1, 250);
    NG_ORIGINAL_ASSERT(read(program, "hierarchy", "groups", std::size_t(1), "id").as_string() == text("Head::Root"), 251);
    NG_ORIGINAL_ASSERT(ngRigUnsigned(read(program, "hierarchy", "groups", std::size_t(1), "parent", "node")) == 3, 252);
    auto anonymous = roundTrip(original);
    put(anonymous, "rootId", Json(std::uint32_t(700100)));
    put(anonymous, "source_sha256", "different-source-identity");
    for (auto& material : get(anonymous, "materials").sourceArrayValue()) {
        put(material, "uuid", Json(ngRigUnsigned(get(material, "uuid")) + 700000));
        const auto path = get(material, "path").as_string();
        put(material, "path", Json(nijigenerate::autorig::deterministic::contracts::detail::allocateContractStorage([&] {
            return Text(std::string("/asset_copy") + std::string(path.view()));
        })));
    }
    anonymous = ngRigClassifyMaterials(anonymous, get(anonymous, "options"));
    auto anonymousProgram = ngRigCompileProgram(anonymous, ngRigDeriveEvidence(anonymous));
    NG_ORIGINAL_ASSERT(read(anonymousProgram, "scaffold", "bones") == read(program, "scaffold", "bones"), 264);
    NG_ORIGINAL_ASSERT(get(anonymousProgram, "native_drivers") == get(program, "native_drivers"), 265);
    NG_ORIGINAL_ASSERT(get(anonymousProgram, "native_depth_scale") == get(program, "native_depth_scale"), 266);
    NG_ORIGINAL_ASSERT(get(anonymousProgram, "reference_template") == get(program, "reference_template"), 267);
    auto carriers = get(program, "carriers").sourceArrayValue();
    for (std::size_t i = 0; i < carriers.size(); ++i) {
        auto carrier = carriers[i]; auto renamed = read(anonymousProgram, "carriers", i);
        NG_ORIGINAL_ASSERT(ngRigUnsigned(get(renamed, "part")) == ngRigUnsigned(get(carrier, "part")) + 700000, 270);
        for (const auto field : {"domain_id", "owner", "chart", "bones", "xs", "ys", "depth", "depth_model_units"}) {
#ifndef NDEBUG
            if (get(renamed, field) != get(carrier, field)) {
                const auto message = nijigenerate::autorig::deterministic::contracts::detail::allocateContractStorage([&] {
                    return nijigenerate::compat::RetainedString(std::string("Model identity changed ") + field);
                });
                throw nijigenerate::compat::ngStaticError<nijigenerate::compat::AssertError>(message,
                    nijigenerate::compat::RetainedString::fromStaticLiteral("tests/autorig_pipeline.d"), 272);
            }
#endif
        }
    }
    NG_ORIGINAL_ASSERT(ngRigUnsigned(read(anonymousProgram, "hierarchy", "body_origin")) == 700003, 274);
    NG_ORIGINAL_ASSERT(ngRigUnsigned(read(anonymousProgram, "hierarchy", "face_origin")) == 700001, 275);
    auto partitioned = roundTrip(observation);
    put(partitioned, "groups", array({object({{"uuid", 501}, {"parent", 100}, {"name", "proximal"}}),
        object({{"uuid", 502}, {"parent", 100}, {"name", "distal"}})}));
    put(get(partitioned, "materials").as_array()[3], "parent", 501);
    put(get(partitioned, "materials").as_array()[4], "parent", 502);
    auto sharedProgram = ngRigCompileProgram(partitioned, ngRigDeriveEvidence(partitioned));
    NG_ORIGINAL_ASSERT(length(get(sharedProgram, "domains")) == 6, 283);
    NG_ORIGINAL_ASSERT(read(sharedProgram, "carriers", std::size_t(3), "domain_id").as_string() ==
        read(sharedProgram, "carriers", std::size_t(4), "domain_id").as_string(), 284);
    NG_ORIGINAL_ASSERT(read(sharedProgram, "carriers", std::size_t(3), "bones") ==
        read(sharedProgram, "carriers", std::size_t(4), "bones"), 285);
    auto spanning = roundTrip(original);
    auto bridge = copyObject(read(spanning, "materials", std::size_t(3)));
    put(bridge, "uuid", 777); put(bridge, "name", "arm_shared"); put(bridge, "path", "/arm_shared");
    auto points = ngRigPoints(get(bridge, "cloud")); auto bothPoints = points.duplicate();
    for (auto p : points) { p[0] = -p[0]; bothPoints.append(p); }
    put(bridge, "cloud", ngRigPointsJson(bothPoints)); put(bridge, "bounds", array({-30.,29.,30.,65.}));
    auto spanningMaterials = get(spanning, "materials").sourceArrayValue();
    spanningMaterials.append(bridge); put(spanning, "materials", Json(spanningMaterials));
    spanning = ngRigClassifyMaterials(spanning, get(spanning, "options"));
    auto spanningProgram = ngRigCompileProgram(spanning, ngRigDeriveEvidence(spanning));
    auto sharedArm = last(get(spanningProgram, "carriers"));
    NG_ORIGINAL_ASSERT(get(sharedArm, "owner").as_string() == text("arm:both") && get(sharedArm, "side").as_string() == text("Both"), 296);
    NG_ORIGINAL_ASSERT(length(get(sharedArm, "bones")) == 6, 297);
    bool hasRelief = false;
    for (auto z : ngRigNumbers(get(sharedArm, "depth"))) {
        NG_ORIGINAL_ASSERT(std::isfinite(z), 299); if (z != 0) hasRelief = true;
    }
    NG_ORIGINAL_ASSERT(hasRelief, 300);
    NG_ORIGINAL_ASSERT(ngRigNumber(get(program, "native_depth_scale")) > 1, 301);
    Array targets;
    for (auto carrier : get(program, "carriers").sourceArrayValue()) {
        auto target = carrier; put(target, "grid", get(carrier, "part"));
        auto bounds = ngRigNumbers(get(carrier, "bounds"));
        Slice<Point2> world{{bounds[0],bounds[1]}, {bounds[2],bounds[1]},
            {bounds[0],bounds[3]}, {bounds[2],bounds[3]}};
        put(target, "world", ngRigPointsJson(world));
        put(target, "mapping", object({{"vertices", ngRigPointsJson(world)},
            {"triangles", array({array({0,1,2}), array({1,3,2})})}}));
        put(target, "root_to_local_direction", array({1.,0.,0.,1.})); targets.append(target);
    }
    put(observation, "targets", Json(targets));
    auto fixedFeet = ngRigCompileFixedFootCorrections(observation, program);
    NG_ORIGINAL_ASSERT(length(get(fixedFeet, "operations")) > 0, 316);
    for (auto operation : get(fixedFeet, "operations").sourceArrayValue())
        for (auto value : ngRigNumbers(get(operation, "values"))) {
            NG_ORIGINAL_ASSERT(std::isfinite(value), 319);
            if (ngRigUnsigned(read(operation, "key", std::size_t(0))) == 2) NG_ORIGINAL_ASSERT(value == 0, 320);
        }
    auto controls = ngRigCompileControls(observation);
    NG_ORIGINAL_ASSERT(length(get(controls, "masks")) == 2 && length(get(controls, "mechanisms")) == 5, 324);
    for (auto mechanism : get(controls, "mechanisms").sourceArrayValue())
        for (auto operation : get(mechanism, "operations").sourceArrayValue())
            for (auto key : get(operation, "keys").sourceArrayValue())
                for (auto offset : ngRigNumbers(get(key, "offsets"))) NG_ORIGINAL_ASSERT(offset == offset, 326);
    Slice<std::uint8_t> rgba{0,0,0,255,0,0,0,255,0,0,0,255,0,0,0,255};
    Array triangles{array({0,1,2}),array({1,3,2})};
    auto mesh = object({{"vertices", ngRigPointsJson({{2.,3.},{4.,3.},{2.,5.},{4.,5.}})},
        {"uv", ngRigPointsJson({{0.,0.},{1.,0.},{0.,1.},{1.,1.}})}, {"triangles", Json(triangles)}});
    auto reloadedMesh = roundTrip(mesh);
    NG_ORIGINAL_ASSERT(ngRigSameTextureMapping(mesh, reloadedMesh), 333);
    get(reloadedMesh, "vertices").as_array()[0].as_array()[0] = Json(2.001);
    NG_ORIGINAL_ASSERT(!ngRigSameTextureMapping(mesh, reloadedMesh), 335);
    auto cloud = ngRigTextureSupport(rgba, 2, 2, mesh);
    NG_ORIGINAL_ASSERT(cloud.size() == 4 && std::abs(cloud[0][0]-2.5)<1e-12 && std::abs(cloud[0][1]-3.5)<1e-12, 337);
    auto projected = ngRigProjectControlOrientation({{0.,0.},{1.,0.},{0.,1.}},
        array({array({0,1,2})}), {0.,0.,0.,0.,0.,-2.});
    NG_ORIGINAL_ASSERT(1+projected[5]-projected[1]>.00399, 340);
    NG_ORIGINAL_ASSERT(projected[0] == 0 && projected[2] == 0 && projected[4] == 0, 341);
    Slice<std::uint8_t> separate; separate.resize(5*4);
    separate[3] = separate[7] = separate[19] = 255;
    auto component = ngRigLargestAlphaComponent(separate, 5, 1);
    NG_ORIGINAL_ASSERT(component[3] == 255 && component[7] == 255 && component[19] == 0, 345);
    auto alphaMaterial = object({{"texture_size", array({2,2})}, {"source_root_mapping", mesh},
        {"alpha_runs_128", ngRigAlphaRuns(rgba,128)}});
    NG_ORIGINAL_ASSERT(ngRigAlphaCoverage(alphaMaterial, {{2.5,3.5},{4.5,3.5}}) == .5, 348);
    put(alphaMaterial, "alpha_runs_32", ngRigAlphaRuns(rgba,32));
    auto physicsAlpha = ngRigPhysicsAsset(alphaMaterial);
    NG_ORIGINAL_ASSERT(physicsAlpha.affineMapping && physicsAlpha.points.size() == cloud.size(), 351);
    for (std::size_t i=0; i<physicsAlpha.points.size(); ++i) for (std::size_t axis=0; axis<2; ++axis)
        NG_ORIGINAL_ASSERT(std::abs(physicsAlpha.points[i][axis]-cloud[i][axis])<1e-10, 352);
    put(alphaMaterial, "source_root_mapping", reloadedMesh);
    physicsAlpha = ngRigPhysicsAsset(alphaMaterial);
    auto warpedCloud = ngRigTextureSupport(rgba,2,2,reloadedMesh);
    NG_ORIGINAL_ASSERT(!physicsAlpha.affineMapping && equal(physicsAlpha.points,warpedCloud), 356);
    auto croppedMesh = roundTrip(mesh);
    put(croppedMesh, "triangles", Json(triangles.slice(0,1)));
    put(alphaMaterial, "source_root_mapping", croppedMesh);
    physicsAlpha = ngRigPhysicsAsset(alphaMaterial);
    auto croppedCloud = ngRigTextureSupport(rgba,2,2,croppedMesh);
    NG_ORIGINAL_ASSERT(physicsAlpha.points.size() == croppedCloud.size(), 362);
    for (std::size_t i=0; i<physicsAlpha.points.size(); ++i) for (std::size_t axis=0; axis<2; ++axis)
        NG_ORIGINAL_ASSERT(std::abs(physicsAlpha.points[i][axis]-croppedCloud[i][axis])<1e-10, 363);
    NG_ORIGINAL_ASSERT(physicsAlpha.points.size() == 3 && !physicsAlpha.mask[3], 364);
    Slice<std::uint8_t> padded; padded.resize(6*4*4);
    for (std::size_t y=1; y<3; ++y) for (std::size_t x=1; x<5; ++x) padded[(y*6+x)*4+3] = 255;
    auto contourMaterial = object({{"texture_size", array({6,4})}, {"alpha_runs_32", ngRigAlphaRuns(padded,32)},
        {"source_root_mapping", object({{"vertices", ngRigPointsJson({{0.,0.},{6.,0.},{0.,4.},{6.,4.}})},
            {"uv", get(mesh,"uv")}, {"triangles", get(mesh,"triangles")}})}});
    auto contour = ngRigAlphaContour(contourMaterial);
    NG_ORIGINAL_ASSERT(get(contour,"opaque_pixels").as_uint64() == 8 && length(get(contour,"points")) == 8, 372);
    for (auto normal : ngRigPoints(get(contour,"normals")))
        NG_ORIGINAL_ASSERT(std::abs(normal[0]*normal[0]+normal[1]*normal[1]-1)<1e-9, 373);
    auto localObservation = copyObject(original);
    auto localMaterial = copyObject(read(original,"materials",std::size_t(0)));
    put(localMaterial,"name","unknown_ornament"); put(localMaterial,"path","/unknown_ornament");
    put(localObservation,"materials",array({localMaterial}));
    auto localClassified = ngRigClassifyMaterials(localObservation,Json(Object{}));
    auto localEvidence = ngRigDeriveEvidence(localClassified);
    NG_ORIGINAL_ASSERT(get(localEvidence,"kind").as_string() == text("local"), 380);
    auto localProgram = ngRigCompileProgram(localClassified,localEvidence);
    NG_ORIGINAL_ASSERT(get(localProgram,"kind").as_string() == text("local") && !contains(localProgram,"scaffold"), 382);
    auto transformed = copyObject(mesh);
    put(transformed,"vertices",ngRigPointsJson({{6.,8.},{10.,8.},{6.,12.},{10.,12.}}));
    auto registration = ngRigSourceUVRegistration(mesh,transformed);
    auto scale = ngRigNumbers(get(registration,"scale")), shift = ngRigNumbers(get(registration,"shift"));
    NG_ORIGINAL_ASSERT(std::abs(scale[0]-.5)<1e-10 && std::abs(scale[1]-.5)<1e-10, 387);
    NG_ORIGINAL_ASSERT(std::abs(shift[0]+1)<1e-10 && std::abs(shift[1]+1)<1e-10, 388);
    NG_ORIGINAL_ASSERT(ngRigVerifySourceUV(mesh,mesh)<1e-12, 389);
    bool changedFrame = false;
    try { ngRigVerifySourceUV(mesh,transformed); }
    catch (const nijigenerate::compat::ThrowableException&) { changedFrame = true; }
    NG_ORIGINAL_ASSERT(changedFrame, 392);
    NG_ORIGINAL_ASSERT(equal(ngRigClosestTriangleWeights({2.,.5},{0.,0.},{1.,0.},{0.,1.}), Point3(0.,1.,0.)), 393);
    NG_ORIGINAL_ASSERT(equal(ngRigClosestTriangleWeights({.5,-1.},{0.,0.},{1.,0.},{0.,1.}), Point3(.5,.5,0.)), 394);
    NG_ORIGINAL_ASSERT(length(get(program,"native_drivers")) == 42, 395);
    for (auto driver : get(program,"native_drivers").sourceArrayValue())
        for (auto value : get(driver,"values").sourceArrayValue()) {
            auto point = ngRigNumbers(get(value,"key"));
            if (equal(point,Slice<double>{0.,0.})) NG_ORIGINAL_ASSERT(ngRigNumber(get(value,"value")) == 0, 398);
        }
    auto cheekState = copyObject(observation);
    auto cheekMaterials = get(observation,"materials").sourceArrayValue().duplicate();
    Slice<std::uint8_t> facePixels; facePixels.resize(24*20*4);
    for (std::size_t i=0; i<24*20; ++i) facePixels[i*4+3] = 255;
    auto skin = copyObject(cheekMaterials[0]);
    put(skin,"texture_size",array({24,20})); put(skin,"alpha_runs_32",ngRigAlphaRuns(facePixels,32));
    put(skin,"source_root_mapping",object({{"vertices",ngRigPointsJson({{-12.,0.},{12.,0.},{-12.,20.},{12.,20.}})},
        {"uv",get(mesh,"uv")},{"triangles",get(mesh,"triangles")}})); cheekMaterials[0] = skin;
    auto temple = copyObject(cheekMaterials[11]);
    put(temple,"uuid",Json(std::uint64_t(1001))); put(temple,"feature","brow");
    put(temple,"cloud",ngRigPointsJson({{-9.,4.},{-3.,4.}})); cheekMaterials.append(temple);
    put(cheekState,"materials",Json(cheekMaterials));
    auto cheekTargets = targets.duplicate();
    for (auto& target : cheekTargets) {
        target = copyObject(target); put(target,"origin",array({0.,0.})); put(target,"grid",Json(std::uint64_t(2000)));
    }
    auto templeTarget = copyObject(cheekTargets[11]);
    put(templeTarget,"part",get(temple,"uuid")); cheekTargets.append(templeTarget);
    put(cheekState,"targets",Json(cheekTargets)); put(cheekState,"controls",controls);
    put(cheekState,"source_uv_program_sha256","synthetic-uv");
    Array headKeys;
    auto faceGrid = ngRigPoints(get(cheekTargets[0],"points"));
    for (auto side : {-1.,0.,1.}) {
        Slice<double> offsets; for ([[maybe_unused]] auto point : faceGrid) { offsets.append(0.); offsets.append(0.); }
        headKeys.append(object({{"grid",Json(std::uint64_t(2000))}, {"key",array({Json(static_cast<int>(side+1)),1})},
            {"offsets",Json(Array(offsets.begin(),offsets.end()))}, {"projection",array({1.,0.,0.,1.,side*.3,0.,0.,0.})}}));
    }
    put(cheekState,"native_head_keys",Json(headKeys));
    put(cheekState,"depth_angle_program_sha256",Json(ngRigDigest(Json(headKeys))));
    auto cheekReport = ngRigCompileCheekCorrections(cheekState,program);
    auto disabledControlState = copyObject(cheekState);
    put(disabledControlState,"control_geometry",ngRigControlGeometry(controls));
    Strings disabledMechanisms;
    for (auto mechanism : get(controls,"mechanisms").sourceArrayValue()) {
        auto name = get(mechanism,"name").as_string();
        disabledMechanisms.append(nijigenerate::autorig::deterministic::contracts::detail::allocateContractStorage([&] {
            return nijigenerate::compat::RetainedString(std::string("mechanism:")+std::string(name.view()));
        }));
    }
    const auto stringsJson = [](const Strings& values) {
        Array result; for (auto value : values) result.append(Json(value)); return Json(result);
    };
    Json reviewedOperations;
    put(disabledControlState,"controls",ngRigReviewControls(controls,
        object({{"disabled",stringsJson(disabledMechanisms)}}),reviewedOperations));
    NG_ORIGINAL_ASSERT(length(read(disabledControlState,"controls","mechanisms")) == 0, 441);
    for (auto mechanism : read(disabledControlState,"control_geometry","mechanisms").sourceArrayValue())
        NG_ORIGINAL_ASSERT(!contains(mechanism,"operations"), 443);
    auto disabledControlCheek = ngRigCompileCheekCorrections(disabledControlState,program);
    NG_ORIGINAL_ASSERT(disabledControlCheek == cheekReport, 445);
    Strings disabledBindings;
    for (auto mechanism : get(controls,"mechanisms").sourceArrayValue())
        for (auto operation : get(mechanism,"operations").sourceArrayValue())
            disabledBindings.append(reviewOperationId("control",operation,get(mechanism,"name").as_string()));
    put(disabledControlState,"controls",ngRigReviewControls(controls,
        object({{"disabled",stringsJson(disabledBindings)}}),reviewedOperations));
    NG_ORIGINAL_ASSERT(length(read(disabledControlState,"controls","mechanisms")) == 0, 451);
    NG_ORIGINAL_ASSERT(ngRigCompileCheekCorrections(disabledControlState,program) == cheekReport, 452);
    NG_ORIGINAL_ASSERT(ngRigDigest(cheekReport) == ngRigDigest(nijigenerate::autorig::json::ngParseAutoRigJson(
        nijigenerate::compat::ngJsonStringify(cheekReport))), 453);
    NG_ORIGINAL_ASSERT(get(cheekReport,"applicable").as_bool() && length(get(cheekReport,"operations")) == 3, 454);
    for (auto operation : get(cheekReport,"operations").sourceArrayValue()) {
        auto allowed = get(operation,"allowed_vertices").sourceArrayValue();
        auto values = ngRigNumbers(get(operation,"values"));
        for (auto value : values) NG_ORIGINAL_ASSERT(std::isfinite(value), 457);
        for (std::size_t i=0; i<allowed.size(); ++i)
            if (!allowed[i].as_bool() || get(operation,"near_side").as_int64() == 0)
                NG_ORIGINAL_ASSERT(values[i*2] == 0 && values[i*2+1] == 0, 459);
    }
    for (auto cheekObservation : get(cheekReport,"observations").sourceArrayValue())
        NG_ORIGINAL_ASSERT(std::isfinite(ngRigNumber(get(cheekObservation,"maximum_local_residual"))), 462);
    using namespace nijigenerate::autorig::framework;
    using namespace nijigenerate::autorig::workflow;
    using nijigenerate::autorig::deterministic::processor::AnimeFrontViewRigProcessor;
    using Stage = nijigenerate::autorig::deterministic::pipeline::RigNativeStage;
    auto& stages = capture->stages;
    Stage stage = [capture](nijigenerate::compat::RetainedStringSlice stageName, Json state, Json plan,
            nijigenerate::compat::ConstSlice<std::uint8_t>, std::shared_ptr<AutoRigTaskContext> context) {
        NG_ORIGINAL_ASSERT(nijigenerate::core::tasks::TaskFiber::hasCurrent(), 467);
        capture->stages.append(stageName);
        if (stageName == text("observe-model")) state = roundTrip(ngTestModelObservation());
        else NG_ORIGINAL_ASSERT(length(read(plan,"scaffold","bones")) == 21, 470);
        if (stageName == text("verify-saved-rig")) return object({{"passed",true}});
        context->publishBlob(retained("model"),{std::uint8_t(1),2,3});
        return state;
    };
    auto processor = std::allocate_shared<AnimeFrontViewRigProcessor>(
        SourceAllocator<AnimeFrontViewRigProcessor>{}, AnimeFrontViewRigProcessor::Projection{}, stage);
    auto sessions = std::allocate_shared<AutoRigSessionManager>(SourceAllocator<AutoRigSessionManager>{},
        sourceWorkspace("autorig-pipeline-"));
    sessions->registerProcessor(processor);
    auto workflows = std::allocate_shared<AutoRigWorkflowManager>(SourceAllocator<AutoRigWorkflowManager>{},sessions);
    NG_ORIGINAL_ASSERT(workflows->listPresets().size() == 1, 478);
    NG_ORIGINAL_ASSERT(workflows->listPresets()[0].spec.id == text("model-to-rig"), 479);
    auto run = workflows->create(processor->procId(),retained("model-to-rig"));
    auto input = run->session()->inputContext(run->stepTaskId(retained("source")));
    NG_ORIGINAL_ASSERT(input->hasValue(retained("options")) && input->value(retained("options")).json.sourceObjectNoRef().size() == 0, 482);
    run->setInput(retained("options"),AutoRigValue::jsonValue(object({{"render",true}})));
    NG_ORIGINAL_ASSERT(run->orderedSteps().size() == 16, 484);
    run->execute();
    NG_ORIGINAL_ASSERT(run->snapshot().state == AutoRigWorkflowState::Succeeded, 486);
    NG_ORIGINAL_ASSERT(!contains(get(run->session()->output(run->stepTaskId(retained("source")),retained("materials")).json,std::size_t(0)),"uuid"), 488);
    auto sourceNames = ngRigReviewMaterialNames(run->session()->output(run->stepTaskId(retained("source")),retained("review")).json);
    NG_ORIGINAL_ASSERT((sourceNames ? sourceNames->size() : 0) == length(get(original,"materials")) && reviewName(sourceNames,1) == text("/face"), 490);
    NG_ORIGINAL_ASSERT(([&] {
        const auto names = ngRigReviewMaterialNames(run->session()->output(run->stepTaskId(retained("source")),retained("materials")).json);
        return (names ? names->size() : 0) == 0;
    }()), 491);
    NG_ORIGINAL_ASSERT(equal(stages,strings({"observe-model","prepare-source-groups","prepare-shoulders","mesh-parts","register-source-uv",
        "prepare-feature-composites","compile-domain-layout","build-native-rig","weld-shoulders",
        "apply-rig-controls","validate-depth-inputs","bake-depth-angles","apply-shape-corrections",
        "apply-secondary-physics","verify-saved-rig"})), 492);
    NG_ORIGINAL_ASSERT(get(run->output(retained("report")).json,"passed").as_bool() && run->output(retained("model")).readBlob().size()>0, 496);
    auto detached = run->session()->output(run->stepTaskId(retained("source")),retained("state")).json;
    NG_ORIGINAL_ASSERT(!contains(detached,"materials"), 498);
    auto originalReference = read(detached,"artifact_refs","materials").as_string();
    auto artifactRefs = get(detached,"artifact_refs"); put(artifactRefs,"materials","detached-reader-edit");
    NG_ORIGINAL_ASSERT(read(run->session()->output(run->stepTaskId(retained("source")),retained("state")).json,
        "artifact_refs","materials").as_string() == originalReference, 501);
    auto observationMetadata = run->session()->output(run->stepTaskId(retained("source")),retained("observation")).json;
    NG_ORIGINAL_ASSERT(!contains(get(observationMetadata,std::size_t(0)),"cloud"), 504);
    auto completedCount = stages.size();
    run->execute();
    NG_ORIGINAL_ASSERT(stages.size() == completedCount, 507);
    workflows->close(run->id());
    auto restored = workflows->reopen(run->id());
    NG_ORIGINAL_ASSERT(restored->id() == run->id() && restored->snapshot().state == AutoRigWorkflowState::Succeeded, 510);
    NG_ORIGINAL_ASSERT(get(restored->session()->inputContext(restored->stepTaskId(retained("source")))->value(retained("options")).json,"render").as_bool(), 511);
    restored->execute();
    NG_ORIGINAL_ASSERT(stages.size() == completedCount && get(restored->output(retained("report")).json,"passed").as_bool(), 513);
    auto sourceAttempt = run->session()->task(run->stepTaskId(retained("source"))).attempt;
    run->session()->inputContext(run->stepTaskId(retained("compile")))->setValue(retained("options"),AutoRigValue::jsonValue(
        object({{"materials",object({{"/iris_r",object({{"static",true}})}})}})));
    run->executeStep(retained("compile"),true);
    NG_ORIGINAL_ASSERT(run->session()->task(run->stepTaskId(retained("source"))).attempt == sourceAttempt, 518);
    NG_ORIGINAL_ASSERT(length(get(run->session()->outputContext(run->stepTaskId(retained("compile")))->value(retained("program")).json,"carriers"))
        == length(get(original,"materials"))-1, 519);
    NG_ORIGINAL_ASSERT(read(run->session()->output(run->stepTaskId(retained("compile")),retained("review")).json,
        "classification",std::size_t(13),"static").as_bool(), 521);
    run->execute();
    auto previousStages = stages.size();
    auto compileAttempt = run->session()->task(run->stepTaskId(retained("compile")),false).attempt;
    run->session()->setInput(run->stepTaskId(retained("weld")),retained("review"),AutoRigValue::jsonValue(
        object({{"disabled",array({"weld:1:2"})}})));
    run->execute();
    NG_ORIGINAL_ASSERT(run->session()->task(run->stepTaskId(retained("source")),false).attempt == sourceAttempt, 528);
    NG_ORIGINAL_ASSERT(run->session()->task(run->stepTaskId(retained("compile")),false).attempt == compileAttempt, 529);
    NG_ORIGINAL_ASSERT(equal(stages.slice(previousStages,stages.size()), strings({"weld-shoulders","apply-rig-controls",
        "validate-depth-inputs","bake-depth-angles","apply-shape-corrections","apply-secondary-physics","verify-saved-rig"})), 530);
    auto& attempts = capture->attempts;
    auto& failControls = capture->failControls;
    failControls = true;
    Stage failingStage = [capture](nijigenerate::compat::RetainedStringSlice stageName, Json state, Json,
            nijigenerate::compat::ConstSlice<std::uint8_t>, std::shared_ptr<AutoRigTaskContext> context) {
        capture->attempts.append(stageName);
        if (stageName == text("observe-model")) state = ngTestModelObservation();
        if (stageName == text("apply-rig-controls") && capture->failControls)
            throw nijigenerate::compat::ThrowableException(retained("synthetic local-control failure"), retained("tests/autorig_pipeline.d"), 539);
        if (stageName == text("verify-saved-rig")) return object({{"passed",true}});
        context->publishBlob(retained("model"),{std::uint8_t(1),2,3});
        return state;
    };
    auto failingProcessor = std::allocate_shared<AnimeFrontViewRigProcessor>(SourceAllocator<AnimeFrontViewRigProcessor>{},
        AnimeFrontViewRigProcessor::Projection{},failingStage);
    auto diagnosticSessions = std::allocate_shared<AutoRigSessionManager>(SourceAllocator<AutoRigSessionManager>{},
        sourceWorkspace("autorig-finish-"));
    diagnosticSessions->registerProcessor(failingProcessor);
    auto diagnosticWorkflows = std::allocate_shared<AutoRigWorkflowManager>(SourceAllocator<AutoRigWorkflowManager>{},diagnosticSessions);
    auto diagnosticRun = diagnosticWorkflows->create(failingProcessor->procId(),retained("model-to-rig"));
    diagnosticRun->execute();
    NG_ORIGINAL_ASSERT(diagnosticRun->snapshot().state == AutoRigWorkflowState::Failed, 549);
    NG_ORIGINAL_ASSERT(diagnosticRun->session()->task(diagnosticRun->stepTaskId(retained("controls"))).state == AutoRigTaskState::Failed, 550);
    NG_ORIGINAL_ASSERT(equal(attempts.slice(attempts.size()-5,attempts.size()),strings({"validate-depth-inputs","bake-depth-angles",
        "apply-shape-corrections","apply-secondary-physics","verify-saved-rig"})), 551);
    auto completion = diagnosticRun->output(retained("report")).json;
    NG_ORIGINAL_ASSERT(!get(completion,"all_finish_stages_succeeded").as_bool() && !get(completion,"rig_complete").as_bool(), 554);
    NG_ORIGINAL_ASSERT(read(completion,"finish_stages",std::size_t(0),"error").as_string() == text("synthetic local-control failure"), 555);
    failControls = false;
    diagnosticRun->execute();
    NG_ORIGINAL_ASSERT(diagnosticRun->snapshot().state == AutoRigWorkflowState::Succeeded, 558);
    NG_ORIGINAL_ASSERT(diagnosticRun->session()->task(diagnosticRun->stepTaskId(retained("controls"))).attempt == 2, 559);
    NG_ORIGINAL_ASSERT(get(diagnosticRun->output(retained("report")).json,"all_finish_stages_succeeded").as_bool(), 560);
}
}
