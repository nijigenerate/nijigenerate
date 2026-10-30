#include "inpexport.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/io/imagefmt/reader.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::windows::inpexport {
using namespace widgets;
using core::translation::ngTranslateRetained;
using core::translation::ngTranslateCString;
using nicxlive::core::nodes::BlendMode;
namespace {
template<class Allocate> auto allocateExportText(Allocate allocate) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString retainExportText(std::string text) {
    return allocateExportText([&] { return compat::RetainedString(std::move(text)); });
}
struct BlendChoice { BlendMode mode; const char* name; const char* title; };
constexpr std::array blending{
    BlendChoice{BlendMode::Normal,"Normal","Normal"},
    BlendChoice{BlendMode::Multiply,"Multiply","Multiply"},
    BlendChoice{BlendMode::Screen,"Screen","Screen"},
    BlendChoice{BlendMode::Overlay,"Overlay","Overlay"},
    BlendChoice{BlendMode::Darken,"Darken","Darken"},
    BlendChoice{BlendMode::Lighten,"Lighten","Lighten"},
    BlendChoice{BlendMode::ColorDodge,"ColorDodge","Color Dodge"},
    BlendChoice{BlendMode::LinearDodge,"LinearDodge","Linear Dodge"},
    BlendChoice{BlendMode::AddGlow,"AddGlow","Add (Glow)"},
    BlendChoice{BlendMode::ColorBurn,"ColorBurn","Color Burn"},
    BlendChoice{BlendMode::HardLight,"HardLight","Hard Light"},
    BlendChoice{BlendMode::SoftLight,"SoftLight","Soft Light"},
    BlendChoice{BlendMode::Subtract,"Subtract","Subtract"},
    BlendChoice{BlendMode::Difference,"Difference","Difference"},
    BlendChoice{BlendMode::Exclusion,"Exclusion","Exclusion"},
    BlendChoice{BlendMode::Inverse,"Inverse","Inverse"},
    BlendChoice{BlendMode::DestinationIn,"DestinationIn","Destination In"},
    BlendChoice{BlendMode::ClipToLower,"ClipToLower","Clip to Lower"}
};
template<class MakeMessage> void errorDialog(MakeMessage makeMessage, bool texture) {
    using namespace widgets::dialog;
    const auto title=ngTranslateCString("Error");
    const auto retained=title.owner.use_count() ? DialogTag::retained(title.data(),title.owner)
        : DialogTag::borrowed(title.data());
    const auto message=makeMessage();
    if(texture) ngDialog(DialogTag::borrowed("WRONG_TEX_FMT"),retained,DialogText(message));
    else ngDialog(retained,DialogText(message));
}
}
void ExportWindow::beginSection(compat::RetainedStringSlice title) {
    const auto receiver=weak_from_this().lock();
    ngText(title); ngDummy(ImVec2(0,4)); ImGui::Indent();
}
void ExportWindow::endSection() {
    const auto receiver=weak_from_this().lock();
    ImGui::Unindent(); ImGui::NewLine();
}
void ExportWindow::regenPreview() {
    const auto receiver=weak_from_this().lock();
    preview=io::inpexport::incINPExportGenPreview(project::incActivePuppet(),settings);
    wasScaledForced=preview.outputScale!=1;
    if(wasScaledForced) settings.scale=preview.outputScale;
}
void ExportWindow::setBlending(BlendMode mode) {
    const auto receiver=weak_from_this().lock();
    settings.decorateWatermarkBlendMode=mode; regenPreview();
}
void ExportWindow::loadWatermark(compat::RetainedString file) {
    const auto receiver=weak_from_this().lock();
    nicxlive::core::ShallowTexture texture(file, 4);
    nicxlive::core::inTexPremultiply(texture.data);
    settings.decorateWatermark=nicxlive::core::Texture::createSource(texture);
    const auto watermark=compat::ngRequireObject(settings.decorateWatermark);
    watermark->setWrapping(nicxlive::core::Wrapping::Repeat);
    regenPreview();
}
void ExportWindow::onBeginUpdate() {
    const auto receiver=weak_from_this().lock();
    flags|=ImGuiWindowFlags_NoSavedSettings;
    const auto posX=compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto sizeX=compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const auto posY=compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto sizeY=compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    const auto x=posX+sizeX/2;
    const auto y=posY+sizeY/2;
    const ImVec2 position(x,y);
    const ImVec2 size(512,384);
    ImGui::SetNextWindowPos(position,ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(size,ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(size,ImVec2(std::numeric_limits<float>::max(),std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void ExportWindow::onUpdate() {
    const auto receiver=weak_from_this().lock();
    float availX=ngAvailableSpace().x;
    if(ImGui::BeginChild("SettingsSidebar",ImVec2(static_cast<float>(availX/3.5),-28),true)) {
        ImGui::PushTextWrapPos(128);
        if(ImGui::Selectable(ngTranslateCString("Atlassing").data(),pane==ExportOptionsPane::Atlassing)) pane=ExportOptionsPane::Atlassing;
        if(ImGui::Selectable(ngTranslateCString("Decoration").data(),pane==ExportOptionsPane::Decoration)) pane=ExportOptionsPane::Decoration;
        ImGui::PopTextWrapPos();
    } ImGui::EndChild(); ImGui::SameLine(0,4);
    if(ImGui::BeginChild("SettingsContent",ImVec2(0,-28),true)) {
        const auto avail=ngAvailableSpace();
        beginSection(ngTranslateRetained(pane.value_));
        ImGui::PushItemWidth(avail.x/2);
        if(pane==ExportOptionsPane::Atlassing) {
            const float previewSize=avail.y/2;
            ImGui::Unindent(); ngDummy(ImVec2(avail.x/2-previewSize/2,previewSize)); ImGui::SameLine();
            ngTextureSlotUntitled("PREVIEW0",preview.preview,ImVec2(previewSize,previewSize),64); ImGui::Indent();
            ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Atlas Settings"))); ImGui::Indent();
            if(ImGui::BeginCombo(ngTranslateCString("Resolution").data(),atlasResolutionString)) {
                std::size_t size=1024;
                for(int i=0;i<3;++i) {
                    size<<=1;
                    const auto text=allocateExportText([&] {
                        return compat::RetainedString(std::to_string(size)+"x"+std::to_string(size));
                    });
                    const compat::SourceCString sizeString{compat::RetainedStringSlice(text)};
                    compat::detail::ngSourceCStringPostconditions(text.view(), sizeString.data());
                    if(ImGui::MenuItem(sizeString.data(),nullptr,settings.atlasResolution==size)) {
                        settings.atlasResolution=size;
                        atlasResolutionStringOwner=sizeString.retainedString();
                        atlasResolutionString=sizeString.data();
                        regenPreview();
                    }
                } ImGui::EndCombo();
            }
            ngCheckbox(ngTranslateCString("Non-linear Scaling").data(),&settings.nonLinearScaling);
            ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "Whether too large parts should individually be scaled down instead of all parts being scaled down uniformly.")));
            int scale=compat::ngFloatToInt32(settings.scale*100.f);
            if(ImGui::InputInt(ngTranslateCString("Texture Scale").data(),&scale,1,10)) {
                scale=std::clamp(scale,25,200); settings.scale=static_cast<float>(static_cast<float>(scale)/100.0); regenPreview();
            }
            if(ImGui::InputInt(ngTranslateCString("Padding").data(),&settings.padding,1,10)) {
                settings.padding=std::clamp(settings.padding,0,std::numeric_limits<int>::max()); regenPreview();
            } ImGui::Unindent();
            ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Optimizations"))); ImGui::Indent();
            ngCheckbox(ngTranslateCString("Prune unused nodes").data(),&settings.optimizePruneUnused);
            ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "Prune nodes which have been disabled from the export."))); ImGui::Unindent();
        }
        else if(pane==ExportOptionsPane::Decoration) {
            const float size=std::min(avail.x/2,avail.y/2);
            const float offset=size*2>=avail.x ? 0 : avail.x/2;
            ImGui::Unindent();
            if(offset>0) {ngDummy(ImVec2(avail.x/2-size,size)); ImGui::SameLine(0,0);}
            ngTextureSlot(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Watermark")),settings.decorateWatermark,ImVec2(size,size));
            ImGui::OpenPopupOnItemClick("TEX_OPTIONS");
            if(ImGui::BeginPopup("TEX_OPTIONS")) {
                if(ImGui::MenuItem(ngTranslateCString("Load").data())) {
                    compat::Slice<io::TFD_Filter> filters{
                        io::TFD_Filter{}, io::TFD_Filter{}, io::TFD_Filter{}};
                    const compat::Slice<const char*> png{"*.png"};
                    filters[0] = {{png.data(), png.size()}, "Portable Network Graphics (*.png)"};
                    const compat::Slice<const char*> jpg{"*.jpeg", "*.jpg"};
                    filters[1] = {{jpg.data(), jpg.size()}, "JPEG Image (*.jpeg)"};
                    const compat::Slice<const char*> tga{"*.tga"};
                    filters[2] = {{tga.data(), tga.size()}, "TARGA Graphics (*.tga)"};
                    auto file=io::incShowImportDialog({filters.data(),filters.size()},
                        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Import...")));
                    if(!file.view().empty()) try {loadWatermark(file.retainedString());}
                    catch(const compat::ThrowableException& ex) {errorDialog([&ex] { return ex.msg; },true);}
                }
                if(ImGui::MenuItem(ngTranslateCString("Remove").data(),nullptr,false,settings.decorateWatermark!=nullptr)) {
                    settings.decorateWatermark.reset(); regenPreview();
                } ImGui::EndPopup();
            }
            if(ImGui::BeginDragDropTarget()) {
                if(const auto* payload=ImGui::AcceptDragDropPayload("__PARTS_DROP")) {
                    const auto& payloadFiles=*compat::ngRequireObject(
                        static_cast<const core::WindowFileDropPayload*>(payload->Data));
                    const auto files=core::ngRetainFileDropPayload(payloadFiles);
                    if(!files.empty()) try {loadWatermark(files[0].retainedString());}
                    catch(const compat::ThrowableException& ex) {errorDialog([&ex] { return ex.msg; },true);}
                    core::ngFinishFileDrag();
                } ImGui::EndDragDropTarget();
            }
            ImGui::SameLine(); ngTextureSlot(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Preview")),preview.preview,ImVec2(size,size),64); ImGui::Indent();
            ImGui::BeginDisabled(!settings.decorateWatermark);
            ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Blending"))); ImGui::Indent();
            auto selected=allocateExportText([&] {
                return std::to_string(static_cast<int>(settings.decorateWatermarkBlendMode));
            });
            for(const auto& choice:blending) if(choice.mode==settings.decorateWatermarkBlendMode)
                allocateExportText([&] { selected=choice.name; });
            if(ImGui::BeginCombo("###Blending",ngTranslateCString(selected).data())) {
                for(const auto& choice:blending) {
                    if(ImGui::Selectable(ngTranslateCString(choice.title).data(),settings.decorateWatermarkBlendMode==choice.mode)) setBlending(choice.mode);
                    if(choice.mode==BlendMode::Inverse) ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                        "Inverts the color by a factor of the overlying color")));
                    if(choice.mode==BlendMode::ClipToLower) ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                        "Special blending mode that causes (while respecting transparency) the part to be clipped to everything underneath")));
                } ImGui::EndCombo();
            } ImGui::Unindent();
            ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Loops"))); ImGui::Indent();
            int loops=std::bit_cast<int>(settings.decorateWatermarkLoops);
            const bool changed=ImGui::DragInt("###LOOPS",&loops,1,1,1000);
            settings.decorateWatermarkLoops=std::bit_cast<std::uint32_t>(loops);
            if(changed) regenPreview(); ImGui::Unindent();
            ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Opacity"))); ImGui::Indent();
            if(ImGui::DragFloat("###OPACITY",&settings.decorateWatermarkOpacity,0.01f,0,1)) regenPreview();
            ImGui::Unindent(); ImGui::EndDisabled();
        }
        else ngText(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("No settings for this category.")));
        ImGui::PopItemWidth();
    } ImGui::EndChild();
    if(ImGui::BeginChild("SettingsButtons",ImVec2(0,0),false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        availX=ngAvailableSpace().x;
        if(wasScaledForced) {
            ImGui::PushTextWrapPos(availX-128);
            ngTextColored(ImVec4(0.8f,0.2f,0.2f,1),ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "A texture was too large to fit the texture atlas, the textures have been scaled down.")));
            ImGui::PopTextWrapPos();
        } ImGui::SameLine(0,0); ngDummy(ImVec2(-64,0)); ImGui::SameLine(0,0);
        if(ngButtonColored(ngTranslateCString("Export").data(),ImVec2(64,24))) {
            try {
                io::inpexport::incINPExport(project::incActivePuppet(),settings,compat::RetainedStringSlice(outFile));
                const std::array<core::translation::TypedFormatArgument,1> arguments{core::translation::ngFormatArgument(outFile)};
                core::tasks::ngSetStatus(compat::RetainedStringSlice(ngTranslateRetained(
                    retainExportText(core::translation::ngFormatTyped("%s was exported...",arguments)))));
                close();
            } catch(const compat::ThrowableException& ex) {
                errorDialog([&ex] { return ex.msg; },false);
                core::tasks::ngSetStatus(compat::RetainedStringSlice(ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("Export failed..."))));
            }
        }
    } ImGui::EndChild();
}
void ExportWindow::initialize(compat::RetainedString file) {
    const auto receiver=weak_from_this().lock();
    base::Window::initialize(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Export Options")));
    outFile=std::move(file); regenPreview();
}
}
