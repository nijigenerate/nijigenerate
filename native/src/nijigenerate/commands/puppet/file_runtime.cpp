#include "file_runtime.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <runtime_state.hpp>
#include <cmath>
#include <limits>
namespace nijigenerate::commands::puppet::file::detail {
namespace {
struct LoadedPuppetSliceArgument {
    const std::shared_ptr<nicxlive::core::Puppet>& puppet;
    operator compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>() const {
        return puppet ? compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>{puppet}
            : compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>{};
    }
};
std::size_t extensionPosition(std::string_view path) {
    const auto separator=[](char c) { return c=='/' || c=='\\' || c==':'; };
    for(auto i=path.size();i>0;) {
        --i;
        if(separator(path[i])) break;
        if(path[i]=='.' && i>0 && !separator(path[i-1])) return i;
    }
    return std::string_view::npos;
}
}
String extension(String path) {
    const auto position=extensionPosition(path.view());
    return position==std::string_view::npos?String{}:path.subslice(position,path.size());
}
String withExtension(String path,std::string_view ext) {
    const auto position=extensionPosition(path.view());
    auto stem=position==std::string_view::npos?path.view():path.view().substr(0,position);
    if(ext.empty()) return position==std::string_view::npos?path:path.subslice(0,position);
    return fileAllocation([&] {
        std::string result(stem);
        if(ext.front()!='.') result+='.';
        result+=ext;
        return String(std::move(result));
    });
}
compat::Slice<core::settings::SettingsString> splitPaths(String value) {
    compat::Slice<core::settings::SettingsString> result;
    if(value.empty()) return result;
    std::size_t start=0;
    for(;;) {
        auto end=value.view().find('|',start);
        if(end==std::string_view::npos) { result.append(value.subslice(start,value.size())); break; }
        result.append(value.subslice(start,end)); start=end+1;
    }
    return result;
}
void showError(String message) {
    using namespace widgets::dialog;
    const auto title=core::translation::ngTranslateCString("Error");
    const auto tag=title.owner.use_count() ? DialogTag::retained(title.data(),title.owner)
        : DialogTag::borrowed(title.data());
    ngDialog(tag,DialogText(message));
}
int asInt(double value) {
    if(!std::isfinite(value)||value<-2147483648.0||value>=2147483648.0)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
std::shared_ptr<LoadResult<std::shared_ptr<nicxlive::core::Puppet>>> ngLoadedActivePuppet(String message) {
    auto puppet=project::incActivePuppet();
    using P=std::shared_ptr<nicxlive::core::Puppet>;
    return std::allocate_shared<LoadResult<P>>(nicxlive::core::d_runtime::SourceAllocator<LoadResult<P>>{},
        true,LoadedPuppetSliceArgument{puppet},message);
}
std::shared_ptr<ext::nodes::ExCamera> ngResolveExportCamera(String name,String& error) {
    error={};
    auto puppet=project::incActivePuppet();
    if(!puppet) { error="No active puppet"; return {}; }
    const auto found=puppet->findNodesType<ext::nodes::ExCamera>(puppet->root);
    if(found.empty()) { error="No cameras available in the active puppet"; return {}; }
    if(name.empty()) return found[0];
    for(const auto camera:found)
        if(compat::ngRequireObject(camera)->getName()==name.view()) return camera;
    error=format("Camera not found: %s",name); return {};
}
bool ngRenderCamera(std::shared_ptr<ext::nodes::ExCamera> selectedCamera,bool transparency,bool postprocessing,
    nicxlive::core::common::SharedArray<std::uint8_t>& data,int& width,int& height) {
    using namespace nicxlive::core;
    data={};width=0;height=0;
    auto camera=selectedCamera->getCamera(); auto viewport=selectedCamera->getViewport();
    int oldWidth=0,oldHeight=0; inGetViewport(oldWidth,oldHeight);
    auto oldCamera=inGetCameraHandle();
    float r=std::numeric_limits<float>::quiet_NaN(),g=r,b=r,a=r;
    width=asInt(viewport.x);height=asInt(viewport.y);
    inSetCamera(camera);inSetViewport(width,height);
    if(transparency) { inGetClearColor(r,g,b,a); inSetClearColor(0,0,0,0); }
    compat::ngRunWithScopeExit([&] {
        inBeginScene();project::incActivePuppet()->update();project::incActivePuppet()->draw();inEndScene();
        if(postprocessing)inPostProcessScene();
        const auto dataLength = inViewportDataLength();
        data = fileAllocation([&] {
            return nicxlive::core::common::SharedArray<std::uint8_t>(dataLength, 0);
        });
        inDumpViewport(data);
    },[&] {
        if(transparency)inSetClearColor(r,g,b,a);
        inSetViewport(oldWidth,oldHeight);inSetCamera(oldCamera);
    });
    return true;
}
void ngMergeImportedPuppet(std::shared_ptr<ext::ExPuppet> source,bool mergeParameters,bool mergeAnimations) {
    auto target=std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    if(!target||!source)return;
    const auto children=source->root->children.duplicate();
    for(const auto& child:children) child->reparent(target->root,nicxlive::core::nodes::Node::OFFSET_END,true);
    if(mergeParameters) for(const auto& parameter:source->parameters) target->parameters.push_back(parameter);
    if(mergeAnimations) for(const auto& [name,animation]:source->getAnimations())
        target->getAnimations()[fileAllocation([unique = ngUniqueAnimationName(target,String(name))] {
            return std::string(unique.view());
        })]=animation;
    target->rescanNodes();target->populateTextureSlots();target->root->transformChanged();
    project::incInitAnimationPlayer(target);project::incFocusCamera(target->root);
}
}
