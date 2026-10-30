#pragma once

// Copyright (c) 2020-2023 Inochi2D Project, 2024 nijigenerate Project.
// Distributed under the repository's 2-Clause BSD License.
#include "nijigenerate/compat/slice.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <math/types.hpp>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <compat/retained_string.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::atlas::packer {
using Vec2i = ::nicxlive::core::math::Vec2i;
using Vec4i = ::nicxlive::core::math::Vec4i;
namespace detail {
struct PackerProbe;
bool contains(Vec4i a, Vec4i b);
class Bin {
    friend struct PackerProbe;
    Vec2i size;
    compat::Slice<Vec4i> usedRectangles;
    compat::Slice<Vec4i> freeRectangles;
    Vec4i scoreRect(Vec2i size, std::int32_t& score1, std::int32_t& score2);
    Vec4i scoreRect(Vec2i size);
    void place(Vec4i& newNode);
    Vec4i findNewNodeFit(Vec2i size, std::int32_t score1, std::int32_t score2);
    bool splitFree(Vec4i freeNode, Vec4i& usedNode);
    void prune();
public:
    Bin() = default;
    explicit Bin(Vec2i size);
    void resize(Vec2i size);
    Vec4i insert(Vec2i size);
    void remove(Vec4i area);
    void clear();
    float occupancy();
    Vec2i getSize();
};
}
class TexturePacker : public std::enable_shared_from_this<TexturePacker> {
    friend struct detail::PackerProbe;
    detail::Bin bin;
public:
    explicit TexturePacker(Vec2i textureSize = {1024, 1024});
    virtual ~TexturePacker() = default;
    TexturePacker(const TexturePacker&) = delete;
    TexturePacker& operator=(const TexturePacker&) = delete;
    TexturePacker(TexturePacker&&) = delete;
    TexturePacker& operator=(TexturePacker&&) = delete;
    virtual Vec4i packTexture(Vec2i size);
    virtual void remove(Vec4i area);
    virtual void clear();
    virtual nicxlive::core::d_runtime::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<TexturePacker> other);
    virtual int opCmp(std::shared_ptr<TexturePacker> other);
protected:
    virtual void prepareSourceClassInfo();
};
bool ngTexturePackerObjectEquals(std::shared_ptr<TexturePacker> left, std::shared_ptr<TexturePacker> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::atlas::packer::TexturePacker> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.atlas.packer.TexturePacker)";
    inline static constexpr char unqualifiedArgument[] = "(TexturePacker)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.atlas.packer.TexturePacker";
    inline static constexpr char unqualifiedElement[] = "TexturePacker";
};
template<> struct SourceObjectTypeNames<::nijigenerate::atlas::packer::TexturePacker> {
    inline static constexpr char className[] = "nijigenerate.atlas.packer.TexturePacker";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.atlas.packer.TexturePacker)";
};
template<> struct SourceTypeSpellings<::nijigenerate::atlas::packer::detail::Bin> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.atlas.packer.Bin)";
    inline static constexpr char unqualifiedArgument[] = "(Bin)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.atlas.packer.Bin";
    inline static constexpr char unqualifiedElement[] = "Bin";
};
}

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::atlas::packer::TexturePacker>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::atlas::packer::TexturePacker>> {};

} // namespace nijigenerate::core::translation
