#include "kramerge.hpp"
#include "kramerge_path.hpp"
#include "kramerge_depth_sort.hpp"
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nodes/projectable.hpp>
#include <resource.hpp>
#include <imgui_internal.h>
#include <bit>
#include <cstring>
#include <span>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <optional>
#include <new>

namespace nijigenerate::windows::kramerge {
namespace {
template<class Operation> decltype(auto) allocateKraMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void prepareKRAMergeClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<KRAMergeWindow>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<KRAMergeWindow>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(KRAMergeWindow), name);
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString KRAMergeWindow::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareKRAMergeClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t KRAMergeWindow::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int KRAMergeWindow::opCmp(std::shared_ptr<KRAMergeWindow>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareKRAMergeClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = allocateKraMemory([&] {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    });
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngKRAMergeWindowObjectEquals(std::shared_ptr<KRAMergeWindow> left,
    std::shared_ptr<KRAMergeWindow> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareKRAMergeClassInfo);
}
namespace {
using nicxlive::core::nodes::Vec3;
using nicxlive::core::nodes::Vec4;
using nicxlive::core::nodes::Mat4;
using nicxlive::core::nodes::Drawable;
compat::RetainedString lower(std::string_view value) {
    auto retained = allocateKraMemory([&] { return compat::RetainedString(value); });
    return io::depthmap_psd::ngPsdDepthLowerString(std::move(retained));
}
compat::RetainedString lower(compat::RetainedString value) {
    return io::depthmap_psd::ngPsdDepthLowerString(std::move(value));
}
Vec4 previewBounds(float width, float height) {
    const float ws = 128.0f / width, hs = 128.0f / height;
    const float scale = hs < ws ? hs : ws;
    Vec4 result{0, 0, width * scale, height * scale};
    if (ws > hs) result.x = (128 - result.z) / 2;
    else if (ws < hs) result.y = (128 - result.w) / 2;
    return result;
}
std::int32_t addInt(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) + std::bit_cast<std::uint32_t>(b));
}
Vec4 partPreviewBounds(const std::shared_ptr<ExPart>& part) {
    float widthScale, heightScale, width, height;
    {
        const auto texture = compat::ngRequireObject(part)->textures.at(0);
        widthScale = 128.0f / static_cast<float>(compat::ngRequireObject(texture)->width());
    }
    {
        const auto texture = compat::ngRequireObject(part)->textures.at(0);
        heightScale = 128.0f / static_cast<float>(compat::ngRequireObject(texture)->height());
    }
    const auto scale = heightScale < widthScale ? heightScale : widthScale;
    {
        const auto texture = compat::ngRequireObject(part)->textures.at(0);
        width = static_cast<float>(compat::ngRequireObject(texture)->width()) * scale;
    }
    {
        const auto texture = compat::ngRequireObject(part)->textures.at(0);
        height = static_cast<float>(compat::ngRequireObject(texture)->height()) * scale;
    }
    Vec4 bounds{0, 0, width, height};
    if (widthScale > heightScale) bounds.x = (128 - bounds.z) / 2;
    else if (widthScale < heightScale) bounds.y = (128 - bounds.w) / 2;
    return bounds;
}
template<typename GetTexture, typename ImageSize>
void preview(GetTexture getTexture, Vec4 bounds, ImageSize imageSize) {
    const auto position = ImGui::GetCursorPos();
    ImGui::ItemSize(ImVec2(128, 128));
    ImGui::SetCursorPos(ImVec2(position.x + bounds.x, position.y + bounds.y));
    const auto texture = getTexture();
    const auto textureId = compat::ngRequireObject(texture)->getTextureId();
    ImGui::Image(static_cast<ImTextureID>(textureId), imageSize());
}
}
void KRAMergeWindow::populateBindings() {
    const auto receiver = weak_from_this().lock();
    const auto puppet = project::incActivePuppet();
    parts = compat::ngRequireObject(puppet)->findNodesType<ExPart>(compat::ngRequireObject(puppet)->root);
    bindingPuppetOwner_ = puppet;
    const auto find = [&](std::string_view segment, bool nameOnly) -> std::shared_ptr<ExPart> {
        auto currentParts = parts;
        for (auto& part : currentParts) {
            const auto layerPath = compat::ngRequireObject(part)->layerPath;
            std::string fallback;
            if (layerPath.empty()) {
                const auto nameReceiver = part;
                const auto name = compat::ngRequireObject(nameReceiver)->getName();
                fallback = allocateKraMemory([&] { return "/" + std::string(name.view()); });
            }
            const auto candidate = layerPath.empty() ? std::string_view(fallback) : layerPath.view();
            if (nameOnly ? detail::baseName(candidate) == detail::baseName(segment) : candidate == segment) return part;
        }
        return {};
    };
    compat::Slice<compat::RetainedString> path;
    std::string segment;
    const auto sourceLayers = document.layers;
    for (std::size_t index = 0; index < sourceLayers.size(); ++index) {
        auto layer = sourceLayers[index];
        if (layer.type != io::kra::LayerType::Any) {
            if (layer.type != io::kra::LayerType::SectionDivider) path.append(layer.name);
            else {
                // A wrapped D string[] length overflows the runtime byte count.
                if (path.empty()) throw compat::ngStaticError<compat::OutOfMemoryError>(
                    std::source_location::current());
                path.resize(path.size() - 1);
            }
            segment.clear();
            allocateKraMemory([&] {
                for (const auto& item : path) { segment += '/'; segment += item.view(); }
            });
            continue;
        }
        layer.extractLayerImage();
        if (layer.data.empty()) continue;
        auto input = layer.data;
        auto data = nicxlive::core::SourceTextureBytes::retainedView(
            input.retentionOwner(), input.data(), input.size());
        nicxlive::core::inTexPremultiply(data);
        const auto width = std::bit_cast<std::int32_t>(layer.width());
        const auto height = std::bit_cast<std::int32_t>(layer.height());
        auto texture = Texture::createSource(data, width, height);
        layer.data = {};
        const auto bounds = previewBounds(static_cast<float>(layer.width()), static_cast<float>(layer.height()));
        const auto fullPath = allocateKraMemory([&] {
            return compat::RetainedString(segment + "/" + std::string(layer.name.view()));
        });
        auto part = find(fullPath.view(), false); if (!part) part = find(fullPath.view(), true);
        auto binding = std::allocate_shared<NodeLayerBinding>(
            nicxlive::core::d_runtime::SourceAllocator<NodeLayerBinding>{});
        binding->layer = layer; binding->layerTexture = texture;
        binding->texturePreviewBounds = bounds;
        binding->node = part ? std::static_pointer_cast<Node>(part) : compat::ngRequireObject(puppet)->root;
        binding->replaceTexture = static_cast<bool>(part);
        binding->layerPath = fullPath;
        binding->retainLayerName(layer.name); binding->indexableName = lower(layer.name);
        bindings.append(binding);
    }
}
void KRAMergeWindow::updatePart(std::shared_ptr<Node> node) {
    const auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node);
    if (part) {
        auto& sourceMesh = part->Drawable::getMesh();
        const auto sourceMeshOwner = part->mesh;
        auto mesh = viewport::common::mesh::IncMesh::create(sourceMesh, sourceMeshOwner);
        auto data = mesh->export_(); data.fixWinding();
        const auto uvCount = data.uvs.size();
        for (std::size_t i = 0; i < uvCount; ++i) {
            const auto texture = part->textures.at(0);
            data.uvs[i].x /= static_cast<float>(compat::ngRequireObject(texture)->width());
            data.uvs[i].y /= static_cast<float>(compat::ngRequireObject(texture)->height());
            data.uvs[i].x += 0.5f; data.uvs[i].y += 0.5f;
        }
        part->rebuffer(data);
    }
    const auto children = compat::ngRequireObject(node)->children;
    for (const auto child : children) updatePart(child);
}
void KRAMergeWindow::apply() {
    const auto receiver = weak_from_this().lock();
    auto sortedBindings = bindings;
    detail::depth_sort::sort({sortedBindings.data(), sortedBindings.size()});
    const auto cx = document.width / 2, cy = document.height / 2;
    const auto puppet = project::incActivePuppet();
    const auto currentBindings = bindings;
    for (const auto binding : currentBindings) {
        if (compat::ngRequireObject(binding)->ignore) continue;
        const auto layerSize = binding->layer.size();
        const auto width = std::bit_cast<std::int32_t>(layerSize[0]);
        const auto height = std::bit_cast<std::int32_t>(layerSize[1]);
        const Vec3 world{static_cast<float>(addInt(binding->layer.left, width / 2)) - static_cast<float>(cx),
            static_cast<float>(addInt(binding->layer.top, height / 2)) - static_cast<float>(cy), 0};
        auto local = binding->node ? Node::getRelativePosition(binding->node->transformNoLock().matrix(), Mat4::translation(world)) : world;
        if (binding->replaceTexture) {
            const auto parent = compat::ngRequireObject(binding->node)->parentPtr();
            local = parent ? Node::getRelativePosition(parent->transformNoLock().matrix(), Mat4::translation(world)) : world;
            compat::ngRequireObject(binding->node)->recalcTransform = true;
            if (renameMapped) {
                const auto nameTarget = std::dynamic_pointer_cast<ExPart>(binding->node);
                compat::ngRequireObject(nameTarget)->setName(binding->layer.name.toNullableString());
            }
            if (retranslateMapped) {
                if (compat::ngRequireObject(binding->node)->lockToRootValue())
                    compat::ngRequireObject(binding->node)->localTransform.translation = world;
                else compat::ngRequireObject(binding->node)->localTransform.translation = local;
            }
            {
                const auto textureTarget = std::dynamic_pointer_cast<ExPart>(binding->node);
                const auto previousTexture = compat::ngRequireObject(textureTarget)->textures.at(0);
                compat::ngRequireObject(previousTexture)->dispose();
            }
            {
                const auto textureTarget = std::dynamic_pointer_cast<ExPart>(binding->node);
                compat::ngRequireObject(textureTarget)->textures.at(0) = binding->layerTexture;
            }
            {
                const auto pathTarget = std::dynamic_pointer_cast<ExPart>(binding->node);
                compat::ngRequireObject(pathTarget)->layerPath = binding->layerPath;
            }
        } else {
            auto part = ext::nodes::incCreateExPart(binding->layerTexture, binding->node,
                binding->layer.name.toNullableString());
            compat::ngRequireObject(part)->layerPath = binding->layerPath;
            compat::ngRequireObject(part)->localTransform.translation = local;
        }
    }
    document = {};
    compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->transformChanged();
    updatePart(compat::ngRequireObject(puppet)->root);
    compat::ngRequireObject(puppet)->populateTextureSlots();
}
void KRAMergeWindow::layerView() {
    using namespace widgets;
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    const auto layerCount = bindings.size();
    for (std::size_t i = 0; i < layerCount; ++i) {
        if (i >= bindings.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, bindings.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/kramerge.d"), 239);
        const auto layer = bindings[i];
        const auto root = compat::ngRequireObject(project::incActivePuppet())->root;
        const bool unmapped = !compat::ngRequireObject(layer)->replaceTexture || !layer->node || layer->node == root;
        if (onlyUnmapped && !unmapped) continue;
        if (!layerFilter.empty()) {
            const auto indexableName = layer->indexableName;
            const auto loweredFilter = lower(layerFilter.retainedString());
            if (indexableName.view().find(loweredFilter.view()) == std::string_view::npos) continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const char* displayName = layer->layerName;
        std::optional<compat::SourceCString> formattedName;
        if (layer->replaceTexture && layer->node) {
            const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("%s \xee\x97\x88 %s"));
            const auto layerName = layer->layer.name;
            const auto nameReceiver = layer->node;
            const auto nodeName = compat::ngRequireObject(nameReceiver)->getName();
            const std::array<core::translation::TypedFormatArgument, 2> args{
                core::translation::ngFormatArgument(layerName), core::translation::ngFormatArgument(nodeName)};
            auto formatted = core::translation::ngFormatTyped(pattern.view(), args);
            const auto retained = allocateKraMemory([&] { return compat::RetainedString(std::move(formatted)); });
            compat::SourceCString next{compat::RetainedStringSlice{retained}};
            compat::detail::ngSourceCStringPostconditions(retained.view(), next.data());
            formattedName.emplace(std::move(next));
            displayName = formattedName->data();
        }
        if (layer->ignore) ngTextDisabled("\xee\x94\xbc"); else ngText("\xee\x94\xbb");
        if (ImGui::IsItemClicked()) layer->ignore = !layer->ignore;
        ImGui::SameLine(0, 8); ImGui::Selectable(displayName, false, ImGuiSelectableFlags_SpanAvailWidth);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            auto raw = layer.get(); ImGui::SetDragDropPayload("__REMAP", &raw, sizeof(raw), ImGuiCond_Always);
            ImGui::Text(layer->layerName); ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            preview([&] { return layer->layerTexture; }, layer->texturePreviewBounds, [&] {
                return ImVec2(layer->texturePreviewBounds.z, layer->texturePreviewBounds.w);
            });
            ImGui::EndTooltip();
        }
        ImGui::OpenPopupOnItemClick("LAYER_POPUP");
        if (ImGui::BeginPopup("LAYER_POPUP")) {
            if (ImGui::MenuItem(ngTranslateCString("Unmap").data())) {
                layer->replaceTexture = false;
                layer->node = compat::ngRequireObject(project::incActivePuppet())->root;
            }
            if (ImGui::MenuItem(ngTranslateCString(layer->ignore ? "Use" : "Ignore").data()))
                layer->ignore = !layer->ignore;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}
void KRAMergeWindow::treeView() {
    auto currentParts = parts;
    for (auto& part : currentParts) {
        if (!nodeFilter.empty()) {
            compat::RetainedString nodeName;
            { const auto nameReceiver = part;
              nodeName = compat::RetainedString(compat::ngRequireObject(nameReceiver)->getName()); }
            if (lower(nodeName).view().find(lower(nodeFilter.retainedString()).view()) == std::string_view::npos) continue;
        }
        {
            const auto nameReceiver = part;
            ImGui::Selectable(compat::ngRequireObject(nameReceiver)->cName(), false,
                ImGuiSelectableFlags_SpanAvailWidth);
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto payload = ImGui::AcceptDragDropPayload("__REMAP")) {
                NodeLayerBinding* target = nullptr;
                // Keep the minimum read bound while accepting larger source-compatible payloads.
                if (payload->DataSize < static_cast<int>(sizeof(target)))
                    throw std::out_of_range("KRA remap payload size");
                std::memcpy(&target, payload->Data, sizeof(target));
                auto currentBindings = bindings;
                for (auto& binding : currentBindings) if (nicxlive::core::ngResourceObjectEquals(
                    compat::ngRequireObject(binding)->node, part)) {
                    binding->node = compat::ngRequireObject(project::incActivePuppet())->root;
                    binding->replaceTexture = false;
                }
                compat::ngRequireObject(target)->node = part; target->replaceTexture = true;
                ImGui::EndDragDropTarget(); return;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            widgets::ngText(compat::RetainedStringSlice(compat::ngRequireObject(part)->getNodePathRetained()));
            const auto bounds = partPreviewBounds(part);
            preview([&] { return compat::ngRequireObject(part)->textures.at(0); }, bounds,
                [bounds] { return ImVec2(bounds.z, bounds.w); });
            ImGui::EndTooltip();
        }
    }
}
void KRAMergeWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const auto positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const auto positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    ImGui::SetNextWindowPos(ImVec2(positionX + halfWidth, positionY + halfHeight),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void KRAMergeWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using namespace widgets;
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    const auto space = ngAvailableSpace(); auto& style = ImGui::GetStyle();
    const float width = space.x / 2;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2);
    const float checkboxHeight = ImGui::GetFrameHeight(); ImGui::PopStyleVar(3);
    float footer = checkboxHeight * 3 + style.ItemSpacing.y * 2;
    if (footer < 32) footer = 32;
    float height = space.y - footer; if (height < 0) height = 0;
    const float filterHeight = ImGui::GetFrameHeight() + style.ItemSpacing.y;
    ImGui::BeginGroup();
    if (ImGui::BeginChild("###Layers", ImVec2(width, height))) {
        ngInputText("##", width - 8, layerFilter);
        ImGui::BeginListBox("###LayerList", ImVec2(width - 8, height - filterHeight)); layerView(); ImGui::EndListBox();
    }
    ImGui::EndChild(); ImGui::SameLine(0, 8);
    if (ImGui::BeginChild("###Nodes", ImVec2(width, height))) {
        ngInputText("##", width, nodeFilter);
        ImGui::BeginListBox("###NodeList", ImVec2(width, height - filterHeight)); treeView(); ImGui::EndListBox();
    }
    ImGui::EndChild(); ImGui::EndGroup();
    if (ImGui::BeginTable("###KraMergeFooter", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("###Options", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("###Actions", ImGuiTableColumnFlags_WidthFixed, 64);
        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::BeginGroup();
        ngCheckbox(ngTranslateCString("Only show unmapped").data(), &onlyUnmapped);
        ngCheckbox(ngTranslateCString("Auto-rename").data(), &renameMapped);
        ngTooltip(compat::RetainedStringSlice{ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Renames all mapped nodes to match the names of the KRA layer that was merged in to them."))});
        ngCheckbox(ngTranslateCString("Re-translate").data(), &retranslateMapped);
        ngTooltip(compat::RetainedStringSlice{ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Moves all nodes so that they visually match their position in the canvas."))});
        ImGui::EndGroup(); ImGui::TableNextColumn(); ImGui::Dummy(ImVec2(0, 8));
        if (ngButtonColored(ngTranslateCString("Merge").data(), ImVec2(64, 24))) {
            appliedTextures = true; apply(); close(); ImGui::EndTable(); return;
        }
        ImGui::EndTable();
    }
}
void KRAMergeWindow::onClose() {
    const auto receiver = weak_from_this().lock();
    file.close();
    if (!appliedTextures) {
        auto currentBindings = bindings;
        for (auto& binding : currentBindings) {
            const auto texture = compat::ngRequireObject(binding)->layerTexture;
            compat::ngRequireObject(texture)->dispose();
        }
    }
    bindings.resize(0); document = {};
}
void KRAMergeWindow::initialize(compat::RetainedString path) {
    document = io::kra::parseDocument(path);
    populateBindings();
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("KRA Merging")));
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}
}
