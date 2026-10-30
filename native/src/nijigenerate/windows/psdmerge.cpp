#include "psdmerge.hpp"
#include "psdmerge_path.hpp"
#include "psdmerge_depth_sort.hpp"
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
#include <imgui_internal.h>
#include <bit>
#include <cstring>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <compat/source_allocator.hpp>

namespace nijigenerate::windows::psdmerge {
namespace {
template<class Operation> decltype(auto) allocatePsdMergeMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
using nicxlive::core::nodes::Vec3;
using nicxlive::core::nodes::Vec4;
using nicxlive::core::nodes::Mat4;
compat::RetainedString lower(std::string_view value) {
    auto retained = allocatePsdMergeMemory([&] { return compat::RetainedString(value); });
    return io::depthmap_psd::ngPsdDepthLowerString(std::move(retained));
}
compat::RetainedString lower(compat::RetainedString value) {
    return io::depthmap_psd::ngPsdDepthLowerString(std::move(value));
}
nicxlive::core::common::NullableString retainSdkString(compat::RetainedString value) {
    return value.toNullableString();
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
    { const auto texture = compat::ngRequireObject(part)->textures.at(0);
        widthScale = 128.0f / static_cast<float>(compat::ngRequireObject(texture)->width()); }
    { const auto texture = compat::ngRequireObject(part)->textures.at(0);
        heightScale = 128.0f / static_cast<float>(compat::ngRequireObject(texture)->height()); }
    const auto scale = heightScale < widthScale ? heightScale : widthScale;
    { const auto texture = compat::ngRequireObject(part)->textures.at(0);
        width = static_cast<float>(compat::ngRequireObject(texture)->width()) * scale; }
    { const auto texture = compat::ngRequireObject(part)->textures.at(0);
        height = static_cast<float>(compat::ngRequireObject(texture)->height()) * scale; }
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
void PSDMergeWindow::populateBindings() {
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
                allocatePsdMergeMemory([&] {
                    fallback.reserve(name.size() + 1);
                    fallback.push_back('/');
                    fallback.append(name.view());
                });
            }
            const auto candidate = layerPath.empty() ? std::string_view(fallback) : layerPath.view();
            if (nameOnly ? detail::baseName(candidate) == detail::baseName(segment) : candidate == segment) return part;
        }
        return {};
    };
    compat::Slice<compat::RetainedString> path;
    std::string segment;
    const auto sourceLayers = document.layers;
    for (std::size_t index = sourceLayers.size(); index-- > 0;) {
        auto layer = sourceLayers[index];
        if (layer.type != io::psd::LayerType::Any) {
            if (layer.name.view() != "</Layer set>" && layer.name.view() != "</Layer group>") path.append(layer.name);
            else {
                // A wrapped D string[] length overflows the runtime byte count.
                if (path.empty()) throw compat::ngStaticError<compat::OutOfMemoryError>(
                    std::source_location::current());
                path.resize(path.size() - 1);
            }
            segment.clear();
            allocatePsdMergeMemory([&] {
                for (const auto& item : path) { segment += '/'; segment += item.view(); }
            });
            continue;
        }
        layer.extractLayerImage();
        auto input = layer.data;
        auto pixels = nicxlive::core::SourceTextureBytes::retainedView(
            input.retentionOwner(), input.data(), input.size());
        nicxlive::core::inTexPremultiply(pixels);
        const auto width = std::bit_cast<std::int32_t>(layer.width());
        const auto height = std::bit_cast<std::int32_t>(layer.height());
        auto texture = Texture::createSource(pixels, width, height);
        layer.data = {};
        const auto bounds = previewBounds(static_cast<float>(layer.width()), static_cast<float>(layer.height()));
        const auto fullPath = allocatePsdMergeMemory([&] {
            return compat::RetainedString(segment + "/" + std::string(layer.name.view()));
        });
        auto part = find(fullPath.view(), false); if (!part) part = find(fullPath.view(), true);
        auto binding = std::allocate_shared<NodeLayerBinding>(
            nicxlive::core::d_runtime::SourceAllocator<NodeLayerBinding>{});
        binding->layer = layer; binding->layerTexture = texture;
        binding->texturePreviewBounds = bounds;
        binding->node = part ? std::static_pointer_cast<Node>(part) : puppet->root;
        binding->replaceTexture = static_cast<bool>(part);
        binding->layerPath = fullPath;
        binding->retainLayerName(layer.name); binding->indexableName = lower(layer.name);
        bindings.append(binding);
    }
}
void PSDMergeWindow::updatePart(std::shared_ptr<Node> node) {
    const auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node);
    const auto projected = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(node);
    if (part && !projected) {
        auto& sourceMesh = part->nicxlive::core::nodes::Drawable::getMesh();
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
void PSDMergeWindow::apply() {
    const auto receiver = weak_from_this().lock();
    auto sortedBindings = bindings;
    detail::depth_sort::sort({sortedBindings.data(), sortedBindings.size()});
    const auto cx = document.width / 2, cy = document.height / 2;
    const auto puppet = project::incActivePuppet();
    const auto currentBindings = bindings;
    for (const auto binding : currentBindings) {
        if (compat::ngRequireObject(binding)->ignore) continue;
        const auto width = std::bit_cast<std::int32_t>(binding->layer.width());
        const auto height = std::bit_cast<std::int32_t>(binding->layer.height());
        const Vec3 world{static_cast<float>(addInt(binding->layer.left, width / 2)) - static_cast<float>(cx),
            static_cast<float>(addInt(binding->layer.top, height / 2)) - static_cast<float>(cy), 0};
        auto local = binding->node ? Node::getRelativePosition(
            compat::ngRequireObject(binding->node)->transformNoLock().matrix(), Mat4::translation(world)) : world;
        if (binding->replaceTexture) {
            const auto parent = compat::ngRequireObject(binding->node)->parentPtr();
            local = parent ? Node::getRelativePosition(parent->transformNoLock().matrix(), Mat4::translation(world)) : world;
            compat::ngRequireObject(binding->node)->recalcTransform = true;
            if (renameMapped) {
                const auto nameTarget = std::dynamic_pointer_cast<ExPart>(binding->node);
                compat::ngRequireObject(nameTarget)->setName(retainSdkString(binding->layer.name));
            }
            if (retranslateMapped) {
                const bool locked = compat::ngRequireObject(binding->node)->lockToRootValue();
                compat::ngRequireObject(binding->node)->localTransform.translation = locked ? world : local;
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
            if (const auto p = compat::ngRequireObject(binding->node)->parentPtr())
                p->notifyChange(binding->node, nicxlive::core::nodes::NotifyReason::StructureChanged);
        } else {
            const auto texture = binding->layerTexture;
            const auto parent = binding->node;
            const auto name = retainSdkString(binding->layer.name);
            auto part = ext::nodes::incCreateExPart(texture, parent, name);
            compat::ngRequireObject(part)->layerPath = binding->layerPath; part->localTransform.translation = local;
            if (const auto p = compat::ngRequireObject(binding->node)->parentPtr())
                p->notifyChange(binding->node, nicxlive::core::nodes::NotifyReason::StructureChanged);
        }
    }
    document = {};
    compat::ngRequireObject(compat::ngRequireObject(puppet)->root)->transformChanged();
    updatePart(compat::ngRequireObject(puppet)->root); compat::ngRequireObject(puppet)->populateTextureSlots();
}
void PSDMergeWindow::layerView() {
    using namespace widgets;
    const auto layerCount = bindings.size();
    for (std::size_t i = 0; i < layerCount; ++i) {
        if (i >= bindings.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, bindings.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/psdmerge.d"), 240);
        const auto layer = bindings[i];
        const auto root = compat::ngRequireObject(project::incActivePuppet())->root;
        const bool unmapped = !compat::ngRequireObject(layer)->replaceTexture || !layer->node || layer->node == root;
        if (onlyUnmapped && !unmapped) continue;
        if (!layerFilter.empty()) {
            const auto indexableName = layer->indexableName;
            const auto filter = lower(layerFilter.retainedString());
            if (indexableName.view().find(filter.view()) == std::string_view::npos) continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const char* displayName = layer->layerName;
        std::optional<compat::SourceCString> formattedName;
        if (layer->replaceTexture && layer->node) {
            const auto pattern = core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("%s \xee\x97\x88 %s"));
            const auto layerName = layer->layer.name;
            const auto nameReceiver = layer->node;
            const auto nodeName = compat::ngRequireObject(nameReceiver)->getName();
            const std::array<core::translation::TypedFormatArgument, 2> args{
                core::translation::ngFormatArgument(layerName), core::translation::ngFormatArgument(nodeName)};
            auto formatted = core::translation::ngFormatTyped(pattern.view(), args);
            const auto retained = allocatePsdMergeMemory([&] { return compat::RetainedString(std::move(formatted)); });
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
            const auto unmap = core::translation::ngTranslateCString("Unmap");
            if (ImGui::MenuItem(unmap.data())) {
                layer->replaceTexture = false;
                layer->node = compat::ngRequireObject(project::incActivePuppet())->root;
            }
            const auto ignore = core::translation::ngTranslateCString(layer->ignore ? "Use" : "Ignore");
            if (ImGui::MenuItem(ignore.data())) layer->ignore = !layer->ignore;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}
void PSDMergeWindow::treeView() {
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
            const auto name = compat::ngRequireObject(nameReceiver)->cNameRetained();
            ImGui::Selectable(name.data(), false, ImGuiSelectableFlags_SpanAvailWidth);
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto payload = ImGui::AcceptDragDropPayload("__REMAP")) {
                NodeLayerBinding* target = nullptr;
                std::memcpy(&target, compat::ngRequireObject(payload->Data), sizeof(target));
                auto currentBindings = bindings;
                for (auto& binding : currentBindings) if (nicxlive::core::ngResourceObjectEquals(
                        compat::ngRequireObject(binding)->node, part)) {
                    compat::ngRequireObject(binding)->node = compat::ngRequireObject(project::incActivePuppet())->root;
                    binding->replaceTexture = false;
                }
                compat::ngRequireObject(target)->node = part; target->replaceTexture = true;
                ImGui::EndDragDropTarget(); return;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            widgets::ngText(compat::RetainedStringSlice(
                compat::ngRequireObject(part)->getNodePathRetained()));
            const auto bounds = partPreviewBounds(part);
            preview([&] { return compat::ngRequireObject(part)->textures.at(0); }, bounds,
                [bounds] { return ImVec2(bounds.z, bounds.w); });
            ImGui::EndTooltip();
        }
    }
}
void PSDMergeWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const auto xPosition = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto xSize = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const float x = xPosition + xSize / 2;
    const auto yPosition = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto ySize = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    const float y = yPosition + ySize / 2;
    ImGui::SetNextWindowPos(ImVec2(x, y),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void PSDMergeWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using namespace widgets;
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
    if (ImGui::BeginTable("###PsdMergeFooter", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("###Options", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("###Actions", ImGuiTableColumnFlags_WidthFixed, 96);
        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::BeginGroup();
        const auto unmapped = core::translation::ngTranslateCString("Only show unmapped");
        ngCheckbox(unmapped.data(), &onlyUnmapped);
        const auto rename = core::translation::ngTranslateCString("Auto-rename");
        ngCheckbox(rename.data(), &renameMapped);
        const auto renameTip = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Renames all mapped nodes to match the names of the PSD layer that was merged in to them."));
        ngTooltip(compat::RetainedStringSlice(renameTip));
        const auto retranslate = core::translation::ngTranslateCString("Re-translate");
        ngCheckbox(retranslate.data(), &retranslateMapped);
        const auto retranslateTip = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Moves all nodes so that they visually match their position in the canvas."));
        ngTooltip(compat::RetainedStringSlice(retranslateTip));
        ImGui::EndGroup(); ImGui::TableNextColumn(); ImGui::Dummy(ImVec2(0, 8));
        const auto merge = core::translation::ngTranslateCString("Merge");
        if (ngButtonColored(merge.data(), ImVec2(96, 24))) {
            appliedTextures = true; apply(); close(); ImGui::EndTable(); return;
        }
        ImGui::EndTable();
    }
}
void PSDMergeWindow::onClose() {
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
    // Ordinary C++ owners release these resources; no collection phase is used.
}
void PSDMergeWindow::initialize(compat::RetainedString path) {
    file = io::psd::RetainedFile(path);
    document = io::psd::parseDocument(file);
    populateBindings(); base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("PSD Merging")));
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}
}
