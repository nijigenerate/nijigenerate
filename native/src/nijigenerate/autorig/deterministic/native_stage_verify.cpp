#include "native_stage.hpp"
#include "detail/native_stage.hpp"
#include "geometry.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nodes/projectable.hpp>
#include <param/binding_impl.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
template<class... Keys> Json field(const Json& value, std::string_view key, Keys... keys) {
    auto result = value.sourceIndexValue(key);
    if constexpr (sizeof...(keys)) return field(result, keys...);
    else return result;
}
Json absentArray() { return core::settings::SettingsJsonArray{}; }
template<class Values> Json numberArray(const Values& values) {
    core::settings::SettingsJsonArray result;
    for (auto value : values) result.append(Json(value));
    return result;
}
using DeformationBinding = nicxlive::core::param::DeformationParameterBinding;
using ValueBinding = nicxlive::core::param::ValueParameterBinding;
using Vec2 = nicxlive::core::math::Vec2;
using Vec2u = nicxlive::core::param::Vec2u;
using Points = compat::Slice<contracts::Point2>;
core::settings::SettingsString exceptionMessage(const compat::ThrowableException& error) {
    return core::settings::SettingsString(error.msg);
}
}

Json verifyRig(Json state, Json program, Task task) {
    enforceNative(field(state, "program_sha256").as_string() == field(program, "content_sha256").as_string(),"Rig program ownership mismatch",1609);
    core::settings::SettingsJsonArray grids, localPoses, findings;
    std::size_t boneCount = 0, keyCount = 0, intermediateCount = 0;
    const bool humanoid = stringGet(state, "kind", "humanoid") == "humanoid";
    framework::Text snapshotSignature;
    auto sourceFrames = verifySourceFrames(state, task);
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        snapshotSignature = editorSignature(puppet);
        auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(
            puppet->findNodeById(uuid(field(state, "rigRoot"))));
        enforceNative(bool(root),"Saved DepthRigRoot is missing",1619);
        boneCount = root->depthBones().size();
        enforceNative(boneCount == (humanoid ? field(program, "scaffold", "bones").sourceArrayValue().size() : 0),"Saved skeleton differs from the compiled rig kind",1620);
        if (humanoid) {
            for (auto sources = get(state, "source_group_grids", absentArray()); auto source : sources.sourceArrayValue()) {
                const auto innerIndex = root->findBindingIndex(uuid(field(source, "grid")));
                const auto surfaceIndex = root->findBindingIndex(uuid(field(source, "surface")));
                enforceNative(innerIndex >= 0 && surfaceIndex >= 0,"Saved imported group grid has no bone binding",1626);
                auto inner = readNativeArray(root->bindings,static_cast<std::size_t>(innerIndex),1627);
                auto surface = readNativeArray(root->bindings,static_cast<std::size_t>(surfaceIndex),1628);
                enforceNative(inner.sourceBoneUuids.size() == surface.sourceBoneUuids.size() &&
                    std::equal(inner.sourceBoneUuids.begin(), inner.sourceBoneUuids.end(), surface.sourceBoneUuids.begin()) &&
                    inner.influenceRule == surface.influenceRule,"Saved imported group grid differs from its semantic surface influence",1629);
            }
            auto originIds = field(state, "material_origins");
            for (auto definition : field(program, "hierarchy", "groups").sourceArrayValue()) {
                auto origin = puppet->findNodeById(uuid(field(originIds, nativeString(field(definition, "id")).view())));
                enforceNative(origin && origin->parentPtr() && origin->parentPtr()->getUUID() ==
                    uuid(field(definition, "parent", "node")),"Saved material origin parent differs from the semantic plan",1635);
                auto point = toRoot(origin, 0, 0);
                auto expected = ngRigNumbers(field(definition, "origin"));
                enforceNative(std::abs(point[0] - readNativeArray(expected,0,1638)) < .001 &&
                    std::abs(point[1] - readNativeArray(expected,1,1638)) < .001,"Saved material origin differs from its anatomical joint",1638);
            }
            framework::Table<bool> checked;
            for (auto target : field(state, "targets").sourceArrayValue()) {
                const auto domain = nativeString(field(target, "domain_id"));
                auto grid = puppet->findNodeById(uuid(field(target, "grid")));
                if (!checked.contains(domain)) {
                    auto parent = field(program, "hierarchy", "surface_parents", domain.view());
                    const auto expectedParent = parent.sourceObjectRef().if_contains("group")
                        ? uuid(field(originIds, nativeString(field(parent, "group")).view())) : uuid(field(parent, "node"));
                    enforceNative(grid && grid->parentPtr() && grid->parentPtr()->getUUID() == expectedParent,"Saved shared surface parent differs from the semantic plan",1648);
                    checked[domain]=true;
                }
                if (nativeString(field(target, "owner")).view() == "head" && nativeString(field(target, "chart")).view() == "face") {
                    bool inherited = false;
                    for (auto cursor = puppet->findNodeById(uuid(field(target, "part"))); cursor;
                        cursor = cursor->parentPtr())
                        if (cursor->getUUID() == uuid(field(program, "hierarchy", "face_origin"))) inherited = true;
                    enforceNative(inherited,"Facial mechanism escaped the face material origin",1656);
                }
            }
            for (auto definition : field(program, "scaffold", "bones").sourceArrayValue()) {
                auto bone = readNativeArray(root->depthBones(),0,1660);
                bool found = false;
                for (auto candidate : root->depthBones()) if (candidate->boneId.view() == field(definition, "id").as_string().view()) {
                    bone = candidate; found = true; break;
                }
                enforceNative(found,"Saved scaffold bone is missing",1664);
                auto head = ngRigNumbers(field(definition, "head")), tail = ngRigNumbers(field(definition, "tail"));
                const std::array<double, 3> actualHead{bone->restHead.x, bone->restHead.y, bone->restHead.z};
                const std::array<double, 3> actualTail{bone->restTail.x, bone->restTail.y, bone->restTail.z};
                for (std::size_t axis = 0; axis < 3; ++axis)
                    enforceNative(std::abs(actualHead[axis] - readNativeArray(head,axis,1668)) < .001 &&
                        std::abs(actualTail[axis] - readNativeArray(tail,axis,1669)) < .001,"Saved scaffold rest pose changed",1668);
                enforceNative(std::abs(bone->restRoll - ngRigNumber(field(definition, "rest_roll"))) < 1e-6,"Saved scaffold rest roll changed",1670);
                enforceNative(bone->allowParentToTargets == field(definition, "allow_parent_to_targets").as_bool(),"Saved bone support inheritance changed",1671);
                enforceNative(bone->lockToRoot == field(definition, "lock_to_root").as_bool(),"Saved bone root attachment changed",1673);
                auto data = serializedJson(bone);
                auto expectedZ = ngRigNumber(field(definition, "pose_origin_z"));
                if (!field(definition, "lock_to_root").as_bool() && !field(definition, "parent").is_null())
                    for (auto parent : field(program, "scaffold", "bones").sourceArrayValue())
                        if (field(parent, "id") == field(definition, "parent"))
                            expectedZ -= ngRigNumber(field(parent, "pose_origin_z"));
                enforceNative(std::abs(ngRigNumber(field(data, "transform", "trans").sourceIndexValue(std::size_t(2))) - expectedZ) < .001,"Saved bone Z origin differs from the registered template",1680);
            }
            for (auto driver : field(program, "native_drivers").sourceArrayValue()) {
                auto parameter = parameterByName(puppet, nativeString(field(driver, "parameter")));
                NodeRef bone;
                for (auto candidate : root->depthBones())
                    if (candidate->boneId.view() == field(driver, "bone").as_string().view()) bone = candidate;
                auto binding = std::dynamic_pointer_cast<ValueBinding>(parameter->getBinding(bone, nativeString(field(driver, "binding")).toNullableString()));
                enforceNative(bool(binding),"Saved native driver is missing",1688);
                for (auto value : field(driver, "values").sourceArrayValue()) {
                    auto point = ngRigNumbers(field(value, "key")); bool matched = false;
                    for (std::size_t y = 0, ny = parameter->axisPoints[1].size(); y < ny; ++y)
                        for (std::size_t x = 0, nx = parameter->axisPoints[0].size(); x < nx; ++x) {
                            const auto axisX=readNativeArray(parameter->axisPoints[0],x,1692);
                            const auto axisY=readNativeArray(parameter->axisPoints[1],y,1692);
                            auto actualPoint = parameter->unmapValue({axisX,axisY});
                            if (std::abs(actualPoint.x - readNativeArray(point,0,1693)) > 1e-6 ||
                                std::abs(actualPoint.y - readNativeArray(point,1,1693)) > 1e-6) continue;
                            const Vec2u index{static_cast<unsigned>(x), static_cast<unsigned>(y)};
                            const auto expected = static_cast<float>(ngRigNumber(field(value, "value")));
                            enforceNative(binding->isSet(index) && std::abs(binding->getValue(index) - expected) < 1e-6,"Saved native driver value changed",1698); matched = true;
                        }
                    enforceNative(matched,"Saved native driver axis changed",1701);
                }
            }
        }
        for (auto composites = get(state, "feature_composites", absentArray()); auto composite : composites.sourceArrayValue()) {
            auto node = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(
                puppet->findNodeById(uuid(field(composite, "uuid"))));
            if (node && !sameTextureMapping(textureMapping(node), field(composite, "mapping")))
                task->previewJson("composite-mesh-mismatch", objectEvidence({{stringLiteralEvidence("uuid"), field(composite, "uuid")},
                    {stringLiteralEvidence("expected"), field(composite, "mapping")}, {stringLiteralEvidence("actual"), textureMapping(node)}}));
            enforceNative(node && !node->autoResizedMesh && sameTextureMapping(textureMapping(node), field(composite, "mapping")),"Saved feature composite mesh changed",1710);
        }
    });
    NativeIntegerMap<std::uint32_t, bool> inspectedGrids;
    for (auto target : field(state, "targets").sourceArrayValue()) {
        ngRigCheckpoint(task);
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(field(target, "part"))));
            enforceNative(bool(part),"Saved material is missing",1720);
            enforceNative(sameTextureMapping(textureMapping(part), field(target, "mapping")),
                "Saved Part geometry or UV mapping changed after meshing",1721);
            if (!humanoid) return;
            const auto gridId = uuid(field(target, "grid"));
            if (inspectedGrids.contains(gridId)) return;
            inspectedGrids[gridId]=true;
            auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(puppet->findNodeById(gridId));
            auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid);
            enforceNative(part && grid && mapped,"Saved material or carrier is missing",1727);
            enforceNative(sameTextureMapping(textureMapping(part), field(target, "mapping")),
                "Saved Part geometry or UV mapping changed after meshing",1728);
            const auto depthCount = mapped->copyDepths().size();
            const auto vertexCount = grid->getVertices().size();
            enforceNative(depthCount == vertexCount,"Saved grid depth count mismatch",1729);
            Points rest;
            const auto initialVertices = grid->getVertices();
            for (std::size_t i = 0, count = initialVertices.size(); i < count; ++i) {
                (void)initialVertices.at(i);
                rest.append(contracts::Point2{});
            }
            const auto restCount = grid->getVertices().size();
            for (std::size_t i = 0; i < restCount; ++i) {
                if (i >= rest.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, rest.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),1732);
                auto& destination = rest[i];
                const auto xVertices = grid->getVertices();
                if (i >= xVertices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, xVertices.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),1732);
                const auto x = xVertices.at(i).x;
                const auto yVertices = grid->getVertices();
                if (i >= yVertices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, yVertices.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),1732);
                destination = contracts::Point2{x,yVertices.at(i).y};
            }
            core::settings::SettingsJsonArray keys;
            for (const auto name : std::array<compat::RetainedString,4>{
                    compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"),
                    compat::RetainedString::fromStaticLiteral("Face::Roll"),
                    compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
                    compat::RetainedString::fromStaticLiteral("Body::Roll")}) {
                auto parameter = parameterByName(puppet, name);
                auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(grid, "deform"));
                enforceNativeLazy(bool(binding),[&] { return nativeText({"Saved angle carrier binding missing: ",name.view()}); },1737);
                for (std::size_t y = 0, ny = parameter->axisPoints[1].size(); y < ny; ++y)
                    for (std::size_t x = 0, nx = parameter->axisPoints[0].size(); x < nx; ++x) {
                        const Vec2u index{static_cast<unsigned>(x), static_cast<unsigned>(y)};
                        enforceNativeLazy(binding->isSet(index),[&] { return nativeText({"Saved angle key was not baked: ",name.view()}); },1740);
                        auto offsets = binding->getValue(index).vertexOffsets;
                        enforceNative(offsets.size() == rest.size(),"Saved angle key vertex count mismatch",1742);
                        Points posed;
                        for (std::size_t i = 0; i < rest.size(); ++i)
                            posed.append(contracts::Point2{rest[i][0] + offsets[i].x, rest[i][1] + offsets[i].y});
                        keys.append(objectEvidence({{stringLiteralEvidence("parameter"), core::settings::SettingsString(name)}, {stringLiteralEvidence("key"), array({x, y})}, {stringLiteralEvidence("points"), ngRigPointsJson(posed)}}));
                        ++keyCount;
                    }
                for (std::size_t y = 0, ny = std::max(std::size_t(1), parameter->axisPoints[1].size() - 1); y < ny; ++y)
                    for (std::size_t x = 0, nx = parameter->axisPoints[0].size() - 1; x < nx; ++x)
                        for (float fx : {.25f, .5f, .75f})
                            for (float fy : parameter->isVec2 ? std::initializer_list<float>{.25f, .5f, .75f}
                                : std::initializer_list<float>{0.f}) {
                                auto offsets = binding->interpolate({static_cast<unsigned>(x), static_cast<unsigned>(y)}, {fx, fy}).vertexOffsets;
                                enforceNative(offsets.size() == rest.size(),"Saved intermediate angle vertex count mismatch",1754);
                                Points posed;
                                for (std::size_t i = 0; i < rest.size(); ++i)
                                    posed.append(contracts::Point2{rest[i][0] + offsets[i].x, rest[i][1] + offsets[i].y});
                                keys.append(objectEvidence({{stringLiteralEvidence("parameter"), core::settings::SettingsString(name)}, {stringLiteralEvidence("key"), array({static_cast<float>(x) + fx, static_cast<float>(y) + fy})},
                                    {stringLiteralEvidence("points"), ngRigPointsJson(posed)}, {stringLiteralEvidence("intermediate"), true}}));
                                ++intermediateCount;
                            }
            }
            for (const auto& pair : std::array<std::array<compat::RetainedString, 2>, 2>{{
                {compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"),
                    compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch")},
                {compat::RetainedString::fromStaticLiteral("Face::Roll"),
                    compat::RetainedString::fromStaticLiteral("Body::Roll")}}})
                for (float a : {-1.f, 0.f, 1.f}) for (float b : {-1.f, 0.f, 1.f}) {
                    Points posed; for (auto p : rest) posed.append(p);
                    for (std::size_t j = 0; j < pair.size(); ++j) {
                        auto parameter = parameterByName(puppet, pair[j]);
                        auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(grid, "deform"));
                        const float value = j == 0 ? a : b;
                        const auto coordinate = parameter->mapAxis(0, value);
                        std::size_t index = 0;
                        while (index + 2 < parameter->axisPoints[0].size() &&
                                coordinate > readNativeArray(parameter->axisPoints[0],index+1,1771)) ++index;
                        const auto lower=readNativeArray(parameter->axisPoints[0],index,1772);
                        const auto numerator=coordinate-lower;
                        const auto upper=readNativeArray(parameter->axisPoints[0],index+1,1773);
                        const auto repeatedLower=readNativeArray(parameter->axisPoints[0],index,1773);
                        const float fraction=numerator/(upper-repeatedLower);
                        const unsigned row = parameter->isVec2
                            ? static_cast<unsigned>(parameter->getClosestAxisPointIndex(1, parameter->mapAxis(1, 0))) : 0;
                        auto offsets = binding->interpolate({static_cast<unsigned>(index), row}, {fraction, 0}).vertexOffsets;
                        enforceNative(offsets.size() == posed.size(),"Combined angle sample vertex count mismatch",1776);
                        for (std::size_t i = 0; i < offsets.size(); ++i) {
                            posed[i][0] += offsets[i].x; posed[i][1] += offsets[i].y;
                        }
                    }
                    keys.append(objectEvidence({{stringLiteralEvidence("parameter"), core::settings::SettingsString(nativeText({pair[0].view()," + ",pair[1].view()}))}, {stringLiteralEvidence("key"), array({a, b})},
                        {stringLiteralEvidence("points"), ngRigPointsJson(posed)}, {stringLiteralEvidence("combined"), true}}));
                    ++intermediateCount;
                }
            grids.append(objectEvidence({{stringLiteralEvidence("part"), field(target, "part")}, {stringLiteralEvidence("columns"), field(target, "xs").sourceArrayValue().size()},
                {stringLiteralEvidence("rest"), ngRigPointsJson(rest)}, {stringLiteralEvidence("keys"), keys}, {stringLiteralEvidence("depth"), numberArray(mapped->copyDepths())}}));
        });
    }
    for (auto mechanism : field(state, "controls", "mechanisms").sourceArrayValue())
        for (auto operation : field(mechanism, "operations").sourceArrayValue()) {
            ngRigCheckpoint(task);
            task->runOnMainThread([&] {
                auto puppet = project::incActivePuppet();
                auto parameter = parameterByName(puppet, nativeString(field(mechanism, "name")));
                auto node = puppet->findNodeById(uuid(field(operation, "part")));
                auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(node, "deform"));
                enforceNative(bool(binding),"Saved local control binding is missing",1795);
                for (auto key : field(operation, "keys").sourceArrayValue()) {
                    auto index = ngRigNumbers(field(key, "key"));
                    const Vec2u kp{static_cast<unsigned>(readNativeArray(index,0,1797)),
                        static_cast<unsigned>(readNativeArray(index,1,1797))};
                    enforceNative(binding->isSet(kp),"Saved local control key is missing",1798);
                    auto expected = ngRigNumbers(field(key, "offsets")); auto actual = binding->getValue(kp).vertexOffsets;
                    enforceNative(actual.size() * 2 == expected.size(),"Saved local control offsets have changed",1800);
                    for (std::size_t i = 0; i < actual.size(); ++i)
                        enforceNative(std::abs(actual[i].x - expected[i * 2]) < 1e-4 * (1 + std::abs(expected[i * 2])) &&
                            std::abs(actual[i].y - expected[i * 2 + 1]) < 1e-4 * (1 + std::abs(expected[i * 2 + 1])),"Saved local control value mismatch",1801);
                }
                auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node);
                enforceNative(bool(part),"Local control target is not a Part",1805);
                core::settings::SettingsJsonArray samples;
                for (std::size_t y = 0, ny = std::max(std::size_t(1), parameter->axisPoints[1].size() - 1); y < ny; ++y)
                    for (std::size_t x = 0, nx = parameter->axisPoints[0].size() - 1; x < nx; ++x)
                        for (float fx : {.25f, .5f, .75f})
                            for (float fy : parameter->isVec2 ? std::initializer_list<float>{.25f, .5f, .75f}
                                : std::initializer_list<float>{0.f}) {
                                auto value = binding->interpolate({static_cast<unsigned>(x), static_cast<unsigned>(y)}, {fx, fy}).vertexOffsets;
                                core::settings::SettingsJsonArray offsets;
                                for (auto p : value) { offsets.append(Json(double(p.x))); offsets.append(Json(double(p.y))); }
                                samples.append(objectEvidence({{stringLiteralEvidence("key"), array({static_cast<float>(x) + fx, static_cast<float>(y) + fy})}, {stringLiteralEvidence("offsets"), offsets}}));
                                ++intermediateCount;
                            }
                localPoses.append(objectEvidence({{stringLiteralEvidence("part"), field(operation, "part")}, {stringLiteralEvidence("parameter"), field(mechanism, "name")},
                    {stringLiteralEvidence("mapping"), textureMapping(part)}, {stringLiteralEvidence("samples"), samples}}));
            });
        }
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        for (auto headKeys = get(state, "native_head_keys", absentArray()); auto key : headKeys.sourceArrayValue()) {
            auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
            auto grid = puppet->findNodeById(uuid(field(key, "grid")));
            auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(grid, "deform"));
            auto indices = ngRigNumbers(field(key, "key")), expected = ngRigNumbers(field(key, "offsets"));
            auto actual = binding->getValue({static_cast<unsigned>(readNativeArray(indices,0,1827)),
                static_cast<unsigned>(readNativeArray(indices,1,1827))}).vertexOffsets;
            enforceNative(actual.size() * 2 == expected.size(),"Saved native head bake vertex count changed",1828);
            for (std::size_t i = 0; i < actual.size(); ++i)
                enforceNative(std::abs(actual[i].x - expected[i * 2]) < .0003 && std::abs(actual[i].y - expected[i * 2 + 1]) < .0003,"Saved native head bake changed after shape correction",1829);
        }
        if (auto feet = state.sourceObjectRef().if_contains("fixed_foot_corrections"))
            for (auto operation : field(*feet, "operations").sourceArrayValue()) {
                auto node = puppet->findNodeById(uuid(field(operation, "grid")));
                auto parameter = parameterByName(puppet, nativeString(field(operation, "parameter")));
                auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(node, "deform"));
                auto indices = ngRigNumbers(field(operation, "key")), expected = ngRigNumbers(field(operation, "values"));
                const Vec2u kp{static_cast<unsigned>(readNativeArray(indices,0,1837)),
                    static_cast<unsigned>(readNativeArray(indices,1,1837))};
                enforceNative(binding && binding->isSet(kp),"Saved fixed-foot key is missing",1838);
                auto actual = binding->getValue(kp).vertexOffsets;
                enforceNative(actual.size() * 2 == expected.size(),"Saved fixed-foot vertex count changed",1840);
                for (std::size_t i = 0; i < actual.size(); ++i)
                    enforceNative(std::abs(actual[i].x - expected[i * 2]) < .0003 && std::abs(actual[i].y - expected[i * 2 + 1]) < .0003,"Saved fixed-foot field differs from the compiled correction",1841);
            }
        auto corrections = get(state, "shape_corrections", objectEvidence({{stringLiteralEvidence("applicable"), false}}));
        if (field(corrections, "applicable").as_bool()) {
            Json unsignedReport=corrections.sourceObjectRef().duplicate(); unsignedReport.sourceObjectRef().erase("content_sha256");
            enforceNative(ngRigDigest(unsignedReport).view() == field(state, "shape_corrections_sha256").as_string().view(),"Saved shape correction report ownership mismatch",1847);
            auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
            for (auto operation : field(corrections, "operations").sourceArrayValue()) {
                bool bound = false;
                for (auto id : field(corrections, "bound_targets").sourceArrayValue())
                    if (ngRigUnsigned(id) == ngRigUnsigned(field(operation, "part"))) bound = true;
                if (!bound) continue;
                auto part = puppet->findNodeById(uuid(field(operation, "part")));
                auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getBinding(part, "deform"));
                auto index = ngRigNumbers(field(operation, "key")), expected = ngRigNumbers(field(operation, "values"));
                const Vec2u kp{static_cast<unsigned>(readNativeArray(index,0,1858)),
                    static_cast<unsigned>(readNativeArray(index,1,1858))};
                enforceNative(binding && binding->isSet(kp),"Saved cheek residual key is missing",1859);
                auto actual = binding->getValue(kp).vertexOffsets;
                enforceNative(actual.size() * 2 == expected.size(),"Saved cheek residual vertex count changed",1861);
                for (std::size_t i = 0; i < actual.size(); ++i)
                    enforceNative(std::abs(actual[i].x - expected[i * 2]) < .0003 && std::abs(actual[i].y - expected[i * 2 + 1]) < .0003,"Saved cheek residual value changed",1862);
            }
        }
        for (auto pairs = get(state, "shoulder_pairs", absentArray()); auto pair : pairs.sourceArrayValue()) {
            if (stringGet(pair, "status") != "applied_readback_verified") continue;
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(field(pair, "source"))));
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(field(pair, "target"))));
            enforceNative(source && target,"Saved shoulder link Part is missing",1870);
            bool found = false;
            const auto links=source->weldedLinks;
            for (auto link : links) if (link.target.lock() == target) {
                auto expected = ngRigNumbers(field(pair, "indices"));
                enforceNativeLazy(link.indices.size() == expected.size() && link.weight == 0,[&] {
                    const std::array<core::translation::TypedFormatArgument, 5> args{
                        core::translation::ngFormatArgument(source->getUUID()), core::translation::ngFormatArgument(target->getUUID()),
                        core::translation::ngFormatArgument(expected.size()), core::translation::ngFormatArgument(link.indices.size()),
                        core::translation::ngFormatArgument(link.weight)};
                    return core::translation::ngFormatTyped(
                        "Saved shoulder link has changed: source=%s target=%s expected_vertices=%s actual_vertices=%s weight=%s",args);
                },1874);
                for (std::size_t i = 0; i < link.indices.size(); ++i)
                    enforceNative(link.indices[i] == expected[i],"Saved shoulder correspondence has changed",1878);
                found = true;
            }
            enforceNative(found,"Saved shoulder welding link is missing",1881);
        }
        enforceNative(editorSignature(puppet) == snapshotSignature,"Model changed during saved rig snapshots",1883);
    });
    for (auto grid : grids) {
        const auto depth = ngRigNumbers(field(grid, "depth"));
        auto rest = ngRigPoints(field(grid, "rest"));
        geometry::ngRigValidateGrid(rest, static_cast<std::size_t>(ngRigNumber(field(grid, "columns"))), 0);
        for (auto key : field(grid, "keys").sourceArrayValue()) {
            ngRigCheckpoint(task);
            try {
                const auto points = ngRigPoints(field(key, "points"));
                const auto columns = static_cast<std::size_t>(ngRigNumber(field(grid, "columns")));
                geometry::ngRigValidateGrid(points, columns, .02, rest);
            } catch (const std::bad_alloc&) { throw; }
            catch (const compat::ThrowableException& error) {
                findings.append(objectEvidence({{stringLiteralEvidence("part"), field(grid, "part")}, {stringLiteralEvidence("parameter"), field(key, "parameter")},
                    {stringLiteralEvidence("key"), field(key, "key")}, {stringLiteralEvidence("message"), exceptionMessage(error)}}));
            }
        }
    }
    for (auto local : localPoses) for (auto sample : field(local, "samples").sourceArrayValue()) {
        ngRigCheckpoint(task);
        try {
            const double ratio = [&] {
                const auto vertices = ngRigPoints(field(local, "mapping", "vertices"));
                const auto triangles = field(local, "mapping", "triangles");
                const auto offsets = ngRigNumbers(field(sample, "offsets"));
                return geometry::ngRigTriangleMinimumRatio(vertices, triangles, offsets);
            }();
            if (!nativeString(field(local, "parameter")).view().ends_with("::Blink"))
                enforceNative(ratio >= .002,"Intermediate local control folded a triangle",1901);
        } catch (const std::bad_alloc&) { throw; }
        catch (const compat::ThrowableException& error) {
            findings.append(objectEvidence({{stringLiteralEvidence("part"), field(local, "part")}, {stringLiteralEvidence("parameter"), field(local, "parameter")},
                {stringLiteralEvidence("key"), field(sample, "key")}, {stringLiteralEvidence("message"), exceptionMessage(error)}}));
        }
    }
    auto report = objectEvidence({{stringLiteralEvidence("schema_version"), "rig-saved-validation-d/1"}, {stringLiteralEvidence("passed"), findings.empty()},
        {stringLiteralEvidence("bones"), boneCount}, {stringLiteralEvidence("baked_keys"), keyCount}, {stringLiteralEvidence("intermediate_samples"), intermediateCount},
        {stringLiteralEvidence("numerical_findings"), findings}, {stringLiteralEvidence("source_sha256"), field(state, "source_sha256")},
        {stringLiteralEvidence("source_uv_readback"), sourceFrames}, {stringLiteralEvidence("readback_verified"), true},
        {stringLiteralEvidence("program_sha256"), field(program, "content_sha256")}, {stringLiteralEvidence("model_sha256"), field(state, "model_sha256")},
        {stringLiteralEvidence("visual_review_required"), true}});
    auto renders = renderValidation(state, task); assignNative(report, "renders", renders);
    if (field(renders, "applicable").as_bool() && !field(renders, "neutral_passed").as_bool())
        findings.append(objectEvidence({{stringLiteralEvidence("message"), "Neutral render differs from the source"}}));
    assignNative(report, "passed", findings.empty()); assignNative(report, "numerical_findings", findings);
    if (!findings.empty()) task->previewJson("validation-findings", report);
    return report;
}
}
