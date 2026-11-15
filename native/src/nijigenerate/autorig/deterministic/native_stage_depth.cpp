#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include "geometry.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/commands/binding/binding_commands.hpp>
#include <nijigenerate/commands/depth/bone_commands.hpp>
#include <nijigenerate/commands/depth/bone_key_projection.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <param/binding_impl.hpp>
#include <nodes/deformable.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
template<class T> Json numericArray(const compat::Slice<T>& values) {
    core::settings::SettingsJsonArray result;
    for (auto value : values) result.append(value);
    return result;
}
}
Json validateDepthInputs(Json state, Json program, Task task) {
    core::settings::SettingsJsonArray surfaces;
    if (stringGet(state, "kind", "humanoid") != "humanoid") {
        state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("depth_validation")) = objectEvidence({{stringLiteralEvidence("applicable"), false}, {stringLiteralEvidence("reason"), "Local rig has no depth surfaces"}});
        return state;
    }
    std::array<double, 4> bounds{INFINITY, INFINITY, -INFINITY, -INFINITY};
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(puppet->findNodeById(uuid(state.sourceIndexValue("rigRoot"))));
        enforceNative(bool(root),"Depth input root is missing",1470);
        for (auto target : state.sourceIndexValue("targets").sourceArrayValue()) {
            auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(puppet->findNodeById(uuid(target.sourceIndexValue("grid"))));
            auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid);
            enforceNative(grid && mapped,"Depth input grid is missing",1474);
            auto raw = mapped->copyDepths(); auto expected = ngRigNumbers(target.sourceIndexValue("depth"));
            enforceNative(raw.size() == expected.size(),"Stored depth length differs from the compiled program",1476);
            for (std::size_t i = 0; i < raw.size(); ++i)
                enforceNative(std::isfinite(raw[i]) && std::abs(raw[i] - expected[i]) <= 1e-7,"Stored grid depth differs from the compiled program",1477);
            const auto rootInverse = core::math::source_matrix::inverse(root->transform().matrix());
            const auto gridMatrix = grid->transform().matrix();
            const auto transform = rootInverse * gridMatrix;
            compat::Slice<double> zs;
            for (auto vertex : grid->getVertices()) {
                auto point = core::math::ngTransformHomogeneousPoint(transform, {vertex.x, vertex.y, 0});
                bounds[0] = dmin(bounds[0], point.x); bounds[1] = dmin(bounds[1], point.y);
                bounds[2] = dmax(bounds[2], point.x); bounds[3] = dmax(bounds[3], point.y);
                zs.append(point.z);
            }
            core::settings::SettingsJsonArray sources;
            bool bound = false;
            const auto bindings = root->bindings;
            for (auto binding : bindings) if (binding.targetUuid == grid->getUUID()) {
                bound = true;
                for (auto bone : binding.sourceBoneUuids) {
                    auto settings = binding.sourceSetting(bone);
                    enforceNative(std::isfinite(settings.depthScale) && std::isfinite(settings.depthOffset) &&
                        std::isfinite(settings.rotation) && std::isfinite(settings.weight),"Nonfinite depth source settings",1493);
                    sources.append(objectEvidence({{stringLiteralEvidence("bone"), bone}, {stringLiteralEvidence("depth_scale"), settings.depthScale},
                        {stringLiteralEvidence("depth_offset"), settings.depthOffset}, {stringLiteralEvidence("rotation"), settings.rotation}, {stringLiteralEvidence("weight"), settings.weight}}));
                }
            }
            enforceNative(bound && !sources.empty(),"Depth grid has no bone sources",1500);
            surfaces.append(objectEvidence({{stringLiteralEvidence("grid"), target.sourceIndexValue("grid")}, {stringLiteralEvidence("raw_depth"), numericArray(raw)},
                {stringLiteralEvidence("vertex_z"), numericArray(zs)}, {stringLiteralEvidence("sources"), sources}}));
        }
    });
    const double observedScale = dmax(1., dmax(bounds[2] - bounds[0], bounds[3] - bounds[1]) * .42 / 2.9);
    const double expectedScale = ngRigNumber(program.sourceIndexValue("native_depth_scale"));
    enforceNative(std::abs(observedScale - expectedScale) <= dmax(.001, observedScale * 1e-5),"Observed native depth unit differs from the compiled scale",1507);
    for (auto& surface : surfaces) {
        auto raw = ngRigNumbers(surface.sourceIndexValue("raw_depth")), zs = ngRigNumbers(surface.sourceIndexValue("vertex_z"));
        for (auto& source : surface.sourceIndexValue("sources").sourceArrayValue()) {
            double low = INFINITY, high = -INFINITY;
            for (std::size_t i = 0; i < raw.size(); ++i) {
                const auto scaled=raw[i]*ngRigNumber(source.sourceIndexValue("depth_scale"));
                const auto shifted=scaled+ngRigNumber(source.sourceIndexValue("depth_offset"));
                const auto modelDepth=shifted*observedScale;
                double effective=modelDepth+readNativeArray(zs,i,1515);
                enforceNative(std::isfinite(effective),"Nonfinite effective depth",1516);
                low = dmin(low, effective); high = dmax(high, effective);
            }
            source.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("effective_z_range_model")) = array({low, high});
            source.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("effective_relief_model")) = high - low;
        }
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("depth_validation")) = objectEvidence({{stringLiteralEvidence("applicable"), true}, {stringLiteralEvidence("depth_units_verified"), true},
        {stringLiteralEvidence("observed_scale"), observedScale}, {stringLiteralEvidence("compiled_scale"), expectedScale}, {stringLiteralEvidence("surfaces"), surfaces},
        {stringLiteralEvidence("visual_volume_accepted"), false}});
    return state;
}

