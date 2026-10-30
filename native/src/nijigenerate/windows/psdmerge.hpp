#pragma once
#include "base.hpp"
#include <nijigenerate/io/psd/document.hpp>
#include <nijigenerate/io/psd/retained_file.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <common/shared_array.hpp>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::windows::psdmerge {
using Node = nicxlive::core::nodes::Node;
using Texture = nicxlive::core::Texture;
using ExPart = ext::nodes::ExPart;
struct NodeLayerBinding {
    io::psd::Layer layer;
    std::shared_ptr<Texture> layerTexture;
    nicxlive::core::nodes::Vec4 texturePreviewBounds{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    std::shared_ptr<Node> node;
    bool replaceTexture = false;
    compat::RetainedString layerPath;
    const char* layerName = nullptr;
    compat::RetainedString indexableName;
    bool ignore = false;
    NodeLayerBinding() = default;
    NodeLayerBinding(const NodeLayerBinding&) = default;
    NodeLayerBinding& operator=(const NodeLayerBinding&) = default;
    NodeLayerBinding(NodeLayerBinding&& other) : NodeLayerBinding(static_cast<const NodeLayerBinding&>(other)) {}
    NodeLayerBinding& operator=(NodeLayerBinding&& other) {
        return *this = static_cast<const NodeLayerBinding&>(other);
    }
    void retainLayerName(compat::RetainedString value) {
        compat::SourceCString next{compat::RetainedStringSlice{value}};
        compat::detail::ngSourceCStringPostconditions(value.view(), next.data());
        layerNameOwner.emplace(std::move(next));
        layerName = layerNameOwner->data();
    }
    int depth() {
        const bool replace = replaceTexture;
        const auto receiver = node;
        // The source final depth method starts its loop at this and returns
        // zero for a null receiver; a native member call through null is invalid.
        const auto value = receiver ? receiver->depth() : 0;
        return replace ? std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) - 1u) : value;
    }
private:
    // Preserve the separate PSD binding's actual NUL backing across copies.
    std::optional<compat::SourceCString> layerNameOwner;
};
class PSDMergeWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    io::psd::RetainedFile file;
    io::psd::PSD document;
    // Source node parent/puppet references retain this real graph implicitly.
    // Keep its actual owner across UI callbacks without changing SDK weak links.
    std::shared_ptr<nicxlive::core::Puppet> bindingPuppetOwner_;
    compat::Slice<std::shared_ptr<NodeLayerBinding>> bindings;
    bool renameMapped = false, retranslateMapped = false, resortModel = false, onlyUnmapped = false;
    nicxlive::core::common::SharedArray<std::shared_ptr<ExPart>> parts;
    compat::RetainedStringSlice layerFilter, nodeFilter;
    bool appliedTextures = false;
    static constexpr float PreviewSize = 128;
    void initialize(compat::RetainedString path);
    void populateBindings();
    void updatePart(std::shared_ptr<Node> node);
    void apply();
    void layerView();
    void treeView();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
    void onClose() override;
public:
    PSDMergeWindow() = default;
    ~PSDMergeWindow() noexcept override = default;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::psdmerge::NodeLayerBinding> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psdmerge.NodeLayerBinding)";
    inline static constexpr char unqualifiedArgument[] = "(NodeLayerBinding)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psdmerge.NodeLayerBinding";
    inline static constexpr char unqualifiedElement[] = "NodeLayerBinding";
};
}
