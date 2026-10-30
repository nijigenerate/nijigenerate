#include "transform.hpp"
#include "transform_internal.hpp"
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/math/triangle_resampling.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/windows/flipconfig_model.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nodes/drawable.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/source_allocator.hpp>
#include <stdexcept>
#include <limits>

namespace nijigenerate::utils::transform {
namespace {
namespace param = nicxlive::core::param;
namespace nodes = nicxlive::core::nodes;
using Binding = param::ParameterBinding;
using Deformation = nodes::Deformation;
using DeformBinding = param::DeformationParameterBinding;

template<class T>
T extrapolateValueAt(param::ParameterBindingImpl<T>* binding, param::Vec2u index, std::int32_t axis) {
    auto* receiver = compat::ngRequireObject(binding);
    const auto bindingLifetime = receiver->weak_from_this().lock();
    param::Vec2 offset;
    {
        const auto parameterLifetime = receiver->getParameter();
        offset = compat::ngRequireObject(receiver->parameter)->getKeypointOffset(index);
    }
    switch (axis) {
    case -1: offset = {1.0f - offset.x, 1.0f - offset.y}; break;
    case 0: offset.x = 1.0f - offset.x; break;
    case 1: offset.y = 1.0f - offset.y; break;
    case 2: break;
    default: compat::ngAssertUnreachable("bad axis", "source/nijigenerate/utils/transform.d", 81);
    }
    param::Vec2u sourceIndex{};
    param::Vec2 subOffset{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    {
        const auto parameterLifetime = receiver->getParameter();
        compat::ngRequireObject(receiver->parameter)->findOffset(offset, sourceIndex, subOffset);
    }
    sourceIndex = detail::sourceIndexValue(sourceIndex);
    return receiver->interpolate(sourceIndex, subOffset);
}

std::shared_ptr<Deformation> getMaskedDeformation(param::DeformSlot* old,
    std::shared_ptr<Deformation> replacement, compat::Slice<std::uint64_t>* selected) {
    if (!replacement) return {};
    // The original aggregate initializes the Vec2Array descriptor directly.
    auto masked = std::allocate_shared<Deformation>(
        nicxlive::core::d_runtime::SourceAllocator<Deformation>{},
        Deformation{compat::ngRequireObject(old)->vertexOffsets});
    const auto offsets = old->vertexOffsets;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        bool found = false;
        const auto selectedValues = *compat::ngRequireObject(selected);
        for (std::size_t j = 0; j < selectedValues.size(); ++j) {
            if (selectedValues[j] == i) { found = true; break; }
        }
        if (found) {
            auto destination = masked->vertexOffsets[i];
            destination = replacement->vertexOffsets[i];
        }
    }
    return masked;
}

std::shared_ptr<Deformation> resample(DeformBinding* destination, DeformBinding* source,
    param::Vec2u index, std::int32_t axis, bool extrapolation, bool hasSource, bool& supported) {
    std::shared_ptr<nodes::Drawable> drawable, sourceDrawable;
    std::shared_ptr<nodes::PathDeformer> path, sourcePath;
    std::shared_ptr<nodes::GridDeformer> grid, sourceGrid;
    if (hasSource) {
        // Each original cast performs a fresh virtual target lookup, in this order.
        path = std::dynamic_pointer_cast<nodes::PathDeformer>(destination->getTarget().target.lock());
        sourcePath = std::dynamic_pointer_cast<nodes::PathDeformer>(source->getTarget().target.lock());
        grid = std::dynamic_pointer_cast<nodes::GridDeformer>(destination->getTarget().target.lock());
        sourceGrid = std::dynamic_pointer_cast<nodes::GridDeformer>(source->getTarget().target.lock());
        drawable = std::dynamic_pointer_cast<nodes::Drawable>(destination->getTarget().target.lock());
        sourceDrawable = std::dynamic_pointer_cast<nodes::Drawable>(source->getTarget().target.lock());
    } else {
        drawable = std::dynamic_pointer_cast<nodes::Drawable>(destination->getTarget().target.lock());
        path = std::dynamic_pointer_cast<nodes::PathDeformer>(destination->getTarget().target.lock());
        grid = std::dynamic_pointer_cast<nodes::GridDeformer>(destination->getTarget().target.lock());
        sourceDrawable = drawable;
        sourcePath = path;
        sourceGrid = grid;
    }
    const auto sample = [&] {
        auto slot = extrapolation ? extrapolateValueAt(source, index, axis) :
            detail::interpolateValueAt(source, index, axis);
        return Deformation{std::move(slot.vertexOffsets)};
    };
    using core::math::triangle::deformByDeformationBinding;
    if (drawable && sourceDrawable) {
        supported = true;
        auto& meshData = drawable->nodes::Drawable::getMesh();
        const auto meshOwner = drawable->mesh;
        const auto mesh = viewport::common::mesh::IncMesh::create(meshData, meshOwner);
        const auto deformation = sample();
        auto vertices = mesh->vertices;
        auto argument = deformation;
        return deformByDeformationBinding(std::move(vertices), sourceDrawable.get(),
            std::move(argument), extrapolation || axis < 1);
    }
    if (path && sourcePath) {
        supported = true;
        const auto deformation = sample();
        auto vertices = path->getVertices();
        auto argument = deformation;
        return deformByDeformationBinding(std::move(vertices), sourcePath.get(),
            std::move(argument), extrapolation || axis < 1);
    }
    if (grid && sourceGrid) {
        supported = true;
        const auto deformation = sample();
        auto vertices = grid->getVertices();
        auto argument = deformation;
        return deformByDeformationBinding(std::move(vertices), sourceGrid.get(),
            std::move(argument), extrapolation || axis < 1);
    }
    return {};
}
}

std::shared_ptr<Binding> ngBindingGetPairFor(std::shared_ptr<param::Parameter> parameter,
    std::shared_ptr<nodes::Node> target, std::shared_ptr<windows::flipconfig::FlipPair> pair,
    compat::RetainedStringSlice name, bool forceCreate) {
    std::shared_ptr<nodes::Node> pairNode;
    bool firstMatches = false;
    if (pair && pair->parts[0]) {
        const auto firstUuid = compat::ngRequireObject(pair->parts[0])->getUUID();
        const auto targetUuid = compat::ngRequireObject(target)->getUUID();
        firstMatches = firstUuid == targetUuid;
    }
    if (firstMatches) pairNode = pair->parts[1];
    else if (pair) pairNode = pair->parts[0];
    if (!pairNode && forceCreate) pairNode = target;
    if (pairNode) {
        const auto bindings = compat::ngRequireObject(parameter)->bindings;
        for (const auto binding : bindings) {
            const auto bound = compat::ngRequireObject(binding)->getTarget();
            const auto boundUuid = compat::ngRequireObject(bound.target.lock())->getUUID();
            const auto pairUuid = pairNode->getUUID();
            if (boundUuid == pairUuid && binding->getTarget().name == name.view())
                return binding;
        }
    }
    if (forceCreate) {
        auto result = compat::ngRequireObject(parameter)->createBinding(pairNode, name.retainedString().toNullableString());
        if (name.view() == "deform" && !std::dynamic_pointer_cast<nodes::Deformable>(pairNode)) return {};
        parameter->addBinding(result);
        auto action = actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(
            parameter, result);
        core::actionstack::incActionPush(std::move(action));
        return result;
    }
    return {};
}

void ngBindingAutoFlip(std::shared_ptr<Binding> binding, std::shared_ptr<Binding> sourceBinding,
    compat::Vec2u targetIndex, std::int32_t axis, bool extrapolation, compat::Slice<std::uint64_t>* selected) {
    const param::Vec2u index{targetIndex.x, targetIndex.y};
    const auto deformBinding = std::dynamic_pointer_cast<DeformBinding>(binding);
    if (sourceBinding) {
        const auto sourceDeform = std::dynamic_pointer_cast<DeformBinding>(sourceBinding);
        if (deformBinding && sourceDeform) {
            bool supported = false;
            auto replacement = resample(deformBinding.get(), sourceDeform.get(), index, axis,
                extrapolation, true, supported);
            if (selected && supported)
                replacement = getMaskedDeformation(&deformBinding->getValue(index), replacement, selected);
            if (replacement) deformBinding->setValue(index, *replacement);
        } else {
            const auto sourceValue = std::dynamic_pointer_cast<param::ValueParameterBinding>(sourceBinding);
            if (!sourceValue) return;
            const auto value = extrapolation ? extrapolateValueAt(sourceValue.get(), index, axis) :
                detail::interpolateValueAt(sourceValue.get(), index, axis);
            const auto valueBinding = std::dynamic_pointer_cast<param::ValueParameterBinding>(binding);
            compat::ngRequireObject(valueBinding)->setValue(index, value);
            if (axis < 2) valueBinding->scaleValueAt(index, axis, -1);
        }
    } else if (deformBinding) {
        bool supported = false;
        auto replacement = resample(deformBinding.get(), deformBinding.get(), index, axis,
            extrapolation, false, supported);
        if (selected) replacement = getMaskedDeformation(&deformBinding->getValue(index), replacement, selected);
        if (replacement) deformBinding->setValue(index, *replacement);
    } else if (extrapolation) {
        compat::ngRequireObject(binding)->extrapolateValueAt(index, axis);
    }
}
}
