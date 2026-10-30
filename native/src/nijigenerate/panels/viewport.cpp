#include "viewport.hpp"
#include <nijigenerate/viewport/main_viewport.hpp>
#include <nijigenerate/viewport/model/onionslice.hpp>
#include <nijigenerate/viewport/asyncderivedupdateoverlay.hpp>
#include <nijigenerate/widgets/asyncderivedupdatedetails.hpp>
#include <nijigenerate/widgets/viewport.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/commands/viewport/control.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/psd.hpp>
#include <nijigenerate/io/kra.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <runtime_state.hpp>
#include <timing.hpp>
#include <math/camera.hpp>
#include <cmath>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::panels::viewport {
namespace view = nijigenerate::viewport::base;
namespace control = commands::viewport::control;
using core::translation::ngTranslateRetained;
using namespace widgets;
using namespace project;
namespace {
float clampSize(float value) { return value<128?128:value>std::numeric_limits<float>::max()?std::numeric_limits<float>::max():value; }
std::int32_t integer(float value) {
    if(!std::isfinite(value)||value< -2147483648.0f||value>=2147483648.0f)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
template<class... Args> std::string format(std::string_view text,const Args&... args) {
    const std::array values{core::translation::ngFormatArgument(args)...};
    return core::translation::ngFormatTyped(text,values);
}
void run(control::ViewportCommand id,bool withPuppet=true) {
    auto context=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    if(withPuppet && incActivePuppet()) context->puppet(incActivePuppet());
    switch (id) {
#define NG_DISPATCH_VIEWPORT(Name) \
    case control::ViewportCommand::Name: \
        (void)commands::ngDispatchRegisteredCommand<control::Name##Command>( \
            control::commands, id, context, {"ViewportCommand." #Name, "ViewportCommand", #Name "Command"}); \
        return
    NG_DISPATCH_VIEWPORT(ToggleMirrorView);
    NG_DISPATCH_VIEWPORT(ToggleOnionSlice);
    NG_DISPATCH_VIEWPORT(TogglePhysics);
    NG_DISPATCH_VIEWPORT(TogglePostProcess);
    NG_DISPATCH_VIEWPORT(ResetPhysics);
    NG_DISPATCH_VIEWPORT(ResetParameters);
    NG_DISPATCH_VIEWPORT(OpenFlipPairWindow);
    NG_DISPATCH_VIEWPORT(OpenAutomeshBatching);
    NG_DISPATCH_VIEWPORT(ResetViewportZoom);
    NG_DISPATCH_VIEWPORT(ResetViewportPosition);
    NG_DISPATCH_VIEWPORT(FitViewportToModel);
#undef NG_DISPATCH_VIEWPORT
    default: throw std::logic_error("Native viewport caller supplied an untranslated command expression");
    }
}
compat::RetainedStringSlice retainedText(std::string text) {
    // The formatter has already returned; only its retained backing is allocated here.
    try { return compat::RetainedStringSlice(compat::RetainedString(std::move(text))); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
void ngInitializeViewportPanelModule() { ngInitializePanelModule<ViewportPanel>(); }
ViewportPanel::ViewportPanel():Panel(compat::RetainedString::fromStaticLiteral("Viewport"),
    ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Viewport")),true) {
    supportsReadOnlyInteraction=true;alwaysVisible=true;
}
void ViewportPanel::onBeginUpdate() {
    ImGuiWindowClass windowClass;
    // D uses the binding struct's zero defaults, not ImGui's C++ constructor
    // overrides (-1 parent and permission to merge with unclassed windows).
    windowClass.ParentViewportId=0;
    windowClass.DockingAllowUnclassed=false;
    windowClass.DockNodeFlagsOverrideSet=ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&windowClass);
    priorWindowPadding=ImGui::GetStyle().WindowPadding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,2));
    ImGui::SetNextWindowDockID(core::ngGetViewportDockSpace(),ImGuiCond_Always);
    flags|=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    Panel::onBeginUpdate();
}
void ViewportPanel::onEndUpdate() { Panel::onEndUpdate();ImGui::PopStyleVar(); }
void ViewportPanel::onUpdate() {
    const auto receiver=shared_from_this();
    const bool interactionAllowed=!api::mcp::task::ngMcpExternalCommandsBlocked();
    auto& input=ImGui::GetIO();auto camera=nicxlive::core::inGetCameraHandle();
    auto* drawList=ImGui::GetWindowDrawList();auto* window=ImGui::GetCurrentWindow();
    auto size=ImGui::GetContentRegionAvail();
    if(std::isnan(size.x)||std::isnan(size.y)) size={0,0};
    size={clampSize(size.x),clampSize(size.y)};
    for(int button=0;button<ImGuiMouseButton_COUNT;++button) {
        if(!interactionAllowed&&button!=ImGuiMouseButton_Right) {
            view::ngEndDrag(button);view::ngEndDragInViewport(button);continue;
        }
        if(!view::ngStartedDrag(button)&&input.MouseDown[button]) {
            if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) view::ngBeginDragInViewport(button);
            view::ngBeginDrag(button);
        }
        if(view::ngStartedDrag(button)&&!input.MouseDown[button]) {
            view::ngEndDrag(button);view::ngEndDragInViewport(button);
        }
    }
    if(ImGui::BeginChild("##ViewportView",{0,-38},ImGuiChildFlags_None,flags)) {
        auto viewport=view::ngViewport();size=ImGui::GetContentRegionAvail();
        size={clampSize(size.x),clampSize(size.y)};
        if(size.x!=lastSize.x||size.y!=lastSize.y)
            nicxlive::core::inSetViewport(integer(size.x*core::dpi::incGetUIScale()),integer(size.y*core::dpi::incGetUIScale()));
        compat::ngRequireObject(viewport)->poll();
        ImGuiHoveredFlags hoverFlags=ImGuiHoveredFlags_None;
        if(actingInViewport) hoverFlags|=ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
        if(ImGui::IsWindowHovered(hoverFlags)) {
            actingInViewport=ImGui::IsMouseDown(ImGuiMouseButton_Left)||ImGui::IsMouseDown(ImGuiMouseButton_Middle)||ImGui::IsMouseDown(ImGuiMouseButton_Right);
            compat::ngRequireObject(viewport)->update(false,!interactionAllowed);
        } else if(interactionAllowed&&(compat::ngRequireObject(view::ngViewport())->alwaysUpdate()||
            (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Tab)))) compat::ngRequireObject(viewport)->update(true);
        if(view::shouldMirrorViewport) { compat::ngRequireObject(camera)->scale.x*=-1;compat::ngRequireObject(viewport)->draw();compat::ngRequireObject(camera)->scale.x*=-1; }
        else compat::ngRequireObject(viewport)->draw();
        int width,height;nicxlive::core::inGetViewport(width,height);
        const auto backgroundColor=core::ngViewportGetBackgroundColor();
        compat::ngRequireObject(window);
        ImRect rect({window->InnerRect.Max.x-1,window->InnerRect.Max.y},
            {window->InnerRect.Min.x+1,window->InnerRect.Max.y+size.y});
        compat::ngRequireObject(drawList)->AddRectFilled(rect.Min,rect.Max,ImGui::GetColorU32(backgroundColor));
        drawList->AddImage(static_cast<ImTextureID>(nicxlive::core::inGetRenderImage()),rect.Min,rect.Max,
            {static_cast<float>(0.5/width),static_cast<float>(1-0.5/height)},
            {static_cast<float>(1-0.5/width),static_cast<float>(0.5/height)},0xffffffff);
        compat::ngRequireObject(viewport)->drawOverlay(drawList,
            compat::ngRequireObject(ImGui::GetCurrentWindow())->InnerRect.Max,rect);
        const auto channel=compat::ngRequireObject(viewport)->asyncDerivedUpdateViewportChannel();auto project=incActiveProject();
        compat::Slice<core::asyncderivedupdate::AsyncDerivedUpdateSnapshot> snapshots;
        if(project&&channel!=0) {
            snapshots=core::asyncderivedupdate::incAsyncDerivedUpdateSnapshots(project->derivedUpdateScope,channel);
            nijigenerate::viewport::asyncderivedupdateoverlay::drawAsyncDerivedUpdateViewportOverlay(
                snapshots,
                drawList,compat::ngRequireObject(ImGui::GetCurrentWindow())->InnerRect.Max,rect);
        }
        ImGui::ItemAdd(rect,ImGui::GetID("###VIEWPORT_DISP"));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,priorWindowPadding);
        if(interactionAllowed&&compat::ngRequireObject(viewport)->hasMenu()) {
            static thread_local ImVec2 downPosition{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
            if(ImGui::IsItemHovered()) {
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) downPosition=ImGui::GetMousePos();
                if(!ImGui::IsPopupOpen("ViewportMenu")&&ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    const auto current=ImGui::GetMousePos();const float dx=downPosition.x-current.x,dy=downPosition.y-current.y;
                    const float distance=std::sqrt(dx*dx+dy*dy);
                    if(distance<16) {compat::ngRequireObject(viewport)->menuOpening();ImGui::OpenPopup("ViewportMenu");}
                }
            }
            if(ImGui::BeginPopup("ViewportMenu")) {compat::ngRequireObject(viewport)->menu();ImGui::EndPopup();}
        }
        ImGui::PopStyleVar();ImGui::BeginDisabled(!interactionAllowed);
        ngBeginViewportToolArea("ToolArea",ImGuiDir_Left);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6,6));compat::ngRequireObject(viewport)->drawTools();ImGui::PopStyleVar();ngEndViewportToolArea();
        ngBeginViewportToolArea("OptionsArea",ImGuiDir_Right);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6,6));compat::ngRequireObject(viewport)->drawOptions();ImGui::PopStyleVar();ngEndViewportToolArea();
        ngBeginViewportToolArea("ConfirmArea",ImGuiDir_Left,ImGuiDir_Down,false);
        if(project&&channel!=0) asyncderivedupdatedetails::drawAsyncDerivedUpdateDetailsUi(snapshots);
        compat::ngRequireObject(viewport)->drawConfirmBar();ngEndViewportToolArea();ImGui::EndDisabled();
        if(interactionAllowed&&incEditMode()==EditMode::ModelEdit&&ngModelEditSubMode()==ModelEditSubMode::Layout)
            view::ngViewportTransformHandle();
        lastSize=size;ImGui::EndChild();
    }
    compat::ngRequireObject(drawList);
    compat::ngRequireObject(window);
    drawList->AddLine({window->InnerRect.Max.x-1,window->InnerRect.Max.y+size.y},
        {window->InnerRect.Min.x+1,window->InnerRect.Max.y+size.y},
        ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Separator]),2);
    if(interactionAllowed&&ImGui::BeginDragDropTarget()) {
        if(const auto* payload=ImGui::AcceptDragDropPayload("__PARTS_DROP")) {
            const auto descriptor=*compat::ngRequireObject(
                static_cast<const core::WindowFileDropPayload*>(payload->Data));
            const auto files=core::ngRetainFileDropPayload(descriptor);
            for(const auto file:files) {
                const auto nameView=platform::ngAutosaveBaseName(file.view());
                const auto nameOffset=nameView.empty()?0:static_cast<std::size_t>(nameView.data()-file.data());
                const auto name=file.slice(nameOffset,nameOffset+nameView.size());
                const auto extensionStart=platform::ngAutosaveStripExtension(name.view()).size();
                const auto extension=io::imageexport::detail::lowerExtension(name.slice(extensionStart,name.size()));
                if(extension.view()==".png"||extension.view()==".tga"||extension.view()==".jpeg"||extension.view()==".jpg")
                    io::incCreatePartsFromFiles(compat::Slice<io::IoString>{io::IoString(file)});
                else if(extension.view()==".psd") {io::psd::incAskImportPSD(io::IoString(file));break;}
                else if(extension.view()==".kra") {io::kra::incAskImportKRA(io::IoString(file));break;}
                else if(extension.view()==".inx") {incOpenProject(file);break;}
                else {
                    const auto title=core::translation::ngTranslateCString("Error");
                    dialog::ngDialog(dialog::DialogTag::retained(title.data(),title.owner),
                        dialog::DialogText::owned(format(ngTranslateRetained(
                            compat::RetainedString::fromStaticLiteral("%s is not supported")).view(),name)));
                }
            }
            core::ngFinishFileDrag();
        }
        ImGui::EndDragDropTarget();
    }
    size=ImGui::GetContentRegionAvail();
    if(ImGui::BeginChild("##ViewportControls",{0,size.y},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar)) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+4);ImGui::PushItemWidth(72);ImGui::Spacing();ImGui::SameLine(0,8);
        const auto zoomText=format(std::string_view("%s%%\0",5),integer(view::viewportZoom*100));
        if(ImGui::SliderFloat("##Zoom",&view::viewportZoom,static_cast<float>(view::viewportZoomMin),
            static_cast<float>(view::viewportZoomMax),zoomText.c_str(),ImGuiSliderFlags_NoRoundToFormat)) {
            compat::ngRequireObject(camera)->scale={view::viewportZoom,view::viewportZoom};view::viewportTargetZoom=view::viewportZoom;
        }
        if(view::viewportTargetZoom!=1) {ImGui::SameLine(0,8);if(ngButtonColored("",{32,0},colorUndefined)) run(control::ViewportCommand::ResetViewportZoom);}
        ImGui::SameLine(0,8);ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);ImGui::SameLine(0,8);
        ngText(retainedText(format(ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("x = %.2f y = %.2f")).view(),
            view::viewportTargetPosition.x,view::viewportTargetPosition.y)));
        if(view::viewportTargetPosition.x!=0||view::viewportTargetPosition.y!=0) {
            ImGui::SameLine(0,8);if(ngButtonColored("##2",{32,0},colorUndefined)) run(control::ViewportCommand::ResetViewportPosition);
        }
        ImGui::SameLine(0,8);if(ngButtonColored("\ue3b4",{32,0},colorUndefined)) run(control::ViewportCommand::FitViewportToModel);
        ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Fit viewport to model"))));ImGui::PopItemWidth();
    }
    ImGui::EndChild();size=ImGui::GetContentRegionAvail();ImGui::SameLine();
    ImGui::Dummy({size.x-size.x-32*10,0});ImGui::SameLine();
    if(ImGui::BeginChild("##ModelControl",{0,size.y},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar)) {
        const auto button=[](const char* icon,control::ViewportCommand command,const char* tip,bool enabled=true,bool puppet=true) {
            if(ngButtonColored(icon,{32,0},enabled?colorUndefined:ImVec4(0.6f,0.6f,0.6f,1))) run(command,puppet);
            // All private callers below supply immutable static tooltip literals.
            ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(
                compat::RetainedString::fromStaticView(tip))));
        };
        button("\ue8d4",control::ViewportCommand::ToggleMirrorView,"Mirror View",view::shouldMirrorViewport);
        ImGui::SameLine(0,0);
        button("\ue71c",control::ViewportCommand::ToggleOnionSlice,"Onion slice",
            compat::ngRequireObject(nijigenerate::viewport::model::onionslice::OnionSlice::singleton())->enabled(),false);
        ImGui::SameLine();ImGui::BeginDisabled(!interactionAllowed);
        button("",control::ViewportCommand::TogglePhysics,"Enable physics",compat::ngRequireObject(incActivePuppet())->enableDrivers);
        ImGui::EndDisabled();ImGui::SameLine(0,0);
        button("",control::ViewportCommand::TogglePostProcess,"Enable post processing",view::shouldPostProcess);
        ImGui::SameLine();ImGui::BeginDisabled(!interactionAllowed);
        button("",control::ViewportCommand::ResetPhysics,"Reset physics");ImGui::SameLine(0,0);
        button("",control::ViewportCommand::ResetParameters,"Reset parameters");ImGui::SameLine();
        button("",control::ViewportCommand::OpenFlipPairWindow,"Configure Flip Pairings");ImGui::SameLine(0,0);
        button("",control::ViewportCommand::OpenAutomeshBatching,"Automesh Batching");ImGui::EndDisabled();
    }
    ImGui::EndChild();
    view::viewportZoom=static_cast<float>((view::viewportZoom-view::viewportTargetZoom)*std::pow(0.001,nicxlive::core::deltaTime)+view::viewportTargetZoom);
    compat::ngRequireObject(camera)->scale={view::viewportZoom,view::viewportZoom};
    const float factor=static_cast<float>(std::pow(0.001,nicxlive::core::deltaTime*1.5));
    compat::ngRequireObject(camera)->position={(compat::ngRequireObject(camera)->position.x-view::viewportTargetPosition.x)*factor+view::viewportTargetPosition.x,
        (compat::ngRequireObject(camera)->position.y-view::viewportTargetPosition.y)*factor+view::viewportTargetPosition.y};
}
}
