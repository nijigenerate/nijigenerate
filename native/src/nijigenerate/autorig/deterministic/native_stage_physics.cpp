#include "native_stage.hpp"
#include "physics.hpp"
#include "review.hpp"
#include "detail/native_stage.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/commands/parameter/param_commands.hpp>
#include <nijigenerate/commands/parameter/paramedit_commands.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_complete.hpp>
#include <nijigenerate/commands/node/simplephysics_config_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_parameter_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <param/binding_impl.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <cmath>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
using namespace physics;

Json applyPhysics(Json state, Json program, Task task) {
    if (stringGet(program, "kind", "humanoid") != "humanoid") return state;
    PhysicsAssets assets;
    auto structure = ngRigPhysicsStructure(state, program, assets, task);
    PhysicsMeshes meshes;
    NativeIntegerMap<std::uint64_t, PhysicsAffineTransform> transforms;
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        const auto parameters = puppet->parameters;
        for (auto parameter : parameters) parameter->value = parameter->defaults;
        puppet->enableDrivers = false;
        puppet->update();
        const auto world = [&](auto&& self, NodeRef node) -> PhysicsAffineTransform {
            if (node == puppet->root) return {1., 0., 0., 0., 1., 0.};
            if (auto found = transforms.find(node->getUUID()); found != transforms.end()) return found->second;
            auto local = node->localTransform;
            enforceNative(local.rotation.x == 0 && local.rotation.y == 0 && !node->pinToMesh,"Unsupported neutral physics coordinate frame",1937);
            const double angle = local.rotation.z, c = std::cos(angle), s = std::sin(angle);
            PhysicsAffineTransform result{c * local.scale.x, -s * local.scale.y, local.translation.x,
                s * local.scale.x, c * local.scale.y, local.translation.y};
            if (node->parentPtr() && !node->lockToRoot) {
                auto parent = self(self,node->parentPtr());
                auto x = ngRigPhysicsTransform(parent, {result[0], result[3]}, true);
                auto y = ngRigPhysicsTransform(parent, {result[1], result[4]}, true);
                auto origin = ngRigPhysicsTransform(parent, {result[2], result[5]});
                result = {x[0], y[0], origin[0], x[1], y[1], origin[1]};
            }
            transforms[node->getUUID()] = result;
            return result;
        };
        for (auto [id, asset] : assets) {
            auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                puppet->findNodeById(static_cast<std::uint32_t>(id)));
            enforceNative(part&&[&] {const auto origin=part->getMesh().origin;return origin.x==0&&origin.y==0;}(),
                "Physics requires a zero-origin Part mesh",1954);
            RigPhysicsMesh mesh;
            for (auto vertex : part->getMesh().vertices) mesh.vertices.append(Point2{vertex.x, vertex.y});
            for (auto uv : part->getMesh().uvs) mesh.uv.append(Point2{uv.x, uv.y});
            for (auto index : part->getMesh().indices) mesh.indices.append(index);
            mesh.toModel = world(world,part);
            meshes[id] = mesh;
        }
        auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(
            puppet->findNodeById(uuid(state.sourceIndexValue("rigRoot"))));
        enforceNative(bool(root),"Physics anatomical rig is missing",1962);
        for (auto bone : root->depthBones()) world(world,bone);
    });
    auto plan = ngRigCompilePhysicsOperations(structure, assets, meshes, task);
    auto before = reviewModel(task), policy = plan.sourceIndexValue("policy");
    core::settings::SettingsJsonObject identities;
    for (auto group : plan.sourceIndexValue("groups").sourceArrayValue()) {
        std::uint32_t parameterId = 0;
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            const auto parameters = puppet->parameters;
            for (auto parameter : parameters)
                enforceNativeLazy(parameter->getName().view() != group.sourceIndexValue("name").as_string().view(),[&] { return nativeText({"Unowned Physics parameter name collision: ",nativeString(group.sourceIndexValue("name")).view()}); },1972);
            auto created = std::dynamic_pointer_cast<commands::CreateResult<ParameterRef>>(commandSource(
                [&] {
                    return createNativeObject<commands::parameter::param::Add2DParameterCommand>(-1, 1);
                },
                [&] {
                    return editorContext();
                }));
            enforceNative(created && created->created.size() == 1,"Could not create physics parameter",1975);
            auto parameter = created->created[0];
            parameter->setName(nativeString(group.sourceIndexValue("name")).toNullableString());
            parameter->min = {-1, -1}; parameter->max = {1, 1};
            parameter->axisPoints[0] = {0.0f, 0.5f, 1.0f};
            parameter->axisPoints[1] = {0.0f, 0.5f, 1.0f};
            parameter->defaults = {0, 0}; parameter->value = parameter->defaults; parameterId = parameter->getUUID();
            auto context = editorContext(); context->parameters({parameter});
            context->parameterValue({0, 0}); context->hasParameterValue(true);
            commandSource(
                [&] {
                    return createNativeObject<commands::parameter::paramedit::SetParameterKeypointCommand>();
                },
                [&] {
                    return context;
                });
        });
        for (auto operation : group.sourceIndexValue("operations").sourceArrayValue()) {
            ngRigCheckpoint(task);
            auto offsets = nativeNumbers(operation.sourceIndexValue("values"));
            auto key = ngRigPoint(operation.sourceIndexValue("key"));
            task->runOnMainThread([&] {
                auto puppet = project::incActivePuppet();
                auto part = puppet->findNodeById(uuid(operation.sourceIndexValue("target")));
                auto parameter = puppet->findParameter(parameterId);
                auto context = editorContext({part}); context->parameters({parameter});
                context->hasExplicitKeyPoint(true);
                context->keyPoint({static_cast<std::uint32_t>(readNativeArray(key,0,1990) + 1),
                    static_cast<std::uint32_t>(readNativeArray(key,1,1990) + 1)});
                commandSource(
                    [&] {
                        return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                            compat::RetainedString::fromStaticLiteral("deform"), offsets, true);
                    },
                    [&] {
                        return context;
                    });
                auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
                    parameter->getBinding(part, "deform"));
                const auto keyPoint = context->keyPoint();
                const nicxlive::core::param::Vec2u runtimeKey{keyPoint.x, keyPoint.y};
                enforceNative(binding && binding->isSet(runtimeKey),"Missing physics key",1993);
                auto actual = binding->getValue(runtimeKey).vertexOffsets;
                enforceNative(actual.size() * 2 == offsets.size(),"Physics binding vertex count differs",1995);
                auto mesh = readNativeMap(meshes,ngRigUnsigned(operation.sourceIndexValue("target")),1996);
                for (std::size_t i = 0; i < actual.size(); ++i) for (std::size_t axis = 0; axis < 2; ++axis) {
                    const auto coordinate = static_cast<float>(std::abs(readNativeArray(mesh.vertices,i,1999)[axis]) + std::abs(offsets[i * 2 + axis]));
                    const auto tolerance = dmax(1e-4, 2. * (std::nextafter(coordinate, INFINITY) - coordinate));
                    enforceNative(std::abs((axis == 0 ? actual[i].x : actual[i].y) - offsets[i * 2 + axis]) <= tolerance,"Physics key readback differs",2001);
                }
            });
        }
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(puppet->findNodeById(uuid(state.sourceIndexValue("rigRoot"))));
            NodeRef parent;
            for (auto bone : root->depthBones()) if (bone->boneId.view() == nativeString(group.sourceIndexValue("parent_bone")).view()) parent = bone;
            enforceNative(bool(parent),"Physics support bone is missing",2010);
            auto instance = std::dynamic_pointer_cast<nicxlive::core::nodes::SimplePhysicsDriver>(createSourceNode(
                [&] {
                    return createNativeObject<commands::node::node::AddNodeCommandT<true>>(
                        compat::RetainedString::fromStaticLiteral("SimplePhysics"),compat::RetainedString::emptyLiteral());
                },
                [&] {
                    return editorContext({parent});
                }));
            enforceNative(bool(instance),"Could not create SimplePhysics",2012); instance->setName(nativeString(group.sourceIndexValue("name")).toNullableString());
            auto context = editorContext({instance});
            using namespace commands::inspector::apply_node;
            auto inspector = panels::inspector::ngCreateInspector<NodeApplyInspector>(
                panels::inspector::Selection<nicxlive::core::nodes::Node>{instance},
                panels::inspector::ModelEditSubMode::Layout);
            context->inspectors({inspector});
            const auto parentInverse = ngRigPhysicsInverse(readNativeMap(transforms,parent->getUUID(),2014));
            const auto fixed = ngRigPoint(group.sourceIndexValue("fixed"));
            const auto anchor = ngRigPhysicsTransform(parentInverse,fixed);
            auto x = createNativeObject<TranslationXCommand>(); x->value = static_cast<float>(anchor[0]); commandSource(
                [&] {
                    return x;
                },
                [&] {
                    return context;
                });
            auto y = createNativeObject<TranslationYCommand>(); y->value = static_cast<float>(anchor[1]); commandSource(
                [&] {
                    return y;
                },
                [&] {
                    return context;
                });
            using namespace commands::node::simplephysics;
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsParameterCommand>(puppet->findParameter(parameterId));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsModelTypeCommand>(nicxlive::core::nodes::PhysicsModel::SpringPendulum);
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsMapModeCommand>(nicxlive::core::nodes::ParamMapMode::XY);
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsLocalOnlyCommand>(false);
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsGravityCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("settings").sourceIndexValue("gravity"))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsLengthCommand>(static_cast<float>(ngRigNumber(group.sourceIndexValue("length"))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsFrequencyCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("profiles").sourceIndexValue(nativeString(group.sourceIndexValue("profile")).view()).sourceIndexValue("frequency"))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsAngleDampingCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("settings").sourceIndexValue("angle_damping"))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsLengthDampingCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("settings").sourceIndexValue("length_damping"))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsOutputScaleXCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("settings").sourceIndexValue("output_scale").sourceIndexValue(std::size_t(0)))));
                },
                [&] {
                    return context;
                });
            commandSource(
                [&] {
                    return createNativeObject<SetSimplePhysicsOutputScaleYCommand>(static_cast<float>(ngRigNumber(policy.sourceIndexValue("settings").sourceIndexValue("output_scale").sourceIndexValue(std::size_t(1)))));
                },
                [&] {
                    return context;
                });
            const auto identityKey = nativeString(group.sourceIndexValue("id"));
            const auto physicsId = instance->getUUID();
            const auto readback = serializedJson(instance);
            const auto identity = objectEvidence({{stringLiteralEvidence("parameter"), parameterId},
                {stringLiteralEvidence("simple_physics"), physicsId}, {stringLiteralEvidence("readback"), readback}});
            identities[identityKey] = identity;
        });
    }
    auto after = reviewModel(task);
    framework::Table<Json> previous;
    for (auto item : before.sourceArrayValue()) previous[item.sourceIndexValue("id").as_string().retainedString()]=item;
    for (auto item : after.sourceArrayValue()) if (auto original=previous.find(
            item.sourceIndexValue("id").as_string().retainedString());original!=previous.end())
        enforceNative(ngRigDigest(item) == ngRigDigest(original->second),"Physics changed an existing node or unrelated binding",2035);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("physics_structure")) = structure; state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("physics_program")) = review::ngRigReviewCompact(plan);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("physics_identities")) = identities;
    return state;
}
}
