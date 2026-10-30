#pragma once
#include "mesh_vertices.hpp"
#include "vertex.hpp"
#include "triangle_resampling.hpp"
#include "nijigenerate/actions/drawable.hpp"
#include "nijigenerate/actions/deformable.hpp"
#include "nijigenerate/actions/binding_all_value_action.hpp"
#include "nijigenerate/core/actionstack.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/ext/param.hpp"
#include "nijigenerate/compat/const_slice.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <nodes/drawable.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

namespace nijigenerate::viewport::common::mesheditor::operations::impl {
void incUpdateWeldedPoints(std::shared_ptr<nicxlive::core::nodes::Drawable> drawable,
    compat::ConstSlice<std::ptrdiff_t> remap);
}

namespace nijigenerate::core::math::mesh {
namespace detail {
template<class Array> decltype(auto) sourceAt(Array& array, std::size_t index, std::size_t line) {
    if (index >= array.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, array.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/mesh.d"), line);
    return array[index];
}
nicxlive::core::math::Vec2Array toVertices(MeshVertexArray vertices);
template<class M> auto& meshObject(M& value) {
    if constexpr (requires { value.export_(); }) return value;
    else return *compat::ngRequireObject(value);
}

template<class M, class F> decltype(auto) withMeshObject(M& value, F&& body) {
    if constexpr (requires { value.export_(); }) return std::forward<F>(body)(value);
    else {
        auto receiver = compat::ngRequireObject(value);
        return std::forward<F>(body)(*receiver);
    }
}
template<class M> bool meshPresent(M* value) {
    // The D condition tests the pointer to the class handle, not the handle value.
    return value != nullptr;
}
template<class T> inline constexpr bool drawableTarget =
    std::is_base_of_v<nicxlive::core::nodes::Drawable, typename T::element_type>;

template<class T> requires drawableTarget<T>
auto sourceTargetMesh(T target) -> decltype(target->nicxlive::core::nodes::Drawable::getMesh()) {
    // The Source Drawable property is final, unlike the legacy native slot.
    return compat::ngRequireObject(target)->nicxlive::core::nodes::Drawable::getMesh();
}

template<class T> requires (!drawableTarget<T>)
auto sourceTargetMesh(T target) -> decltype(target->getMesh()) {
    return compat::ngRequireObject(target)->getMesh();
}

template<class V> auto sourcePosition(V& value) {
    if constexpr (std::is_pointer_v<V> &&
        std::is_same_v<std::remove_cv_t<std::remove_pointer_t<V>>, MeshVertex>) {
        auto requiredVertex = compat::ngRequireObject(value);
        return vertex::position(requiredVertex);
    } else return vertex::position(value);
}
template<class T> struct Applier;

template<class T> requires std::derived_from<typename T::element_type, nicxlive::core::nodes::Drawable>
struct Applier<T> {
    static auto changeAction(T target) {
        return actions::drawable::DrawableChangeAction::createSource([&] {
            return std::tuple{compat::RetainedString(compat::ngRequireObject(target)->getName()), target};
        });
    }
    static void postApply(T target, compat::ConstSlice<std::ptrdiff_t> remap) {
        viewport::common::mesheditor::operations::impl::incUpdateWeldedPoints(target, remap);
    }
    template<class V, class M> static void rebuffer(T target, V vertices, M data = nullptr) {
        compat::ngRequireObject(target)->rebuffer(*compat::ngRequireObject(data));
    }
};

template<class T> requires (std::derived_from<typename T::element_type, nicxlive::core::nodes::Deformable> &&
    !std::derived_from<typename T::element_type, nicxlive::core::nodes::Drawable>)
struct Applier<T> {
    static auto changeAction(T target) {
        return actions::deformable::DeformableChangeAction::createSource([&] {
            return std::tuple{compat::RetainedString(compat::ngRequireObject(target)->getName()), target};
        });
    }
    // The original non-Drawable specialization has no welding operation.
    static void postApply(T target, compat::ConstSlice<std::ptrdiff_t> remap) {}
    template<class M> static void rebuffer(T target, nicxlive::core::math::Vec2Array vertices, M* data = nullptr) {
        compat::ngRequireObject(target)->rebuffer(vertices);
    }
    template<class M> static void rebuffer(T target, compat::Slice<nicxlive::core::math::Vec2> vertices,
        M* data = nullptr) {
        compat::ngRequireObject(target)->rebuffer(triangle::toVec2Array(vertices));
    }
    template<class M> static void rebuffer(T target, const std::vector<nicxlive::core::math::Vec2>& vertices,
        M* data = nullptr) {
        compat::ngRequireObject(target)->rebuffer(triangle::toVec2Array(vertices));
    }
    template<class M> static void rebuffer(T target, MeshVertexArray vertices, M* data = nullptr) {
        compat::ngRequireObject(target)->rebuffer(toVertices(vertices));
    }
};

template<class F> void forDeformationBinding(const std::shared_ptr<nicxlive::core::nodes::Node>& target, F&& body) {
    // Snapshot the D outer array descriptor; changes to its backing entries stay visible.
    auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
    for (auto parameter : parameters) {
        if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
            auto children = group->children;
            for (auto& child : children) body(compat::ngRequireObject(child)->getBinding(target, "deform"));
        } else body(compat::ngRequireObject(parameter)->getBinding(target, "deform"));
    }
}
}

template<class T, class V, class M>
void applyMeshToTarget(T target, V vertices, M* mesh, bool preserveDepthBoneBindings) {
    using Binding = nicxlive::core::param::DeformationParameterBinding;
    using Deformation = nicxlive::core::param::DeformSlot;
    using Vec2u = nicxlive::core::param::Vec2u;
    actionstack::incActionPushGroup();
    compat::ngRunWithScopeExit([&] {
        auto action = detail::Applier<T>::changeAction(target);
        compat::Slice<std::ptrdiff_t> weldingRemap;
        weldingRemap.resize(target->getVertices().size(), -1);
        if constexpr (detail::drawableTarget<T>) {
            bool haveIdentity = false;
            if constexpr (requires { detail::meshObject(*mesh).isBasedOn(detail::sourceTargetMesh(target)); }) {
                if (detail::meshPresent(mesh))
                    haveIdentity = detail::withMeshObject(*mesh, [&](auto& receiver) {
                        return receiver.isBasedOn(detail::sourceTargetMesh(target));
                    });
            }
            for (std::size_t j = 0; j < vertices.size(); ++j) {
                auto value = vertices[j];
                if constexpr (std::is_same_v<decltype(value), MeshVertex*>) {
                    if (haveIdentity) {
                        if (compat::ngRequireObject(value)->originalIndex < weldingRemap.size())
                            detail::sourceAt(weldingRemap, compat::ngRequireObject(value)->originalIndex, 100) =
                                static_cast<std::ptrdiff_t>(j);
                        continue;
                    }
                }
                const auto originalVertices = target->getVertices();
                for (std::size_t i = 0; i < originalVertices.size(); ++i) {
                    const auto original = originalVertices[i].toVector();
                    if (detail::sourceAt(weldingRemap, i, 105) < 0) {
                        const auto point = detail::sourcePosition(value);
                        if (point.x == original.x && point.y == original.y) {
                            detail::sourceAt(weldingRemap, i, 106) = static_cast<std::ptrdiff_t>(j); break;
                        }
                    }
                }
            }
        }
        nicxlive::core::nodes::MeshData data;
        if (detail::meshPresent(mesh)) {
            detail::withMeshObject(*mesh, [&](auto& receiver) { data = receiver.export_(); });
            data.fixWinding();
            compat::ngRequireObject(target)->normalizeUV(&data);
        }
        compat::Slice<std::shared_ptr<Binding>> deformers;
        struct PreservedBinding {
            std::shared_ptr<Binding> binding;
            decltype(std::declval<Binding>().values) values;
        };
        compat::Slice<PreservedBinding> preserved;
        compat::Slice<std::ptrdiff_t> originalIndices;
        if (preserveDepthBoneBindings) {
            originalIndices.resize(vertices.size(), -1);
            for (std::size_t oldIndex = 0; oldIndex < weldingRemap.size(); ++oldIndex)
                if (weldingRemap[oldIndex] >= 0)
                    detail::sourceAt(originalIndices, static_cast<std::size_t>(weldingRemap[oldIndex]), 135) =
                        static_cast<std::ptrdiff_t>(oldIndex);
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                if (detail::sourceAt(originalIndices, i, 138) >= 0) continue;
                if constexpr (detail::drawableTarget<T>) continue;
                auto value = vertices[i];
                const auto point = detail::sourcePosition(value);
                const auto originalVertices = target->getVertices();
                for (std::size_t j = 0; j < originalVertices.size(); ++j) {
                    const auto original = originalVertices[j].toVector();
                    if (detail::sourceAt(weldingRemap, j, 142) < 0 && point.x == original.x && point.y == original.y) {
                        detail::sourceAt(originalIndices, i, 143) = static_cast<std::ptrdiff_t>(j);
                        detail::sourceAt(weldingRemap, j, 144) = static_cast<std::ptrdiff_t>(i); break;
                    }
                }
            }
        }
        const auto alter = [&](const std::shared_ptr<nicxlive::core::param::ParameterBinding>& binding) {
            auto deform = std::dynamic_pointer_cast<Binding>(binding);
            if (!deform) return;
            using Change = actions::binding::ParameterBindingAllValueChangeAction<Deformation>;
            action->addAction(actions::binding::ngCreateBindingAllValueAction<Change>(
                compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(
                    "Deformation recalculation on mesh update")),
                deform, actions::binding::BindingUpdate{}, !preserveDepthBoneBindings));
            if (preserveDepthBoneBindings) {
                PreservedBinding snapshot; snapshot.binding = deform;
                snapshot.values.resize(deform->values.size());
                const auto rows = deform->values;
                for (std::size_t x = 0; x < rows.size(); ++x) {
                    const auto row = rows[x];
                    detail::sourceAt(snapshot.values, x, 160).resize(row.size());
                    for (std::size_t y = 0; y < row.size(); ++y) {
                        const auto value = row[y];
                        auto& destination = detail::sourceAt(detail::sourceAt(snapshot.values, x, 162), y, 162).vertexOffsets;
                        destination = value.vertexOffsets.dup();
                    }
                }
                preserved.append(snapshot);
            }
            const auto xCount = static_cast<std::uint32_t>(deform->values.size());
            for (std::uint32_t x = 0; x < xCount; ++x) {
                const auto yCount = static_cast<std::uint32_t>(detail::sourceAt(deform->values, x, 167).size());
                for (std::uint32_t y = 0; y < yCount; ++y) {
                    [[maybe_unused]] const auto previousDeform =
                        detail::sourceAt(detail::sourceAt(deform->values, x, 168), y, 168);
                    if (deform->isSet(Vec2u{x, y})) {
                        auto changed = triangle::deformByDeformationBinding(vertices, deform.get(), Vec2u{x, y}, false);
                        if (changed) {
                            auto& destination = detail::sourceAt(detail::sourceAt(deform->values, x, 172), y, 172);
                            destination = *changed;
                        }
                    } else detail::sourceAt(detail::sourceAt(deform->values, x, 174), y, 174).vertexOffsets.resize(vertices.size());
                    deformers.append(deform);
                }
            }
        };
        detail::forDeformationBinding(target, alter);
        compat::ngRequireObject(project::incActivePuppet())->resetDrivers();
        compat::ngRequireObject(target)->clearCache();
        detail::Applier<T>::rebuffer(target, vertices, &data);
        for (auto deform : deformers) compat::ngRequireObject(deform)->reInterpolate();
        for (auto snapshot : preserved) {
            const auto rows = snapshot.values;
            for (std::size_t x = 0; x < rows.size(); ++x) {
                const auto row = rows[x];
                for (std::size_t y = 0; y < row.size(); ++y) {
                    auto value = row[y];
                    for (std::size_t i = 0; i < originalIndices.size(); ++i) {
                        const auto original = originalIndices[i];
                        if (original >= 0 && static_cast<std::size_t>(original) < value.vertexOffsets.size()) {
                            auto destination = detail::sourceAt(detail::sourceAt(snapshot.binding->values, x, 212), y, 212)
                                .vertexOffsets[i];
                            destination = value.vertexOffsets[static_cast<std::size_t>(original)];
                        }
                    }
                }
            }
        }
        compat::ngRequireObject(target)->notifyChange(target, nicxlive::core::nodes::NotifyReason::StructureChanged);
        action->updateNewState();
        actionstack::incActionPush(action);
        detail::Applier<T>::postApply(target, weldingRemap);
        if constexpr (requires { detail::meshObject(*mesh).isBasedOn(detail::sourceTargetMesh(target)); }) {
            if (detail::meshPresent(mesh) && detail::withMeshObject(*mesh, [&](auto& receiver) {
                return receiver.isBasedOn(detail::sourceTargetMesh(target));
            })) {
                const auto meshVertices = detail::withMeshObject(*mesh, [](auto& receiver) { return receiver.vertices; });
                for (std::size_t i = 0; i < meshVertices.size(); ++i)
                    compat::ngRequireObject(meshVertices[i])->originalIndex = i;
            }
        }
    }, [] { actionstack::incActionPopGroup(); });
}

