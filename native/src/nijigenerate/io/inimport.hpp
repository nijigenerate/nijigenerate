#pragma once
#include "inimport_fwd.hpp"
#include "inimport_layers.hpp"
#include "package.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/retained_exception_message.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <texture.hpp>
#include <bit>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <source_location>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace nijigenerate::io::inimport {
using ImportNode = std::shared_ptr<nicxlive::core::nodes::Node>;
template<class T> struct ImportLayerHash {
    std::size_t operator()(const LayerRef<T>& value) const {
        const auto object = value.operator->();
        return typename nicxlive::core::common::ObjectClassKeyPolicy<
            std::shared_ptr<IncImportLayer<T>>>::Hash{}(object);
    }
};
template<class T> struct ImportLayerEqual {
    bool operator()(const LayerRef<T>& left, const LayerRef<T>& right) const {
        const auto query = left.operator->();
        const auto stored = right.operator->();
        return typename nicxlive::core::common::ObjectClassKeyPolicy<
            std::shared_ptr<IncImportLayer<T>>>::Equal{}(query, stored);
    }
};
template<class T> using ImportedNodes = nicxlive::core::common::ResourceKeyMap<
    LayerRef<T>, ImportNode, ImportLayerHash<T>, ImportLayerEqual<T>>;
template<class T> ImportNode ngCreateImportGroupNode(LayerRef<T> layer, IncImportSettings settings) {
    using nicxlive::core::nodes::Node;
    if (!layer) actions::node::detail::failActionObjectReference();
    if (layer->requiresClippingSurface()) return Node::inInstantiateNodeRetained(
        compat::RetainedString::fromStaticLiteral("DynamicComposite"), {});
    if (!settings.keepStructure) return {};
    if (layer->passThrough && layer->imageLayerRef.opacity < 255) return Node::inInstantiateNodeRetained(
        compat::RetainedString::fromStaticLiteral("Composite"), {});
    return Node::inInstantiateNodeRetained(layer->passThrough
        ? compat::RetainedString::fromStaticLiteral("Node") : settings.layerGroupNodeType.retainedString(), {});
}
template<class T> void ngApplyImportLayerClipping(ImportedNodes<T> nodes) {
    using namespace nicxlive::core::nodes;
    for (const auto [layer, node] : nodes) {
        if (!layer) actions::node::detail::failActionObjectReference();
        if (!layer->clippingBase) continue;
        auto part = std::dynamic_pointer_cast<Part>(node);
        const auto base = nodes.find(layer->clippingBase);
        auto drawable = base == nodes.end() ? nullptr : std::dynamic_pointer_cast<Drawable>(base->second);
        if (!part || !drawable) {
            const auto path = layer->getLayerPath();
            constexpr std::string_view prefix = "PSD clipping needs a Part and drawable base: ";
            const auto message = detail::allocateImportBytes([&] {
                std::string bytes;
                bytes.reserve(prefix.size() + path.size());
                bytes.append(prefix);
                if (!path.empty()) bytes.append(path.data(), path.size());
                // enforce duplicates its lazy message after the concatenation.
                return compat::RetainedString(std::string_view(bytes));
            });
            throw compat::ThrowableException(message,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inimport.d"), 225);
        }
        part->masks.push_back(MaskBinding{drawable->getUUID(), drawable, MaskingMode::Mask});
    }
}
namespace detail {
inline std::int32_t wrap(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
inline std::string format(std::string_view pattern, std::initializer_list<core::translation::TypedFormatArgument> args) {
    return core::translation::ngFormatTyped(pattern, std::span(args.begin(), args.size()));
}
// Additional document providers register their actual fully-qualified source
// names with the canonical metadata registry, never native RTTI spellings.
template<class T> struct DocumentName {
    static compat::RetainedString get() {
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(T));
    }
};
template<> struct DocumentName<psd::PSD> {
    static compat::RetainedString get() { return compat::RetainedString::fromStaticLiteral("psd.PSD"); }
};
template<> struct DocumentName<kra::KRA> {
    static compat::RetainedString get() { return compat::RetainedString::fromStaticLiteral("kra.KRA"); }
};
template<class TypeName, class Message>
void reportImportFailure(TypeName&& readTypeName, Message&& readMessage) {
    core::tasks::ngSetStatus(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Import failed...")));
    const auto title = core::translation::ngTranslateCString("Error");
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "An error occured during %s import:\n%s"));
    const auto typeArgument = core::translation::ngFormatArgument(readTypeName());
    const auto messageArgument = core::translation::ngFormatArgument(readMessage());
    auto formatted = format(pattern.view(), {typeArgument, messageArgument});
    const auto body = allocateImportBytes([&] {
        return widgets::dialog::DialogText::owned(std::move(formatted));
    });
    widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(title.data(), title.owner),
        body);
}
}
template<class T> void incImport(core::settings::SettingsString file, IncImportSettings settings = {}) {
    using namespace nicxlive::core::nodes;
    using Traits = ImportTraits<T>;
    project::incNewProject();
    try {
        T document = Traits::parseDocument(file.retainedString());
        auto layers = incBuildLayerLayout(document);
        const auto centerX = document.width / 2, centerY = document.height / 2;
        auto puppet = ext::ExPuppet::create();
        ImportedNodes<T> imported;
        const auto add = [&](auto& self, ImportNode parent, LayerRef<T> layer) -> void {
            ImportNode child;
            if (!layer) actions::node::detail::failActionObjectReference();
            if (layer->isLayerGroup) child = ngCreateImportGroupNode(layer, settings);
            else {
                auto& image = layer->imageLayerRef;
                image.extractLayerImage();
                // Texture receives the same source layer descriptor after premultiplication.
                {
                    auto pixels = image.data;
                    core::texture_pixels::ngTexPremultiply(
                        std::span<std::uint8_t>(pixels.data(), pixels.size()));
                }
                auto texturePixels = image.data;
                const auto textureWidth = image.width();
                const auto textureHeight = image.height();
                nicxlive::core::SourceTextureBytes textureBytes;
                auto textureOwner = texturePixels.retentionOwner();
                if (textureOwner.use_count() != 0) {
                    textureBytes = nicxlive::core::SourceTextureBytes::retainedView(
                        std::move(textureOwner), texturePixels.data(), texturePixels.size());
                } else {
                    textureBytes = nicxlive::core::SourceTextureBytes::staticView(
                        texturePixels.data(), texturePixels.size());
                }
                auto texture = nicxlive::core::Texture::createSource(
                    textureBytes, detail::wrap(textureWidth), detail::wrap(textureHeight));
                auto part = ext::nodes::incCreateExPart(texture, {},
                    ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(layer->name)));
                const auto layerPath = layer->getLayerPath();
                if (!part) compat::ngFailNullAccess(true);
                part->layerPath = layerPath;
                const auto size = image.size();
                const auto left = image.left;
                const auto top = image.top;
                const auto x = detail::wrap(static_cast<std::uint32_t>(left) +
                    static_cast<std::uint32_t>(detail::wrap(size[0]) / 2) - static_cast<std::uint32_t>(centerX));
                const auto y = detail::wrap(static_cast<std::uint32_t>(top) +
                    static_cast<std::uint32_t>(detail::wrap(size[1]) / 2) - static_cast<std::uint32_t>(centerY));
                part->localTransform.translation = {static_cast<float>(x), static_cast<float>(y), 0};
                child = part;
            }
            if (child) {
                imported[layer] = child;
                child->setName(ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(layer->name)));
                child->zSort(-static_cast<float>(layer->index));
                child->reparent(parent, 0);
                child->setEnabled(Traits::isVisible(layer->imageLayerRef));
                // Native Composite inherits Part; both original branches assign these same properties.
                if (auto part = std::dynamic_pointer_cast<Part>(child)) {
                    part->blendMode = layer->blendMode;
                    part->opacity = static_cast<float>(layer->imageLayerRef.opacity) / 255;
                }
            }
            const auto sublayers = layer->children;
            for (std::size_t i = 0; i < sublayers.size(); ++i) self(self, child ? child : parent, sublayers[i]);
        };
        for (std::size_t i = 0; i < layers.size(); ++i) add(add, puppet->root, layers[i]);
        ngApplyImportLayerClipping(imported);
        puppet->populateTextureSlots();
        compat::ngRequireObject(puppet->root)->transformChanged();
        const auto rootChildren = compat::ngRequireObject(puppet->root)->children;
        for (const auto child : rootChildren) compat::ngRequireObject(child)->centralize();
        compat::ngRequireObject(puppet->root)->build();
        auto activeProject = project::incActiveProject();
        if (!activeProject) compat::ngFailNullAccess(true);
        activeProject->puppet = puppet;
        project::incFocusCamera(compat::ngRequireObject(project::incActivePuppet())->root);
        project::incInitAnimationPlayer(puppet);
        auto importedMessage = detail::format("%s was imported...",
            {core::translation::ngFormatArgument(file)});
        const auto retainedMessage = detail::allocateImportBytes([&] {
            return compat::RetainedString(std::move(importedMessage));
        });
        core::tasks::ngSetStatus(core::translation::ngTranslateRetained(
            retainedMessage));
    } catch (const compat::ThrowableException& error) {
        detail::reportImportFailure([] { return detail::DocumentName<T>::get(); }, [&] {
            return compat::ngRetainExceptionMessage(error);
        });
    }
}
template<class T> class LoadHandler : public ImportKeepHandler {
public:
    explicit LoadHandler(core::settings::SettingsString file) : file_(std::move(file)) {}
    bool load(AskKeepLayerFolder selection) override {
        [[maybe_unused]] const auto receiver = this->weak_from_this().lock();
        switch (selection) {
        case AskKeepLayerFolder::NotPreserve: incImport<T>(file_, IncImportSettings{false}); return true;
        case AskKeepLayerFolder::Preserve: {
            IncImportSettings settings;
            settings.keepStructure = true;
            settings.layerGroupNodeType = core::settings::incSettingsGet<core::settings::SettingsString>(
                compat::RetainedString::fromStaticLiteral("LayerGroupReplacement"),
                compat::RetainedString::fromStaticLiteral("Node"));
            incImport<T>(file_, settings); return true;
        }
        case AskKeepLayerFolder::Cancel: return false;
        default:
            throw compat::ThrowableException(
                compat::RetainedString::fromStaticLiteral("Invalid selection"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inimport.d"), 206);
        }
    }
protected:
    void prepareSourceClassInfo() override {
        ImportKeepHandler::prepareSourceClassInfo();
        detail::ngPrepareImportTemplateClass<LoadHandler<T>>();
    }
private:
    core::settings::SettingsString file_;
};
template<class T> bool incAskImport(core::settings::SettingsString file) {
    if (file.isNull()) return false;
    auto handler = std::allocate_shared<LoadHandler<T>>(
        nicxlive::core::d_runtime::SourceAllocator<LoadHandler<T>>{}, file);
    return incKeepStructDialog(handler);
}
}

namespace nicxlive::core::d_runtime {
template<class T>
struct SourceTypeSpellings<::nijigenerate::io::inimport::LoadHandler<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        static_assert(requires { sizeof(::nijigenerate::io::inimport::LoadHandler<T>); },
            "Source type spelling requires the actual import-handler declaration");
        return ::nijigenerate::io::inimport::detail::ngImportTemplateTypeSpellings<T>("LoadHandler");
    }
};
template<class T>
struct SourceObjectTypeNames<::nijigenerate::io::inimport::LoadHandler<T>> {
    static RetainedString className() {
        return ::nijigenerate::io::inimport::detail::ngImportTemplateName<T>("LoadHandler", false);
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::io::inimport::detail::ngImportTemplateName<T>("LoadHandler", true);
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {
template<class T>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::io::inimport::LoadHandler<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::io::inimport::LoadHandler<T>>> {};
} // namespace nijigenerate::core::translation
