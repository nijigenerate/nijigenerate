// Copyright 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "base.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::operations::base {
namespace {
template<class Allocate> auto allocateMirrorStorage(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

void IncMeshEditorOne::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(IncMeshEditorOne),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.operations.base.IncMeshEditorOne"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString IncMeshEditorOne::toString() {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t IncMeshEditorOne::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool IncMeshEditorOne::opEquals(std::shared_ptr<IncMeshEditorOne> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int IncMeshEditorOne::opCmp(std::shared_ptr<IncMeshEditorOne>) {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string bytes = "need opCmp for class ";
        bytes += className.view();
        message = compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

std::uint64_t IncMeshEditorOne::mirrorVertex(std::uint32_t axis, std::uint64_t index) {
    const auto keepAlive = shared_from_this();
    if (axis == 0) return index;
    const auto vertices = getVerticesByIndex(Indices{index});
    if (vertices.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, vertices.size(),
        compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/operations/base.d"), 191);
    auto* vertex = vertices[0];
    if (!vertex) return std::numeric_limits<std::uint64_t>::max();
    const auto mirrored = getVertexFromPoint(mirror(axis, vertex->position));
    if (mirrored == index) return std::numeric_limits<std::uint64_t>::max();
    return mirrored;
}

IncMeshEditorOne::MeshVertex* IncMeshEditorOne::mirrorVertex(std::uint32_t axis, MeshVertex* vertex) {
    const auto keepAlive = shared_from_this();
    if (axis == 0) return vertex;
    const auto index = getVertexFromPoint(mirror(axis, compat::ngRequireObject(vertex)->position));
    const auto vertices = getVerticesByIndex(Indices{index});
    if (vertices.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, vertices.size(),
        compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/operations/base.d"), 203);
    auto* mirrored = vertices[0];
    if (!mirrored || mirrored == vertex) return nullptr;
    // Preserve the second virtual lookup in the independent source.
    const auto result = getVerticesByIndex(Indices{index});
    if (result.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, result.size(),
        compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/operations/base.d"), 205);
    return result[0];
}

void IncMeshEditorOne::placeOnMirror(Vec2 point, float aoe) {
    const auto keepAlive = shared_from_this();
    if (isOnMirror(point, aoe)) {
        if (mirrorHoriz && mirrorVert && isOnMirrorCenter(point, aoe)) point = {0, 0};
        else if (mirrorVert) point.y = 0;
        else if (mirrorHoriz) point.x = 0;
        const auto vertex = core::math::mesh::ngCreateMeshVertex(point);
        substituteMeshVertices(vertex.get());
    }
}

void IncMeshEditorOne::updateMirrorSelected() {
    auto self = shared_from_this();
    mirrorSelected.resize(0);
    if (!mirrorHoriz && !mirrorVert) return;
    const auto contains = [](const Indices& values, std::uint64_t value) {
        for (auto candidate : values) if (candidate == value) return true;
        return false;
    };
    // The source foreach retains its initial array descriptor across virtual calls.
    const auto originalSelected = selected;
    auto temporary = allocateMirrorStorage([] { return std::make_shared<Indices>(); });
    const auto vertex = allocateMirrorStorage([] { return std::make_shared<std::uint64_t>(); });
    for (auto value : originalSelected) {
        *vertex = value;
        if (contains(mirrorSelected, *vertex)) continue;
        temporary->append(*vertex);
        auto callback = allocateMirrorStorage([&] {
            return std::make_shared<const std::function<void(std::uint32_t)>>(
                [self, temporary, vertex, contains](std::uint32_t axis) {
                    const auto mirrored = self->mirrorVertex(axis, *vertex);
                    if (mirrored == std::numeric_limits<std::uint64_t>::max()) return;
                    if (axis != 0 && !contains(*temporary, mirrored) && !contains(self->mirrorSelected, mirrored))
                        self->mirrorSelected.append(mirrored);
                });
        });
        foreachMirror(std::move(callback));
    }
    const auto mirroredVertices = getVerticesByIndex(mirrorSelected);
    for (auto* vertex : mirroredVertices) updateAddVertexAction(vertex);
    selected = *temporary;
}

} // namespace nijigenerate::viewport::common::mesheditor::operations::base
