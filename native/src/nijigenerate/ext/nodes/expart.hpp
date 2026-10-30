#pragma once

#include <nodes/part.hpp>
#include <serde.hpp>
#include <texture.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <common/nullable_string.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nijigenerate::ext::nodes {

class ExPart : public nicxlive::core::nodes::Part {
public:
    // Legacy native construction; the Source parent-aware path uses create.
    ExPart();
    static std::shared_ptr<ExPart> create(std::shared_ptr<nicxlive::core::nodes::Node> parent = {});
    static std::shared_ptr<ExPart> create(nicxlive::core::nodes::MeshData data,
        nicxlive::core::common::SharedArray<std::shared_ptr<nicxlive::core::Texture>> textures,
        std::shared_ptr<nicxlive::core::nodes::Node> parent = {});
    template<class BuildArguments>
    static std::shared_ptr<ExPart> createSourceData(BuildArguments&& buildArguments) {
        auto result = std::allocate_shared<ExPart>(
            nicxlive::core::d_runtime::SourceAllocator<ExPart>{}, DeferredConstruction{});
        auto arguments = std::forward<BuildArguments>(buildArguments)();
        std::apply([&](auto&&... values) { result->constructSourceData(values...); }, arguments);
        return result;
    }
    ExPart(const nicxlive::core::nodes::MeshData& data,
        const std::vector<std::shared_ptr<nicxlive::core::Texture>>& textures);
    ExPart(const ExPart&) = delete;
    ExPart& operator=(const ExPart&) = delete;
    ExPart(ExPart&&) = delete;
    ExPart& operator=(ExPart&&) = delete;

    compat::RetainedString layerPath{};

protected:
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    explicit ExPart(DeferredConstruction construction);
    void constructSourceData(const nicxlive::core::nodes::MeshData& data,
        const nicxlive::core::common::SharedArray<std::shared_ptr<nicxlive::core::Texture>>& textures,
        const std::shared_ptr<nicxlive::core::nodes::Node>& parent);
    void serializeSelf(nicxlive::core::serde::TypedJsonWriter& serializer) const override;
    nicxlive::core::serde::ReadError deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) override;
    void serializeSelf(nicxlive::core::serde::InochiSerializer& serializer) const override;
    nicxlive::core::serde::SerdeException deserializeFromFghj(
        const nicxlive::core::serde::Fghj& data) override;
};

std::shared_ptr<ExPart> incCreateExPart(std::shared_ptr<nicxlive::core::Texture> texture,
    std::shared_ptr<nicxlive::core::nodes::Node> parent = {},
    nicxlive::core::common::NullableString name =
        nicxlive::core::common::NullableString::fromStaticLiteral("New Part"));

// Call during explicit editor initialization, before loading or creating models.
void incRegisterExPart();

} // namespace nijigenerate::ext::nodes

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExPart> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.expart.ExPart";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.expart.ExPart)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExPart> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.expart.ExPart)";
    inline static constexpr char unqualifiedArgument[] = "(ExPart)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.expart.ExPart";
    inline static constexpr char unqualifiedElement[] = "ExPart";
};
}
