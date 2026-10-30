#pragma once
// Port of the independent nijigenerate atlas/atlas.d, BSD-2-Clause.
#include "packer.hpp"
#include "atlas_mapping.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nodes/part.hpp>
#include <texture.hpp>
#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::atlas::atlas {
using Texture = nicxlive::core::Texture;
using Part = nicxlive::core::nodes::Part;
using Rect = nicxlive::core::nodes::Rect;
using BlendMode = nicxlive::core::nodes::BlendMode;
void incInitAtlassing();
class Atlas : public std::enable_shared_from_this<Atlas> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
public:
    float scale = 1;
    int padding = 4;
    std::array<std::shared_ptr<Texture>, 3> textures{};
    std::shared_ptr<packer::TexturePacker> packer;
    AtlasMappings mappings;
    std::array<int, 3> packedIndices{};
    static std::shared_ptr<Atlas> create(std::size_t atlasSize, int padding, float scale);
    template<class Derived, class... Args>
    static std::shared_ptr<Derived> ngCreateDerived(Args&&... args) {
        static_assert(std::is_base_of_v<Atlas, Derived>);
        auto receiver = std::allocate_shared<Derived>(nicxlive::core::d_runtime::SourceAllocator<Derived>{});
        receiver->initialize(std::forward<Args>(args)...);
        return receiver;
    }
    // Native compatibility entry; source callers use the retained logical factory.
    Atlas(std::size_t atlasSize, int padding, float scale);
    virtual ~Atlas() = default;
    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;
    Atlas(Atlas&&) = delete;
    Atlas& operator=(Atlas&&) = delete;
    virtual bool pack(std::shared_ptr<Part> part, float fscale = 1);
    virtual void renderOnTop(std::uint32_t atlasIdx, std::shared_ptr<Texture> texture, Rect where,
        Rect uvs, BlendMode blendMode, float opacity);
    virtual void resize(std::size_t atlasSize);
    virtual void finalize();
    virtual void clear();
    virtual nicxlive::core::d_runtime::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Atlas> other);
    virtual int opCmp(std::shared_ptr<Atlas> other);
protected:
    Atlas() { prepareSourceClassInfo(); }
    virtual void prepareSourceClassInfo();
    void initialize(std::size_t atlasSize, int padding, float scale);
};
bool ngAtlasObjectEquals(std::shared_ptr<Atlas> left, std::shared_ptr<Atlas> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::atlas::atlas::Atlas> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.atlas.atlas.Atlas)";
    inline static constexpr char unqualifiedArgument[] = "(Atlas)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.atlas.atlas.Atlas";
    inline static constexpr char unqualifiedElement[] = "Atlas";
};
template<> struct SourceObjectTypeNames<::nijigenerate::atlas::atlas::Atlas> {
    inline static constexpr char className[] = "nijigenerate.atlas.atlas.Atlas";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.atlas.atlas.Atlas)";
};
}

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::atlas::atlas::Atlas>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::atlas::atlas::Atlas>> {};

} // namespace nijigenerate::core::translation
