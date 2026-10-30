#include "detail/native_stage.hpp"
#include "native_vertex_bounds.hpp"
#include "geometry.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/compat/throwable.hpp>
namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
template<class Container> core::settings::SettingsString cellDigest(const Container& values) {
    if constexpr(std::is_same_v<Container,nicxlive::core::math::Vec2Array>) {
        const auto cells=values.toArray();
        return bytesDigest(rawBytes<nicxlive::core::math::Vec2>({cells.data(),cells.size()}));
    } else {
        using T=std::remove_cvref_t<decltype(values[0])>;
        return bytesDigest(rawBytes<T>({values.data(),values.size()}));
    }
}
compat::RetainedString finishedDigest(contracts::detail::Sha256& hash) {
    const auto digest=hash.finish();auto result=allocateNativeText(digest.size()*2,'\xff');
    constexpr char hex[]="0123456789ABCDEF";std::size_t offset=0;
    for(auto b:digest){result[offset++]=hex[b>>4];result[offset++]=hex[b&15];}
    return retainNativeText(std::move(result));
}
}
Json reviewModel(Task task) {
    core::settings::SettingsJsonArray items;
    task->runOnMainThread([&] {
        auto puppet=project::incActivePuppet();
        const auto visit = [&](auto&& self, NodeRef node) -> void {
            auto local=node->localTransform;
            auto item=objectEvidence({{stringLiteralEvidence("id"),core::settings::SettingsString(nativeText({"node:",nativeNumber(node->getUUID()).view()}))},{stringLiteralEvidence("name"),node->getName()},{stringLiteralEvidence("type"),node->typeId()},
                {stringLiteralEvidence("parent"),node->parentPtr()?compat::ngRequireObject(node->parentPtr())->getUUID():0u},
                {stringLiteralEvidence("parent_name"),node->parentPtr()?compat::ngRequireObject(node->parentPtr())->getName():nicxlive::core::common::NullableString::fromStaticLiteral("")},{stringLiteralEvidence("enabled"),node->getEnabled()},{stringLiteralEvidence("zsort"),node->relZSort()},
                {stringLiteralEvidence("translation"),array({local.translation.x,local.translation.y,local.translation.z})},
                {stringLiteralEvidence("rotation"),array({local.rotation.x,local.rotation.y,local.rotation.z})},{stringLiteralEvidence("scale"),array({local.scale.x,local.scale.y})}});
            if(auto bone=std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(node)) {
                assignNative(item,"bone",core::settings::SettingsString(bone->boneId));assignNative(item,"rest_head",array({bone->restHead.x,bone->restHead.y,bone->restHead.z}));
                assignNative(item,"rest_tail",array({bone->restTail.x,bone->restTail.y,bone->restTail.z}));assignNative(item,"rest_roll",bone->restRoll);
                assignNative(item,"inherit_parent",bone->allowParentToTargets);
            }
            if(auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) {
                auto mesh=part->getMesh();assignNative(item,"vertices",mesh.vertices.size());assignNative(item,"vertex_bounds",reviewVertexBounds(mesh.vertices));
                assignNative(item,"triangles",mesh.indices.size()/3);assignNative(item,"vertices_sha256",cellDigest(mesh.vertices));assignNative(item,"uv_sha256",cellDigest(mesh.uvs));
                assignNative(item,"indices_sha256",cellDigest(mesh.indices));assignNative(item,"opacity",part->opacity);
                core::settings::SettingsJsonArray masks,welds;
                const auto sourceMasks=part->masks;
                for(auto mask:sourceMasks) {auto mode=mask.mode==nicxlive::core::nodes::MaskingMode::Mask?compat::RetainedString::fromStaticLiteral("Mask"):
                    mask.mode==nicxlive::core::nodes::MaskingMode::DodgeMask?compat::RetainedString::fromStaticLiteral("DodgeMask"):nativeNumber(static_cast<int>(mask.mode));
                    masks.append(objectEvidence({{stringLiteralEvidence("source"),mask.maskSrcUUID},{stringLiteralEvidence("mode"),core::settings::SettingsString(mode)}}));}
                const auto sourceWelds=part->weldedLinks;
                for(auto weld:sourceWelds) {
                    auto target=weld.target.lock();core::settings::SettingsJsonArray indices;for(auto i:weld.indices)indices.append(i);
                    welds.append(objectEvidence({{stringLiteralEvidence("target"),target?target->getUUID():0u},{stringLiteralEvidence("target_name"),target?target->getName():nicxlive::core::common::NullableString::fromStaticLiteral("")},{stringLiteralEvidence("paired_vertices"),weld.indices.size()},
                        {stringLiteralEvidence("weight"),weld.weight},{stringLiteralEvidence("indices_sha256"),core::settings::SettingsString(ngRigDigest(indices))}}));
                }
                assignNative(item,"masks",masks);assignNative(item,"welding",welds);
            }else if(auto deformable=std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) {
                assignNative(item,"vertices",deformable->getVertices().size());
                assignNative(item,"vertex_bounds",reviewVertexBounds(deformable->getVertices()));
                assignNative(item,"vertices_sha256",cellDigest(deformable->getVertices().toArray()));
            }
            if(auto mapped=std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(node))assignNative(item,"depth_sha256",cellDigest(mapped->copyDepths()));
            items.append(item);const auto children=node->childrenList();for(auto child:children)self(self,child);
        };
        visit(visit,puppet->root);
        const auto parameters=puppet->parameters;
        for(auto parameter:parameters) {
            core::settings::SettingsJsonArray bindings;
            const auto parameterBindings=parameter->bindings;
            for(auto binding:parameterBindings) {
                compat::RetainedString digest;std::size_t keyCount=0;double maximumOffset=0;
                for(std::size_t y=0,yCount=parameter->axisPoints[1].size();y<yCount;++y)
                    for(std::size_t x=0,xCount=parameter->axisPoints[0].size();x<xCount;++x)
                    if(binding->isSet({static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y)}))++keyCount;
                auto deformation=std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(binding);
                if(deformation) {
                    contracts::detail::Sha256 hash;
                    for(std::size_t y=0,yCount=parameter->axisPoints[1].size();y<yCount;++y)
                        for(std::size_t x=0,xCount=parameter->axisPoints[0].size();x<xCount;++x) {
                        nicxlive::core::param::Vec2u index{static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y)};if(!binding->isSet(index))continue;
                        std::array<std::uint32_t,2> coordinates{static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y)};
                        hash.put(rawBytes<std::uint32_t>(coordinates));auto offsets=deformation->getValue(index).vertexOffsets;
                        for(std::size_t i=0;i<offsets.size();++i){auto point=offsets.at(i);maximumOffset=dmax(maximumOffset,dmax(std::abs(point.x),std::abs(point.y)));}
                        auto cells=offsets.toArray();hash.put(rawBytes<nicxlive::core::math::Vec2>({cells.data(),cells.size()}));
                    }
                    digest=finishedDigest(hash);
                }else digest=ngRigDigest(serializedJson(binding)).retainedString();
                const auto targetId=compat::ngRequireObject(binding->getTarget().getResource())->getUUID();
                const auto targetName=compat::ngRequireObject(binding->getTarget().getResource())->getName();
                auto entry=objectEvidence({{stringLiteralEvidence("target"),targetId},{stringLiteralEvidence("target_name"),targetName},
                    {stringLiteralEvidence("property"),binding->getNameDescriptor()},{stringLiteralEvidence("authored_keys"),keyCount},
                    {stringLiteralEvidence("content_sha256"),core::settings::SettingsString(digest)}});
                if(deformation)assignNative(entry,"maximum_offset",maximumOffset);bindings.append(entry);
            }
            items.append(objectEvidence({{stringLiteralEvidence("id"),core::settings::SettingsString(nativeText({"parameter:",nativeNumber(parameter->getUUID()).view()}))},{stringLiteralEvidence("name"),parameter->getName()},{stringLiteralEvidence("bindings"),bindings},
                {stringLiteralEvidence("minimum"),array({parameter->min.x,parameter->min.y})},{stringLiteralEvidence("maximum"),array({parameter->max.x,parameter->max.y})},
                {stringLiteralEvidence("default"),array({parameter->defaults.x,parameter->defaults.y})},
                {stringLiteralEvidence("axisX"),[&] {
                    const auto points=parameter->axisPoints[0];
                    core::settings::SettingsJsonArray values;
                    for(auto point:points)values.append(point);
                    return Json(values);
                }()},
                {stringLiteralEvidence("axisY"),[&] {
                    const auto points=parameter->axisPoints[1];
                    core::settings::SettingsJsonArray values;
                    for(auto point:points)values.append(point);
                    return Json(values);
                }()}}));
        }
    });
    return items;
}
Json verifySourceFrames(Json state,Task task) {
    core::settings::SettingsJsonArray snapshots,checks;
    task->runOnMainThread([&] {auto puppet=project::incActivePuppet();for(auto material:indexNative(state,"materials").sourceArrayValue()) {
        auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(material,"uuid"))));
        enforceNative(bool(part),"Source frame verification Part is missing",282);snapshots.append(rootTextureMapping(part));
    }});
    auto materials=indexNative(state,"materials").sourceArrayValue();
    for(std::size_t i=0;i<materials.size();++i) {
        auto material=materials[i];ngRigCheckpoint(task);
        try {checks.append(objectEvidence({{stringLiteralEvidence("part"),indexNative(material,"uuid")},{stringLiteralEvidence("maximum_root_error"),[&] {
            const auto sourceMapping=indexNative(material,"source_root_mapping");
            const auto actualMapping=snapshots[i];
            return geometry::ngRigVerifySourceUV(sourceMapping,actualMapping);
        }()}}));}
        catch(const compat::ThrowableException& error) {
            auto diagnostic=objectEvidence({{stringLiteralEvidence("part"),indexNative(material,"uuid")},{stringLiteralEvidence("path"),indexNative(material,"path")},
                {stringLiteralEvidence("source"),indexNative(material,"source_root_mapping")},{stringLiteralEvidence("actual"),snapshots[i]},
                {stringLiteralEvidence("error"),core::settings::SettingsString(error.msg)}});
            task->previewJson("source-frame-mismatch",diagnostic);
            const auto path=nativeString(indexNative(material,"path"));
            throw compat::ThrowableException(nativeText({path.view(),": ",error.msg.view()}),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),295);
        }
    }
    return checks;
}
}
