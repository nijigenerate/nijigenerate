// Faithful body of regression.d20064-20111; include once outside namespaces.
#include "root_meshgroup_compat.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/node/conversion_map.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <common/shared_array.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/mesh_group.hpp>
#include <algorithm>

namespace nijigenerate_tests::regression::detail {
void testMeshGroupGridDeformerCompatibility() {
    using nijigenerate::commands::node::base::conversionMap;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::project::incActivePuppet;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::MeshGroup;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto containsConversion = [](RetainedString from, RetainedString to) {
        if (!conversionMap().contains(from.view())) return false;
        const auto destinations = conversionMap().get(from.view());
        return std::find_if(destinations.begin(), destinations.end(), [&](RetainedString item) {
            return item.view() == to.view();
        }) != destinations.end();
    };
    resetCase();
    require(containsConversion(text("MeshGroup"), text("GridDeformer")),
        text("node conversion map should keep MeshGroup to GridDeformer migration available"));
    require(containsConversion(text("GridDeformer"), text("MeshGroup")),
        text("node conversion map should keep GridDeformer to MeshGroup compatibility available"));

    auto grid = GridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    grid->setName(NullableString::fromStaticLiteral("compat-grid"));
    grid->dynamic = true;
    grid->rebuffer(Vec2Array(SharedArray<Vec2>{{-10, -5}, {10, -5}, {-10, 5}, {10, 5}}));
    grid->getDeformation() = Vec2Array(SharedArray<Vec2>{{1, 0}, {0, 2}, {-1, 0}, {0, -2}});

    auto meshGroup = MeshGroup::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    meshGroup->setName(NullableString::fromStaticLiteral("compat-meshgroup"));
    meshGroup->copyFrom(*grid, false, true);
    require(meshGroup->getMesh().vertices.size() == grid->getVertices().size(),
        text("MeshGroup compatibility copy should preserve GridDeformer vertex count"));
    require(meshGroup->getMesh().indices.size() == 6,
        text("MeshGroup compatibility copy should rebuild a quad grid as two triangles"));
    require(meshGroup->getDeformation().size() == grid->getDeformation().size(),
        text("MeshGroup compatibility copy should preserve deformation length"));
    require(nearVec2Array(meshGroup->getDeformation(), grid->getDeformation()),
        text("MeshGroup compatibility copy should preserve deformation offsets"));
    require(meshGroup->dynamic == grid->dynamic,
        text("MeshGroup compatibility copy should preserve dynamic mode"));
    require(meshGroup->getTranslateChildren(),
        text("MeshGroup compatibility copy from GridDeformer should keep child translation enabled"));

    auto copiedMeshGroup = MeshGroup::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    copiedMeshGroup->copyFrom(*meshGroup, false, true);
    require(copiedMeshGroup->dynamic == meshGroup->dynamic,
        text("MeshGroup-to-MeshGroup copy should preserve dynamic mode"));
    require(copiedMeshGroup->getTranslateChildren() == meshGroup->getTranslateChildren(),
        text("MeshGroup-to-MeshGroup copy should preserve translate-children mode"));
}
}
