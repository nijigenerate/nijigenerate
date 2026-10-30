#include "detail/native_stage.hpp"
#include "native.hpp"
#include "observation.hpp"
#include "evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/viewport/vertex/automesh/common.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
using namespace observation;
Json observeModel(Json options,Task task) {
    std::uint32_t rootId=0;framework::Text signature;compat::Slice<std::uint32_t> ids;core::settings::SettingsJsonArray groups;
    NativeIntegerMap<std::uint32_t,std::size_t> sourceOrders;
    task->runOnMainThread([&] {
        auto puppet=project::incActivePuppet();enforceNative(bool(puppet),"No imported model is open",434);rootId=puppet->root->getUUID();signature=editorSignature(puppet);
        NativeIntegerMap<std::uint32_t, bool> clippingSurfaces;
        const auto collectClipping = [&](auto&& self, NodeRef node) -> void {
            if(auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) {
                const auto masks=part->masks;
                for(auto mask:masks)if(mask.mode==nicxlive::core::nodes::MaskingMode::Mask) {
                    clippingSurfaces[node->getUUID()]=true;clippingSurfaces[mask.maskSrcUUID]=true;
                }
            }
            for(auto childRange=node->childrenList();auto child:childRange)self(self,child);
        };
        collectClipping(collectClipping,puppet->root);
        for(auto parameter:puppet->parameters)enforceNative(parameter->value.x==parameter->defaults.x&&parameter->value.y==parameter->defaults.y,"Model must be at its default pose before AutoRig",447);
        const auto visit = [&](auto&& self, NodeRef node) -> void {
            const auto key=node->getUUID();const auto order=sourceOrders.size();sourceOrders[key]=order;
            enforceNative(!std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node),"Model already contains a depth rig",450);
            if(node->typeId()=="Part")ids.append(node->getUUID());
            else if(node!=puppet->root&&!node->children.empty())groups.append(objectEvidence({{stringLiteralEvidence("uuid"),node->getUUID()},{stringLiteralEvidence("name"),node->getName()},{stringLiteralEvidence("parent"),node->parentPtr()->getUUID()},
                {stringLiteralEvidence("source_order"),readNativeMap(sourceOrders,node->getUUID(),454)},{stringLiteralEvidence("clipping_surface"),clippingSurfaces.contains(node->getUUID())}}));
            for(auto childRange=node->childrenList();auto child:childRange)self(self,child);
        };
        visit(visit,puppet->root);
    });
    enforceNative(!ids.empty(),"Imported model has no Parts",460);core::settings::SettingsJsonArray materials;
    for(auto id:ids) {
        ngRigCheckpoint(task);Json record,rootMesh;compat::Slice<std::uint8_t> pixels;std::size_t width=0,height=0;
        task->runOnMainThread([&] {
            auto puppet=project::incActivePuppet();enforceNative(puppet&&puppet->root->getUUID()==rootId,"Active model changed during observation",469);
            auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(id));enforceNative(bool(part),"Part disappeared during observation",471);
            auto alpha=viewport::vertex::automesh::common::getAlphaInput(part);enforceNative(alpha.img&&alpha.w>0&&alpha.h>0,"Part has no readable alpha texture",473);
            width=static_cast<std::size_t>(alpha.w);height=static_cast<std::size_t>(alpha.h);pixels=alpha.img->data().duplicate();
            auto mapping=textureMapping(part);compat::Slice<Point2> world;for(auto p:ngRigPoints(indexNative(mapping,"vertices")))world.append(toRoot(part,p[0],p[1]));
            rootMesh=objectEvidence({{stringLiteralEvidence("vertices"),ngRigPointsJson(world)},{stringLiteralEvidence("uv"),indexNative(mapping,"uv")},{stringLiteralEvidence("triangles"),indexNative(mapping,"triangles")}});
            const double inf=std::numeric_limits<double>::infinity();std::array<double,4> bounds{inf,inf,-inf,-inf};
            for(auto p:world)for(std::size_t i=0;i<2;++i){bounds[i]=dmin(bounds[i],p[i]);bounds[i+2]=dmax(bounds[i+2],p[i]);}
            framework::Text path;NativeVector<core::settings::SettingsString> ancestors;NativeVector<std::uint32_t> ancestorIds;
            for(NodeRef node=part;node!=puppet->root;node=node->parentPtr()) {
                const auto name=node->getName();
                path=nativeText({"/",name.view(),path.view()});
                if(node!=part) {
                    ancestors.insert(ancestors.begin(),core::settings::SettingsString(node->getName()));
                    ancestorIds.insert(ancestorIds.begin(),node->getUUID());
                }
            }
            if(auto imported=std::dynamic_pointer_cast<ext::nodes::ExPart>(part);imported&&!imported->layerPath.empty())
                path=imported->layerPath;
            bool active=native::ngRigMaterialIsActive(part);
            record=objectEvidence({{stringLiteralEvidence("name"),part->getName()},{stringLiteralEvidence("path"),core::settings::SettingsString(path)},
                {stringLiteralEvidence("uuid"),id},{stringLiteralEvidence("source_order"),readNativeMap(sourceOrders,id,498)},{stringLiteralEvidence("active"),active},
                {stringLiteralEvidence("bounds"),array({bounds[0],bounds[1],bounds[2],bounds[3]})},{stringLiteralEvidence("source_mapping"),mapping},{stringLiteralEvidence("texture_size"),array({width,height})},
                {stringLiteralEvidence("texture_sha256"),bytesDigest({pixels.data(),pixels.size()})}});
            core::settings::SettingsJsonArray ancestorsJson,idsJson;for(auto s:ancestors)ancestorsJson.append(s);for(auto v:ancestorIds)idsJson.append(v);
            assignNative(record,"ancestors",ancestorsJson);assignNative(record,"ancestor_ids",idsJson);assignNative(record,"parent",part->parentPtr()->getUUID());
            const auto rootInverse=core::math::source_matrix::inverse(puppet->root->transform().matrix());
            const auto parentMatrix=part->parentPtr()->transform().matrix();
            const auto matrix=rootInverse*parentMatrix;
            auto multiply=[&](std::array<float,4> input){std::array<float,4> out{};
                for(int column=0;column<4;++column)for(int row=0;row<4;++row)out[row]=nicxlive::core::math::detail::inmathAccumulate(out[row],nicxlive::core::math::detail::inmathProduct(matrix[row][column],input[column]));return out;};
            auto px=multiply({1,0,0,0}),py=multiply({0,1,0,0}),po=multiply({0,0,0,1});
            assignNative(record,"parent_to_root",array({px[0],py[0],po[0],px[1],py[1],po[1]}));bool found=false;
            for(NodeRef cursor=part;cursor&&!found;cursor=cursor->parentPtr())if(auto surface=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(cursor))
                for(auto mask:surface->masks)if(mask.mode==nicxlive::core::nodes::MaskingMode::Mask){assignNative(record,"receiver",mask.maskSrcUUID);found=true;break;}
        });
        auto cloud=ngRigTextureSupport(pixels,width,height,rootMesh,40000,task);
        assignNative(record,"source_root_mapping",rootMesh);assignNative(record,"alpha_runs_32",ngRigAlphaRuns(pixels,32));
        assignNative(record,"alpha_runs_128",ngRigAlphaRuns(pixels,128));auto strongAlpha=pixels.duplicate();
        for(std::size_t pixel=0;pixel<strongAlpha.size()/4;++pixel)if(strongAlpha[pixel*4+3]<=128)strongAlpha[pixel*4+3]=0;
        assignNative(record,"draw_order_cloud",ngRigPointsJson(ngRigTextureSupport(strongAlpha,width,height,rootMesh,10000,task)));
        if(evidence::ngRigMaterialFeature(indexNative(record,"name").as_string()).view()=="sclera") {
            auto mainPatch=ngRigLargestAlphaComponent(pixels,width,height,task);assignNative(record,"landmark_cloud",ngRigPointsJson(ngRigTextureSupport(mainPatch,width,height,rootMesh,40000,task)));
        }
        assignNative(record,"opaque_perimeter_coverage",ngRigAlphaPerimeterCoverage(pixels,width,height));
        assignNative(record,"cloud",ngRigPointsJson(cloud));assignNative(record,"active",indexNative(record,"active").as_bool()&&!cloud.empty());materials.append(record);
    }
    evidence::ngRigPropagateClippingVisibility(materials);evidence::ngRigResolveGroupReceivers(materials,groups);evidence::ngRigPropagateClippingVisibility(materials);
    auto result=objectEvidence({{stringLiteralEvidence("schema_version"),"rig-model-observation-d/1"},{stringLiteralEvidence("rootId"),rootId},{stringLiteralEvidence("materials"),materials},{stringLiteralEvidence("groups"),groups},{stringLiteralEvidence("options"),options}});
    task->runOnMainThread([&]{auto puppet=project::incActivePuppet();enforceNative(puppet&&puppet->root->getUUID()==rootId&&editorSignature(puppet)==signature,"Imported model changed while CPU snapshots were being acquired",537);});
    assignNative(result,"source_sha256",core::settings::SettingsString(ngRigDigest(result)));
    if(get(options,"render",false).as_bool())assignNative(result,"neutral_render",
        renderFrame(task,compat::RetainedString::fromStaticLiteral("source-neutral")));
    return result;
}
}
