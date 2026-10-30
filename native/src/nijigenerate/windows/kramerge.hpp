#pragma once
#include "base.hpp"
#include <nijigenerate/io/kra/document.hpp>
#include <nijigenerate/io/psd/retained_file.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <common/shared_array.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace nijigenerate::windows::kramerge {
using Node = nicxlive::core::nodes::Node;
using Texture = nicxlive::core::Texture;
using ExPart = ext::nodes::ExPart;
struct NodeLayerBinding {
    io::kra::Layer layer;
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
        layerNameStorage.emplace(std::move(next));
        layerName = layerNameStorage->data();
    }
    int depth() {
        const bool replace = replaceTexture;
        const auto receiver = node;
        // The source final getter starts its loop at nullable this.
        const auto value = receiver ? receiver->depth() : 0;
        return replace ? std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) - 1u) : value;
    }
private:
    // Copies retain the actual required toStringz buffer behind the source pointer.
    std::optional<compat::SourceCString> layerNameStorage;
};
class KRAMergeWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    io::psd::RetainedFile file;
    io::kra::KRA document;
    // Preserve the actual populated model across approved weak SDK back-links.
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
    KRAMergeWindow() = default;
    ~KRAMergeWindow() noexcept override = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<KRAMergeWindow> other);
};
bool ngKRAMergeWindowObjectEquals(std::shared_ptr<KRAMergeWindow> left,
    std::shared_ptr<KRAMergeWindow> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::kramerge::NodeLayerBinding> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.kramerge.NodeLayerBinding)";
    inline static constexpr char unqualifiedArgument[] = "(NodeLayerBinding)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.kramerge.NodeLayerBinding";
    inline static constexpr char unqualifiedElement[] = "NodeLayerBinding";
};
}
