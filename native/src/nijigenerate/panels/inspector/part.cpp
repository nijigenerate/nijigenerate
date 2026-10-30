#include "part.hpp"
#include <nijigenerate/core/math/welding.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
namespace nijigenerate::panels::inspector::part {
namespace {
using String = compat::RetainedStringSlice;
std::shared_ptr<Texture>& textureSlot(const std::shared_ptr<Part>& part, std::size_t index, std::size_t line) {
    auto& slots = compat::ngRequireObject(part)->textures;
    if (index >= slots.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, slots.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/part.d"), line);
    return slots[index];
}
String extension(String file) {
    return file.slice(platform::ngAutosaveStripExtension(file.view()).size(), file.size());
}
template<class Message> void errorDialog(Message&& makeMessage) {
    namespace dialog = widgets::dialog;
    const auto title = core::translation::ngTranslateCString("Error");
    auto message = std::forward<Message>(makeMessage)();
    dialog::ngDialog(dialog::DialogTag::retained(title.data(), title.owner),
        dialog::DialogText(message.retainedString()));
}
void unsupported(String file) {
    errorDialog([file] {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("%s is not supported"));
        const std::array<core::translation::TypedFormatArgument, 1> args{
            core::translation::ngFormatArgument(file)};
        return String(composite::detail::retainedText(core::translation::ngFormatTyped(pattern.view(), args)));
    });
}
void applyTextureToSlot(std::shared_ptr<Part> part, TextureUsage usage, String file) {
    const auto ext = io::imageexport::detail::lowerExtension(extension(file));
    if (ext.view() != ".png" && ext.view() != ".tga" && ext.view() != ".jpeg" && ext.view() != ".jpg") {
        unsupported(file);
        return;
    }
    const auto index = static_cast<std::size_t>(usage);
    try {
        const int channels = usage == TextureUsage::Albedo ? 4 :
            usage == TextureUsage::Emissive || usage == TextureUsage::Bumpmap ? 3 : 0;
        auto tex = nicxlive::core::ShallowTexture(file.retainedString(), channels);
        if (usage != TextureUsage::Albedo) {
            if (tex.width != compat::ngRequireObject(textureSlot(part, 0, 56))->width() ||
                tex.height != compat::ngRequireObject(textureSlot(part, 0, 56))->height()) {
                errorDialog([] { return String(core::translation::ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral(
                        "Size of given texture does not match the Albedo texture."))); });
                return;
            }
        }
        if (tex.convChannels == 4)
            core::texture_pixels::ngTexPremultiply(tex.data);
        auto& destination=textureSlot(part,index,65);
        destination=Texture::createSourceDeferred([&] { return tex; });
        if (usage == TextureUsage::Albedo) {
            // The source deliberately enumerates the tail slice with index 0,
            // then reads slots[i], not slots[i + 1]. Keep that authored quirk.
            const auto tail = [&] {
                const auto slots = compat::ngRequireObject(part)->textures;
                if (slots.empty()) throw compat::ngStaticError<compat::ArraySliceError>(1, slots.size(), slots.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/part.d"), 68);
                return slots.slice(1, slots.size());
            }();
            for (std::size_t i = 0; i < tail.size(); ++i) {
                [[maybe_unused]] const auto ignored = tail[i];
                if (textureSlot(part, i, 69) &&
                    (compat::ngRequireObject(textureSlot(part, i, 69))->width() !=
                        compat::ngRequireObject(textureSlot(part, 0, 69))->width() ||
                    compat::ngRequireObject(textureSlot(part, i, 69))->height() !=
                        compat::ngRequireObject(textureSlot(part, 0, 69))->height()))
                    textureSlot(part, i, 70).reset();
            }
        }
    } catch (const compat::ThrowableException& exception) {
        const auto message = exception.msg;
        if (message.size() < 11)
            throw compat::ngStaticError<compat::ArraySliceError>(0, 11, message.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/part.d"), 75);
        if (message.view().substr(0, 11) == "unsupported") unsupported(file);
        else errorDialog([message] { return String(message); });
    }
    compat::ngRequireObject(project::incActivePuppet())->rescanNodes();
    compat::ngRequireObject(project::incActivePuppet())->populateTextureSlots();
    compat::ngRequireObject(project::incActivePuppet())->updateTextureState();
}
}
void incInspectorTextureSlot(std::shared_ptr<Part> part, TextureUsage usage, String title, ImVec2 size) {
    using core::translation::ngTranslate;
    using core::translation::ngTranslateCString;
    const auto index = static_cast<std::size_t>(usage);
    ImGui::PushID(static_cast<int>(compat::ngRequireObject(part)->getUUID()));
    ImGui::PushID(static_cast<int>(static_cast<std::uint32_t>(usage)));
    widgets::ngTextureSlot(title, textureSlot(part, index, 30), size);
    if (textureSlot(part, index, 94)) {
        ImGui::OpenPopupOnItemClick("TEX_OPTIONS");
        if (ImGui::BeginPopup("TEX_OPTIONS")) {
            if (ImGui::MenuItem(ngTranslateCString("Save to File").data())) {
                const char* patterns[]{"*.png"};
                const io::TFD_Filter filters[]{{patterns, "PNG File"}};
                String file = io::incShowSaveDialog(filters, "texture.png");
                if (!file.isNull()) {
                    if (extension(file).empty()) {
                        try {
                            file = compat::RetainedString(std::string(file.view()) + ".png");
                        } catch (const std::bad_alloc&) {
                            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                        } catch (const std::length_error&) {
                            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                        }
                    }
                    compat::ngRequireObject(textureSlot(part, index, 108))->save(file.retainedString());
                }
            }
            if (ImGui::MenuItem(ngTranslateCString("Load from File").data())) {
                const char* png[]{"*.png"}; const char* jpg[]{"*.jpeg", "*.jpg"}; const char* tga[]{"*.tga"};
                const io::TFD_Filter filters[]{{png, "Portable Network Graphics (*.png)"},
                    {jpg, "JPEG Image (*.jpeg)"}, {tga, "TARGA Graphics (*.tga)"}};
                const auto file = io::incShowImportDialog(filters, String(core::translation::ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("Import..."))));
                if (!file.isNull()) applyTextureToSlot(part, usage, file);
            }
            if (usage != TextureUsage::Albedo) {
                if (ImGui::MenuItem(ngTranslateCString("Remove").data())) {
                    textureSlot(part, index, 128).reset();
                    compat::ngRequireObject(project::incActivePuppet())->rescanNodes();
                    compat::ngRequireObject(project::incActivePuppet())->populateTextureSlots();
                }
            } else if (ImGui::MenuItem(ngTranslateCString("Make Emissive").data())) {
                auto& destination=textureSlot(part,static_cast<std::size_t>(TextureUsage::Emissive),137);
                auto texture = Texture::createSourceDeferred([&] {
                    const auto data = compat::ngRequireObject(textureSlot(part, index, 139))->getSourceTextureData(true);
                    const auto width = compat::ngRequireObject(textureSlot(part, index, 140))->width();
                    const auto height = compat::ngRequireObject(textureSlot(part, index, 141))->height();
                    return nicxlive::core::ShallowTexture(data, width, height, 4, 3);
                });
                destination=std::move(texture);
                compat::ngRequireObject(project::incActivePuppet())->rescanNodes();
                compat::ngRequireObject(project::incActivePuppet())->populateTextureSlots();
                compat::ngRequireObject(project::incActivePuppet())->updateTextureState();
            }
            ImGui::EndPopup();
        }
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload("__PARTS_DROP")) {
            const auto descriptor = *compat::ngRequireObject(
                static_cast<const core::WindowFileDropPayload*>(payload->Data));
            const auto files = core::ngRetainFileDropPayload(descriptor);
            if (!files.empty()) applyTextureToSlot(part, usage, files[0]);
            core::ngFinishFileDrag();
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopID(); ImGui::PopID();
}
nicxlive::core::common::SharedArray<std::ptrdiff_t> incRegisterWeldedPoints(std::shared_ptr<Drawable> node,
    std::shared_ptr<Drawable> counter,float weight) {
    core::actionstack::incActionPushGroup();
    actions::node::Indices retained;
    compat::ngRunWithScopeExit([&] {
        compat::Slice<nicxlive::core::math::Vec2> source,target;
        auto sourceVertices=compat::ngRequireObject(node)->getVertices();
        for(std::size_t i=0;i<sourceVertices.size();++i) {
            const auto vertex=sourceVertices.at(i);
            auto position=core::math::ngTransformHomogeneousPoint(compat::ngRequireObject(node)->transform().matrix(),{vertex.x,vertex.y,0});
            source.append({position.x,position.y});
        }
        auto targetVertices=compat::ngRequireObject(counter)->getVertices();
        for(std::size_t i=0;i<targetVertices.size();++i) {
            const auto vertex=targetVertices.at(i);
            auto position=core::math::ngTransformHomogeneousPoint(compat::ngRequireObject(counter)->transform().matrix(),{vertex.x,vertex.y,0});
            target.append({position.x,position.y});
        }
        auto indices=core::math::welding::ngMatchWeldingVertices(
            {source.data(),source.size()},{target.data(),target.size()});
        retained=actions::node::Indices::retainedView(indices.retentionOwner(),indices.data(),indices.size());
        if(compat::ngRequireObject(node)->isWeldedBy(counter)) core::actionstack::incActionPush(
            actions::node::DrawableChangeWeldingAction::create(
                node, counter, retained, weight, actions::node::Indices{}, true));
        else core::actionstack::incActionPush(std::allocate_shared<actions::node::DrawableAddWeldingAction>(
            nicxlive::core::d_runtime::SourceAllocator<actions::node::DrawableAddWeldingAction>{},
            node,counter,retained,weight));
    }, [] { core::actionstack::incActionPopGroup(); });
    return retained;
}
}
