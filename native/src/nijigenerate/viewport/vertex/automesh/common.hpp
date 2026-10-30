#pragma once

#include "alpha_provider.hpp"
#include "nijigenerate/core/cv/image.hpp"
#include <common/resource_key_map.hpp>
#include "nijigenerate/viewport/common/mesh.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

namespace nijigenerate::viewport::vertex::automesh::common {

namespace mesh = ::nijigenerate::viewport::common::mesh;

struct AlphaInput {
    std::shared_ptr<::nijigenerate::core::cv::image::Image> img;
    std::int32_t w = 0;
    std::int32_t h = 0;
    bool fromProvider = false;
    nicxlive::core::math::Vec2 providerWorldCenter{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
};

namespace detail {
struct AlphaInputCacheHash {
    std::size_t operator()(std::uint32_t value) const noexcept { return static_cast<std::size_t>(value); }
};
}
using AlphaInputCache = nicxlive::core::common::ResourceKeyMap<std::uint32_t, AlphaInput,
    detail::AlphaInputCacheHash, std::equal_to<std::uint32_t>>;
using Target = std::shared_ptr<nicxlive::core::nodes::Deformable>;
using Mesh = std::shared_ptr<::nijigenerate::viewport::common::mesh::IncMesh>;

void setAutoMeshAlphaInputCache(AlphaInputCache* cache);
void clearAutoMeshAlphaInputCache(AlphaInputCache* cache);
AlphaInput alphaInputFromProvider(std::shared_ptr<IAlphaProvider> provider);
AlphaInput alphaInputFromProviderWithImage(std::shared_ptr<IAlphaProvider> provider);
AlphaInput getAlphaInput(Target target);
nicxlive::core::math::Vec2 alphaImageCenter(AlphaInput input);
void mapImageCenteredMeshToTargetLocal(Mesh& mesh, Target target, AlphaInput input);

} // namespace nijigenerate::viewport::vertex::automesh::common

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::common::AlphaInput> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.vertex.automesh.common.AlphaInput)";
    inline static constexpr char unqualifiedArgument[] = "(AlphaInput)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.vertex.automesh.common.AlphaInput";
    inline static constexpr char unqualifiedElement[] = "AlphaInput";
};
}