Json bakeAngles(Json state, Task task) {
    if (stringGet(state, "kind", "humanoid") != "humanoid") return state;
    const std::array<compat::RetainedString, 4> names{
        compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"),
        compat::RetainedString::fromStaticLiteral("Face::Roll"),
        compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
        compat::RetainedString::fromStaticLiteral("Body::Roll")};
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        NativeIntegerMap<std::uint32_t, bool> grids;
        for (auto target : state.sourceIndexValue("targets").sourceArrayValue()) grids[uuid(target.sourceIndexValue("grid"))]=true;
        for (auto source : get(state, "source_group_grids", core::settings::SettingsJsonArray{}).sourceArrayValue())
            grids[uuid(source.sourceIndexValue("grid"))]=true;
        for (const auto& name : names) {
            auto parameter = parameterByName(puppet, name);
            enforceNativeLazy(bool(parameter),[&] { return nativeText({"Standard rig parameter missing: ",name.view()}); },1541);
            auto context = editorContext(); context->parameters({parameter});
            const auto bindings = parameter->bindings;
            for (auto binding : bindings)
                if (binding->getNameDescriptor().view() == "deform" &&
                    grids.contains(compat::ngRequireObject(binding->getTarget().getResource())->getUUID())) {
                    auto active = context->activeBindings(); active.append(binding); context->activeBindings(active);
                }
            if (!context->activeBindings().empty())
                commandSource(
                    [&] {
                        return createNativeObject<commands::binding::RemoveBindingCommand>();
                    },
                    [&] {
                        return context;
                    });
        }
        const auto parameters = puppet->parameters;
        for (auto parameter : parameters) parameter->value = parameter->defaults;
        puppet->update();
    });
    core::settings::SettingsJsonArray headKeys, legRollKeys, orientationCorrections;
    for (const auto& name : names) {
        std::size_t nx = 0, ny = 0;
        task->runOnMainThread([&] {
            auto parameter = parameterByName(project::incActivePuppet(), name);
            enforceNativeLazy(bool(parameter),[&] { return nativeText({"Standard rig parameter missing: ",name.view()}); },1557);
            nx = parameter->axisPoints[0].size(); ny = parameter->axisPoints[1].size();
        });
        for (std::size_t x = 0; x < nx; ++x) for (std::size_t y = 0; y < ny; ++y) {
            ngRigCheckpoint(task);
            std::shared_ptr<commands::CommandResult> result;
            task->runOnMainThread([&] {
                auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, name);
                const auto parameters = puppet->parameters;
                for (auto other : parameters) other->value = other->defaults;
                parameter->value = parameter->unmapValue({readNativeArray(parameter->axisPoints[0],x,1566),
                    readNativeArray(parameter->axisPoints[1],y,1566)});
                puppet->update();
                auto apply = createNativeObject<commands::depth::bone::ApplyDepthBoneDeformCommand>();
                apply->root = puppet->findNodeById(uuid(state.sourceIndexValue("rigRoot")));
                auto context = editorContext(); context->armedParameters({parameter}); result = commandSource(
                    [&] {
                        return apply;
                    },
                    [&] {
                        return context;
                    });
            });
            result = result->waitForCompletion(); enforceNativeLazy(result->succeeded,[&] { return result->message; },1574);
            settleDepthRefresh(task);
            if (name.view() == "Body::Yaw-Pitch" || name.view() == "Face::Yaw-Pitch") task->runOnMainThread([&] {
                auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, name);
                NativeIntegerMap<std::uint32_t,bool> seen;
                for (auto target : state.sourceIndexValue("targets").sourceArrayValue()) {
                    const auto gridId = uuid(target.sourceIndexValue("grid"));
                    const auto owner = nativeString(target.sourceIndexValue("owner")).view();
                    if (owner != (name.view()=="Body::Yaw-Pitch"?"torso":"head") || seen.contains(gridId)) continue;
                    seen[gridId] = true;
                    auto node = puppet->findNodeById(gridId);
                    auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
                        parameter->getBinding(node, "deform"));
                    const nicxlive::core::param::Vec2u kp{static_cast<unsigned>(x),static_cast<unsigned>(y)};
                    enforceNative(binding && binding->isSet(kp), "Missing native surface bake", 1575);
                    const auto xs = ngRigNumbers(target.sourceIndexValue("xs"));
                    const auto ys = ngRigNumbers(target.sourceIndexValue("ys"));
                    compat::Slice<Point2> rest, posed;
                    const auto baked = binding->getValue(kp).vertexOffsets;
                    enforceNative(baked.size() == xs.size()*ys.size(), "Native surface grid size mismatch", 1575);
                    std::size_t index = 0;
                    for (const auto gy : ys) for (const auto gx : xs) {
                        rest.append(Point2{gx,gy});
                        posed.append(Point2{gx+baked[index].x,gy+baked[index].y}); ++index;
                    }
                    double maximum = 0.;
                    try {
                        maximum = geometry::ngRigRepairGridOrientation(posed, xs.size(), rest);
                    } catch (const compat::ThrowableException& error) {
                        enforceNativeLazy(false, [&] {
                            return nativeText({error.msg.view(), ": ", name.view(), " / ",
                                node->getName().view(), " key=(", nativeNumber(x).view(), ",",
                                nativeNumber(y).view(), ")"});
                        }, 1575);
                    }
                    if (maximum == 0) continue;
                    compat::Slice<float> offsets;
                    for (std::size_t i = 0; i < rest.size(); ++i) {
                        offsets.append(static_cast<float>(posed[i][0]-rest[i][0]));
                        offsets.append(static_cast<float>(posed[i][1]-rest[i][1]));
                    }
                    auto context = editorContext({node}); context->parameters({parameter});
                    context->keyPoint({kp.x,kp.y}); context->hasExplicitKeyPoint(true);
                    commandSource([&] {
                        return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                            compat::RetainedString::fromStaticLiteral("deform"), offsets, true, false);
                    }, [&] { return context; });
                    orientationCorrections.append(objectEvidence({{stringLiteralEvidence("grid"),gridId},
                        {stringLiteralEvidence("parameter"),core::settings::SettingsString(name)},
                        {stringLiteralEvidence("key"),array({x,y})}, {stringLiteralEvidence("maximum_shift"),maximum}}));
                }
            });
            if (name.view() == "Body::Roll") task->runOnMainThread([&] {
                auto puppet=project::incActivePuppet();auto parameter=parameterByName(puppet,name);
                NativeIntegerMap<std::uint32_t,bool> seen;
                for(auto target:state.sourceIndexValue("targets").sourceArrayValue()) {
                    const auto gridId=uuid(target.sourceIndexValue("grid"));
                    if(!nativeString(target.sourceIndexValue("owner")).view().starts_with("leg:")||seen.contains(gridId))continue;
                    seen[gridId]=true;
                    auto binding=std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
                        parameter->getBinding(puppet->findNodeById(gridId),"deform"));
                    const nicxlive::core::param::Vec2u kp{static_cast<unsigned>(x),static_cast<unsigned>(y)};
                    enforceNative(binding&&binding->isSet(kp),"Missing native leg roll bake",1575);
                    compat::Slice<double> offsets;
                    for(auto offset:binding->getValue(kp).vertexOffsets){offsets.append(offset.x);offsets.append(offset.y);}
                    legRollKeys.append(objectEvidence({{stringLiteralEvidence("grid"),gridId},
                        {stringLiteralEvidence("key"),array({x,y})},{stringLiteralEvidence("offsets"),numericArray(offsets)}}));
                }
            });
            if (name.view() == "Face::Yaw-Pitch") task->runOnMainThread([&] {
                auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, name);
                auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(puppet->findNodeById(uuid(state.sourceIndexValue("rigRoot"))));
                std::uint64_t head = 0;
                for (auto bone : root->depthBones()) if (bone->boneId.view() == "Head") head = bone->getUUID();
                compat::Vec2u index{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
                auto projection = commands::depth::bone::ngDepthBoneKeyProjection(root, head, parameter, index);
                const nicxlive::core::param::Vec2u runtimeIndex{index.x, index.y};
                NativeIntegerMap<std::uint32_t, bool> seen;
                for (auto target : state.sourceIndexValue("targets").sourceArrayValue())
                    if (nativeString(target.sourceIndexValue("owner")).view() == "head" && !seen.contains(uuid(target.sourceIndexValue("grid")))) {
                        auto grid = puppet->findNodeById(uuid(target.sourceIndexValue("grid")));
                        auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
                            parameter->getBinding(grid, "deform"));
                        enforceNativeLazy(binding && binding->isSet(runtimeIndex),[&] {
                            return nativeText({"Face bake readback is missing: grid=",nativeNumber(grid->getUUID()).view(),
                                " key=(",nativeNumber(x).view(),",",nativeNumber(y).view(),")"});
                        },1587);
                        compat::Slice<double> offsets;
                        for (auto offset : binding->getValue(runtimeIndex).vertexOffsets) { offsets.append(offset.x); offsets.append(offset.y); }
                        auto value = parameter->unmapValue({readNativeArray(parameter->axisPoints[0],x,1591),
                            readNativeArray(parameter->axisPoints[1],y,1591)});
                        headKeys.append(objectEvidence({{stringLiteralEvidence("grid"), target.sourceIndexValue("grid")}, {stringLiteralEvidence("key"), array({x, y})},
                            {stringLiteralEvidence("value"), array({value.x, value.y})}, {stringLiteralEvidence("offsets"), numericArray(offsets)},
                            {stringLiteralEvidence("projection"), numericArray(projection)}}));
                        seen[uuid(target.sourceIndexValue("grid"))]=true;
                    }
            });
        }
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto parameter = parameterByName(puppet, name);
            if (name.view()=="Face::Yaw-Pitch" || name.view()=="Body::Yaw-Pitch") {
                NativeIntegerMap<std::uint32_t,bool> seen;
                for(auto target:state.sourceIndexValue("targets").sourceArrayValue()) {
                    const auto gridId=uuid(target.sourceIndexValue("grid"));
                    const auto owner=nativeString(target.sourceIndexValue("owner")).view();
                    if(owner!=(name.view()=="Face::Yaw-Pitch"?"head":"torso")||seen.contains(gridId))continue;
                    seen[gridId]=true;
                    auto node=puppet->findNodeById(gridId);
                    auto binding=std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
                        parameter->getBinding(node,"deform"));
                    const auto xs=ngRigNumbers(target.sourceIndexValue("xs"));
                    const auto ys=ngRigNumbers(target.sourceIndexValue("ys"));
                    compat::Slice<Point2> rest;
                    for(auto gy:ys)for(auto gx:xs)rest.append(Point2{gx,gy});
                    compat::Slice<compat::Slice<Point2>> keys;
                    for(std::size_t x=0;x<nx;++x)for(std::size_t y=0;y<ny;++y) {
                        auto offsets=binding->getValue({static_cast<unsigned>(x),static_cast<unsigned>(y)}).vertexOffsets;
                        compat::Slice<Point2> posed;
                        for(std::size_t i=0;i<rest.size();++i)posed.append(Point2{rest[i][0]+offsets[i].x,rest[i][1]+offsets[i].y});
                        keys.append(posed);
                    }
                    const auto maximum=geometry::ngRigRepairGridInterpolation(keys,nx,ny,xs.size(),rest);
                    if(maximum==0)continue;
                    for(std::size_t x=0;x<nx;++x)for(std::size_t y=0;y<ny;++y) {
                        compat::Slice<float> offsets;
                        for(std::size_t i=0;i<rest.size();++i) {
                            offsets.append(static_cast<float>(keys[x*ny+y][i][0]-rest[i][0]));
                            offsets.append(static_cast<float>(keys[x*ny+y][i][1]-rest[i][1]));
                        }
                        auto context=editorContext({node});context->parameters({parameter});
                        context->keyPoint({static_cast<unsigned>(x),static_cast<unsigned>(y)});context->hasExplicitKeyPoint(true);
                        commandSource([&] {return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                            compat::RetainedString::fromStaticLiteral("deform"),offsets,true,false);},[&] {return context;});
                        // The editor normalizes float offsets through its mesh. Record the
                        // actual binding, as the ordinary bake does, for exact later checks.
                        const auto actual=binding->getValue({static_cast<unsigned>(x),static_cast<unsigned>(y)}).vertexOffsets;
                        compat::Slice<double> recorded;
                        enforceNative(actual.size()*2==offsets.size(),"Interpolation correction readback size differs",1575);
                        for(std::size_t i=0;i<actual.size();++i) {
                            enforceNative(std::abs(actual[i].x-offsets[i*2])<.0003&&
                                std::abs(actual[i].y-offsets[i*2+1])<.0003,"Interpolation correction readback differs",1575);
                            recorded.append(actual[i].x);recorded.append(actual[i].y);
                        }
                        if(name.view()=="Face::Yaw-Pitch")for(auto& key:headKeys)
                            if(uuid(key.sourceIndexValue("grid"))==gridId) {
                                auto point=ngRigNumbers(key.sourceIndexValue("key"));
                                if(point[0]==x&&point[1]==y)key.sourceIndexAssignSlot("offsets")=numericArray(recorded);
                            }
                    }
                    orientationCorrections.append(objectEvidence({{stringLiteralEvidence("grid"),gridId},
                        {stringLiteralEvidence("parameter"),core::settings::SettingsString(name)},
                        {stringLiteralEvidence("interpolation"),true},{stringLiteralEvidence("maximum_shift"),maximum}}));
                }
            }
            parameter->value = {0, 0}; puppet->update();
        });
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("native_leg_roll_keys")) = legRollKeys;
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("native_orientation_corrections")) = orientationCorrections;
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("native_head_keys")) = headKeys;
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("depth_angle_program_sha256")) = core::settings::SettingsString(ngRigDigest(Json(headKeys)));
    return state;
}
}