template<class T, class V, class M>
void applyMeshToTargetNoRecord(T target, V vertices, M* mesh) {
    using Binding = nicxlive::core::param::DeformationParameterBinding;
    using Vec2u = nicxlive::core::param::Vec2u;
    nicxlive::core::nodes::MeshData data;
    if (detail::meshPresent(mesh)) {
        detail::withMeshObject(*mesh, [&](auto& receiver) { data = receiver.export_(); });
        data.fixWinding(); compat::ngRequireObject(target)->normalizeUV(&data);
    }
    compat::Slice<std::shared_ptr<Binding>> deformers;
    detail::forDeformationBinding(target, [&](const std::shared_ptr<nicxlive::core::param::ParameterBinding>& binding) {
        auto deform = std::dynamic_pointer_cast<Binding>(binding);
        if (!deform) return;
        const auto xCount = static_cast<std::uint32_t>(deform->values.size());
        for (std::uint32_t x = 0; x < xCount; ++x) {
            const auto yCount = static_cast<std::uint32_t>(detail::sourceAt(deform->values, x, 248).size());
            for (std::uint32_t y = 0; y < yCount; ++y) {
                [[maybe_unused]] const auto previousDeform =
                    detail::sourceAt(detail::sourceAt(deform->values, x, 249), y, 249);
                if (deform->isSet(Vec2u{x, y})) {
                    auto changed = triangle::deformByDeformationBinding(vertices, deform.get(), Vec2u{x, y}, false);
                    if (changed) {
                        auto& destination = detail::sourceAt(detail::sourceAt(deform->values, x, 253), y, 253);
                        destination = *changed;
                    }
                } else detail::sourceAt(detail::sourceAt(deform->values, x, 255), y, 255).vertexOffsets.resize(vertices.size());
                deformers.append(deform);
            }
        }
    });
    compat::ngRequireObject(project::incActivePuppet())->resetDrivers();
    compat::ngRequireObject(target)->clearCache();
    detail::Applier<T>::rebuffer(target, vertices, &data);
    for (auto deform : deformers) compat::ngRequireObject(deform)->reInterpolate();
    compat::ngRequireObject(target)->notifyChange(target, nicxlive::core::nodes::NotifyReason::StructureChanged);
}
}
