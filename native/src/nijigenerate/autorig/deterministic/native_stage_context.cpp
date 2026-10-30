#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "native_signature.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/commands/depth/detail/bone_flush.hpp>
#include <nijigenerate/commands/depth/detail/bone_pivot_cache.hpp>
#include <nijigenerate/commands/depth/bone_effective_pivots.hpp>
#include <serde_json.hpp>
#include <chrono>
namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
Json reviewSettings(Task task) {return task->hasInput("review")?task->input("review").json:Json{};}
std::shared_ptr<commands::Context> editorContext(compat::Slice<NodeRef> nodes) {
    enforceNative(core::thread_context::ngIsMainThreadOrUnattached(),"Rig editor access requires the main thread",223);
    auto context=createNativeObject<commands::Context>();context->puppet(project::incActivePuppet());
    if(!nodes.isNull())context->nodes(nodes);return context;
}
ParameterRef parameterByName(PuppetRef puppet,framework::Text name) {
    const auto parameters=puppet->parameters;
    for(auto p:parameters)if(p->getName().view()==name.view())return p;
    throw compat::ThrowableException(nativeText({"Rig parameter not found: ",name.view()}),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),231);
}
Json textureMapping(std::shared_ptr<nicxlive::core::nodes::Part> part) {
    enforceNative(core::thread_context::ngIsMainThreadOrUnattached(),"Rig mesh snapshot requires the main thread",253);
    auto mesh=part->getMesh();core::settings::SettingsJsonArray coordinates,texture,triangles;
    for(auto v:mesh.vertices)coordinates.append(array({v.x,v.y}));
    for(auto uv:mesh.uvs)texture.append(array({uv.x,uv.y}));
    enforceNative(mesh.indices.size()%3==0,"Invalid native triangle index count",258);
    for(std::size_t i=0;i<mesh.indices.size()/3;++i)triangles.append(array({mesh.indices[i*3],mesh.indices[i*3+1],mesh.indices[i*3+2]}));
    return objectEvidence({{stringLiteralEvidence("vertices"),coordinates},{stringLiteralEvidence("uv"),texture},{stringLiteralEvidence("triangles"),triangles}});
}
compat::RetainedString editorSignature(PuppetRef puppet) {
    enforceNative(core::thread_context::ngIsMainThreadOrUnattached(),"Rig model snapshot requires the main thread",395);
    auto snapshot=serializedJson(puppet);authoredState(indexNative(snapshot,"nodes"));
    return contracts::ngRigDigest(snapshot).retainedString();
}
void settleDepthRefresh(Task task) {
    const auto start=std::chrono::steady_clock::now();
    for(;;) {
        bool pending=false;
        task->runOnMainThread([&]{commands::depth::bone::ngFlushDepthBoneEffectivePivotDirty();commands::depth::bone::ngFlushDepthBoneDirty();
            pending=commands::depth::bone::ngHasPendingDepthBoneEffectivePivotRefresh()||commands::depth::bone::ngHasPendingDepthBoneRefresh();});
        if(!pending)return;
        enforceNative(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count()<300,"Native depth refresh did not settle before checkpoint",420);
        contracts::ngRigCheckpoint(task);
    }
}
}
