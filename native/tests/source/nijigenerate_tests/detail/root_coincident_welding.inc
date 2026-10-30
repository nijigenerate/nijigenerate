// Faithful body of regression.d1474-1500; include once outside namespaces.
#include "root_coincident_welding.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <common/shared_array.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace nijigenerate_tests::regression::detail {
void testCoincidentWeldingVertexSamples() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::param::DeformSlot;
    using nicxlive::core::param::Vec2u;
    using nijigenerate::viewport::common::mesh::IncMesh;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    // The original size_t conversion uses unsigned decimal and a 20-byte buffer.
    const auto indexText = [](std::size_t value) {
        std::array<char, 20> buffer;
        auto first = buffer.size();
        do {
            const auto digit = value % 10;
            value /= 10;
            buffer[--first] = static_cast<char>('0' + digit);
        } while (value);
        auto bytes = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(bytes.data(), buffer.data() + first, bytes.size());
        return RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
    };
    const auto vectorText = [](Vec2 value) {
        std::string output;
        format::TypedFormatWriter writer(output);
        format::FormatSpec spec;
        format::ngFormatValueTo(writer, value, spec);
        return nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(output));
        });
    };
    resetCase();
    auto part = newMeshPart(text("coincident-seam"));
    auto original = part->getMesh();
    MeshData data;
    data.vertices = Vec2Array(SharedArray<Vec2>{original.vertices[0].toVector(),
        original.vertices[1].toVector(), original.vertices[2].toVector(), original.vertices[0].toVector()});
    data.uvs = Vec2Array(SharedArray<Vec2>{original.uvs[0].toVector(),
        original.uvs[1].toVector(), original.uvs[2].toVector(), original.uvs[0].toVector()});
    // Keep the explicit left duplication, right literal, and concatenation allocation.
    auto leftIndices = original.indices.duplicate();
    auto rightIndices = SharedArray<std::uint16_t>{3, 1, 2};
    SharedArray<std::uint16_t> combined(leftIndices.size() + rightIndices.size(), std::uint16_t{});
    for (std::size_t i = 0; i < leftIndices.size(); ++i) combined[i] = leftIndices[i];
    for (std::size_t i = 0; i < rightIndices.size(); ++i) combined[leftIndices.size() + i] = rightIndices[i];
    data.indices = combined;
    part->rebuffer(data);
    auto param = new2DParameter(text("Coincident samples"));
    auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(param->createBinding(part, "deform", false));
    param->addBinding(binding);
    auto offsets = Vec2Array(SharedArray<Vec2>{{1, 2}, {3, 4}, {5, 6}, {21, 22}});
    // DeformSlot is the binding's actual representation of the D Deformation value.
    binding->setValue(Vec2u{0, 0}, DeformSlot(offsets));
    auto mesh = IncMesh::createSourceDeferred([part] {
        auto& cell = part->getMesh();
        return std::shared_ptr<MeshData>(part->mesh, &cell);
    });
    nijigenerate::core::math::mesh::applyMeshToTarget(part, mesh->vertices, &mesh, true);
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        auto offset = offsets[i];
        const auto actual = binding->values[0][0].vertexOffsets[i].toVector();
        const auto expected = offset.toVector();
        const bool condition = actual.x == expected.x && actual.y == expected.y;
        const auto number = indexText(i);
        const auto expectedText = vectorText(offset.toVector());
        const auto actualText = vectorText(binding->values[0][0].vertexOffsets[i].toVector());
        require(condition, concatenateSourceStrings({
            text("Coincident mesh vertices must retain their distinct authored deformation samples: "),
            number, text(" expected "), expectedText, text(" got "), actualText}));
    }
}
}
