#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/commands/depth/bone_packet_build.hpp>
#include <nijigenerate/commands/depth/detail/bone_grid_influence.hpp>
#include <nijigenerate/commands/depth/bone_key_projection.hpp>
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/project.hpp>
#include <runtime_state.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <algorithm>
#include <set>

int main(int argc, char** argv) {
    using namespace nijigenerate;
    using Json = core::settings::SettingsJsonValue;
    using Array = core::settings::SettingsJsonArray;
    using Object = core::settings::SettingsJsonObject;
    const bool resolve = argc == 5 && std::string_view(argv[3]) == "--resolve";
    if (argc < 3 || (argc != 3 && !(argc == 4 && std::string_view(argv[3]) == "--packets") && !resolve)) {
        std::cerr << "Usage: autorig_saved_geometry_replay model-document.json output.json [--packets|--resolve raw-offsets.json]\n";
        return 2;
    }
    try {
        const bool packets = argc == 4;
        std::ifstream input(argv[1]);
        const std::string document(std::istreambuf_iterator<char>(input), {});
        auto data = autorig::json::ngParseAutoRigJson(compat::RetainedString(document));
        // Projection depends on the saved bone hierarchy and bindings, not GPU
        // textures. Keep those actual nodes and transforms in this CPU replay.
        std::set<std::uint64_t> gridIds;
        std::function<void(Json)> stripRenderNodes;
        stripRenderNodes = [&](Json node) {
            const auto type = node.sourceIndexValue("type").as_string().view();
            if (type == "GridDeformer") gridIds.insert(autorig::deterministic::contracts::ngRigUnsigned(node.sourceIndexValue("uuid")));
            if (type != "DepthRigRoot" && type != "DepthBone" && !((packets || resolve) && type == "GridDeformer"))
                node.sourceIndexAssignSlot("type") = Json("Node");
            if (const auto children = node.sourceObjectRef().if_contains("children"))
                for (auto child : children->sourceArrayValue()) stripRenderNodes(child);
        };
        stripRenderNodes(data.sourceIndexValue("nodes"));
        if (packets || resolve) {
            // The native bake removes previous grid keys before setting a pose.
            // Keeping them would move descendant origins a second time.
            for (auto parameter : data.sourceIndexValue("param").sourceArrayValue()) {
                if (!parameter.sourceObjectRef().if_contains("bindings")) continue;
                if (!parameter.sourceIndexValue("name").as_string().view().starts_with("Face::")
                        && !parameter.sourceIndexValue("name").as_string().view().starts_with("Body::")) continue;
                Array bindings;
                for (auto binding : parameter.sourceIndexValue("bindings").sourceArrayValue()) {
                    const bool grid = gridIds.contains(autorig::deterministic::contracts::ngRigUnsigned(binding.sourceIndexValue("node")));
                    if (!(grid && binding.sourceIndexValue("param_name").as_string().view() == "deform")) bindings.append(binding);
                }
                parameter.sourceIndexAssignSlot("bindings") = Json(bindings);
            }
        }
        nicxlive::core::nodes::Node::inInitNodesSource();
        ext::nodes::ngRegisterExDepthBoneNodes();
        if (packets || resolve) ext::nodes::ngRegisterExGridDeformer();
        ext::incRegisterExParameter();
        const auto encoded = core::settings::ngSettingsJsonString(data);
        auto puppet = io::project_reader::ngLoadProjectJson<ext::ExPuppet>(encoded);
        if (packets || resolve) {
            // The production depth unit calculation resolves binding targets
            // through the active project. Supply an in-memory project only.
            project::ngCreateHeadlessRegressionProject()->puppet = puppet;
        }
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root;
        std::vector<std::shared_ptr<ext::nodes::ExDepthBone>> bones;
        std::unordered_map<std::uint64_t, std::shared_ptr<nicxlive::core::nodes::Deformable>> targets;
        std::function<void(std::shared_ptr<nicxlive::core::nodes::Node>)> visit;
        visit = [&](auto node) {
            if (auto r = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node)) root = r;
            if (auto bone = std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(node)) bones.push_back(bone);
            if (auto target = std::dynamic_pointer_cast<ext::nodes::ExGridDeformer>(node)) targets[node->uuid] = target;
            for (auto child : node->children) visit(child);
        };
        visit(puppet->root);
        if (!root || bones.empty()) throw std::runtime_error("Saved model has no DepthRigRoot/DepthBones");
        Array rows;
        if (resolve) {
            std::ifstream offsetsInput(argv[4]);
            const std::string offsetsText(std::istreambuf_iterator<char>(offsetsInput), {});
            const auto raw = autorig::json::ngParseAutoRigJson(compat::RetainedString(offsetsText)).sourceArrayValue();
            namespace c = autorig::deterministic::contracts;
            using Vec2Array = nicxlive::core::math::Vec2Array;
            std::vector<std::shared_ptr<nicxlive::core::nodes::Deformable>> ordered;
            for (const auto& [_, target] : targets) ordered.push_back(target);
            const auto depth = [](std::shared_ptr<nicxlive::core::nodes::Node> node) {
                std::size_t result = 0;
                for (; node; node = node->parentPtr()) ++result;
                return result;
            };
            std::sort(ordered.begin(), ordered.end(), [&](auto a, auto b) { return depth(a) < depth(b); });
            for (auto parameter : puppet->parameters) {
                if (!parameter->name.view().starts_with("Face::") && !parameter->name.view().starts_with("Body::")) continue;
                for (std::size_t x = 0; x < parameter->axisPoints[0].size(); ++x)
                    for (std::size_t y = 0; y < parameter->axisPoints[1].size(); ++y) {
                        for (auto other : puppet->parameters) other->value = other->defaults;
                        parameter->value = parameter->unmapValue({parameter->axisPoints[0][x], parameter->axisPoints[1][y]});
                        puppet->update();
                        commands::depth::bone::detail::DepthBoneResolvedOffsets resolved;
                        for (auto target : ordered) {
                            Json entry;
                            for (auto candidate : raw) {
                                auto key = candidate.sourceIndexValue("key").sourceArrayValue();
                                if (candidate.sourceIndexValue("parameter").as_string().view() == parameter->name.view()
                                        && c::ngRigUnsigned(candidate.sourceIndexValue("target")) == target->uuid
                                        && c::ngRigUnsigned(key[0]) == x && c::ngRigUnsigned(key[1]) == y) { entry = candidate; break; }
                            }
                            if (entry.is_null()) continue;
                            const auto points = c::ngRigPoints(entry.sourceIndexValue("offsets"));
                            if (points.size() != target->getVertices().size()) throw std::runtime_error("Offset count differs from actual grid");
                            Vec2Array original; original.setLength(points.size());
                            for (std::size_t i = 0; i < points.size(); ++i)
                                original.set(i, {static_cast<float>(points[i][0]), static_cast<float>(points[i][1])});
                            auto adjusted = original.dup();
                            // Same rootward-first fixed-point correction as the
                            // production completed-batch writeback.
                            for (int iteration = 0; iteration < 6; ++iteration) {
                                auto inherited = commands::depth::bone::detail::depthBoneAncestorGridInfluence(root,
                                    target, parameter, {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)}, adjusted, resolved);
                                if (inherited.size() != adjusted.size()) throw std::runtime_error("Invalid ancestor influence length");
                                auto next = original.dup(); next -= inherited;
                                bool same = true;
                                for (std::size_t i = 0; i < next.size(); ++i)
                                    if (next[i].x != adjusted[i].x || next[i].y != adjusted[i].y) same = false;
                                adjusted = next;
                                if (same) break;
                            }
                            resolved[target->uuid] = adjusted;
                            Array values;
                            for (auto value : adjusted) values.append(Json(Array{Json(value.x), Json(value.y)}));
                            entry.sourceIndexAssignSlot("offsets") = Json(values);
                            rows.append(entry);
                        }
                    }
            }
            if (rows.size() != raw.size()) throw std::runtime_error("Some raw offsets could not be resolved against the saved model");
        } else if (packets) {
            const auto numbers = [](const auto& values) {
                Array result;
                for (const auto value : values) result.append(static_cast<double>(value));
                return Json(result);
            };
            const auto matrix = [](const auto& value) {
                Array result;
                // Match bone_gpu_async_render.cpp. The translated Metal kernel
                // multiplies row vectors by these row-major uploaded matrices.
                for (int row = 0; row < 4; ++row)
                    for (int column = 0; column < 4; ++column) result.append(static_cast<double>(value[row][column]));
                return Json(result);
            };
            for (auto parameter : puppet->parameters) {
                if (!parameter->name.view().starts_with("Face::") && !parameter->name.view().starts_with("Body::")) continue;
                for (std::size_t x = 0; x < parameter->axisPoints[0].size(); ++x)
                    for (std::size_t y = 0; y < parameter->axisPoints[1].size(); ++y) {
                        for (auto other : puppet->parameters) other->value = other->defaults;
                        parameter->value = parameter->unmapValue({parameter->axisPoints[0][x], parameter->axisPoints[1][y]});
                        puppet->update();
                        for (auto& binding : root->bindings) {
                            const auto found = targets.find(binding.targetUuid);
                            if (found == targets.end()) continue;
                            commands::depth::bone::DepthBoneGpuOffsetPacket packet;
                            compat::RetainedStringSlice error;
                            if (!commands::depth::bone::ngBuildDepthBoneGpuOffsetPacket(root, &binding,
                                    found->second, parameter, {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)},
                                    packet, error, false, false)) throw std::runtime_error(std::string(error.view()));
                            Object row;
                            row["parameter"] = Json(core::settings::SettingsString(parameter->name));
                            row["target"] = Json(binding.targetUuid);
                            row["key"] = Json(Array{Json(x), Json(y)});
                            Array vertices;
                            for (auto v : packet.vertices) { vertices.append(v.x); vertices.append(v.y); }
                            row["vertices"] = Json(vertices);
                            row["depths"] = numbers(packet.depths);
                            row["bones"] = numbers(packet.bones);
                            row["sources"] = numbers(packet.sources);
                            row["targetToRoot"] = matrix(packet.targetToRoot);
                            row["rootToTarget"] = matrix(packet.rootToTarget);
                            row["boneCount"] = Json(packet.boneCount);
                            row["sourceCount"] = Json(packet.sourceCount);
                            row["maxInfluences"] = Json(packet.maxInfluences);
                            row["influenceRadiusFloor"] = Json(packet.influenceRadiusFloor);
                            row["radiusScale"] = Json(packet.radiusScale);
                            rows.append(Json(row));
                        }
                    }
            }
            if (rows.empty()) throw std::runtime_error("No saved grid bindings to replay");
        } else
        for (auto parameter : puppet->parameters) {
            const auto name = parameter->name.view();
            if (!name.starts_with("Face::") && !name.starts_with("Body::")) continue;
            for (std::size_t x = 0; x < parameter->axisPoints[0].size(); ++x)
                for (std::size_t y = 0; y < parameter->axisPoints[1].size(); ++y)
                    for (const auto& bone : bones) {
                        const auto projection = commands::depth::bone::ngDepthBoneKeyProjection(root,
                            bone->uuid, parameter, {static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)});
                        Array values;
                        for (const double value : projection) values.append(value);
                        Object row;
                        row["parameter"] = Json(core::settings::SettingsString(parameter->name));
                        row["bone"] = Json(core::settings::SettingsString(bone->boneId));
                        row["bone_uuid"] = Json(static_cast<std::uint64_t>(bone->uuid));
                        row["key"] = Json(Array{Json(x), Json(y)});
                        row["projection"] = Json(values);
                        rows.append(Json(row));
                    }
        }
        std::ofstream output(argv[2]);
        output << core::settings::ngSettingsJsonString(Json(rows));
        std::cout << "Saved-model native " << (resolve ? "ancestor influence" : packets ? "GPU packet" : "bone projection")
                  << " replay: " << rows.size() << " keys\n";
        return 0;
    } catch (const compat::Throwable& error) {
        std::cerr << error.msg.view() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
