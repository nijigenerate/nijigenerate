#include "detail/native_stage.hpp"
#include "native_grid_scales.hpp"
#include "registered.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/depth/bone_commands.hpp>
#include <nijigenerate/commands/depth/map_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nodes/composite.hpp>
#include <nodes/grid_deformer.hpp>
namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
using namespace commands::node::node;
using namespace commands::depth::bone;
using namespace commands::inspector::apply_node;
using commands::depth::map::SetDepthsCommand;
using Matrix=nicxlive::core::math::Mat4;
namespace {
std::array<float,4> mul(const Matrix& m,std::array<float,4> p) {
    std::array<float,4> out{};
    for(std::size_t c=0;c<4;++c)for(std::size_t r=0;r<4;++r)
        out[r]=nicxlive::core::math::detail::inmathAccumulate(out[r],
            nicxlive::core::math::detail::inmathProduct(m[r][c],p[c]));
    return out;
}
std::shared_ptr<commands::Context> inspectorContext(NodeRef node) {
    auto context=editorContext({node});
    context->inspectors({panels::inspector::ngCreateInspector<NodeApplyInspector>(
        panels::inspector::Selection<nicxlive::core::nodes::Node>{node},panels::inspector::ModelEditSubMode::Layout)});
    return context;
}
template<class C,class Value> void property(NodeRef node,Value makeValue) {
    auto context=inspectorContext(node);
    auto action=createNativeObject<C>();action->value=makeValue();commandSource(
        [&] {
            return action;
        },
        [&] {
            return context;
        });
}
void setFrame(NodeRef node,Matrix matrix) {
    auto local=core::math::source_matrix::inverse(node->parentPtr()->transform().matrix())*matrix;
    auto x=mul(local,{1,0,0,0}),y=mul(local,{0,1,0,0}),p=mul(local,{0,0,0,1});
    float sx=std::sqrt(x[0]*x[0]+x[1]*x[1]),sy=std::sqrt(y[0]*y[0]+y[1]*y[1]);
    enforceNative(sx>0&&sy>0&&std::abs(x[0]*y[0]+x[1]*y[1])<=1e-4*sx*sy,"Material hierarchy requires an unsupported shear",952);
    if(x[0]*y[1]-x[1]*y[0]<0)sy=-sy;
    auto context=inspectorContext(node);
    auto scaleX=createNativeObject<ScaleXCommand>();scaleX->value=sx;commandSource(
        [&] {
            return scaleX;
        },
        [&] {
            return context;
        });
    auto scaleY=createNativeObject<ScaleYCommand>();scaleY->value=sy;commandSource(
        [&] {
            return scaleY;
        },
        [&] {
            return context;
        });
    auto rotation=createNativeObject<RotationZCommand>();rotation->value=static_cast<float>(
        std::atan2(static_cast<double>(x[1]),static_cast<double>(x[0])));commandSource(
            [&] {
                return rotation;
            },
            [&] {
                return context;
            });
    auto tx=createNativeObject<TranslationXCommand>();tx->value=p[0];commandSource(
        [&] {
            return tx;
        },
        [&] {
            return context;
        });
    auto ty=createNativeObject<TranslationYCommand>();ty->value=p[1];commandSource(
        [&] {
            return ty;
        },
        [&] {
            return context;
        });
    auto tz=createNativeObject<TranslationZCommand>();tz->value=p[2];commandSource(
        [&] {
            return tz;
        },
        [&] {
            return context;
        });
    node->transformChanged();
}
void movePreservingFrame(NodeRef unit,NodeRef parent) {
    auto world=unit->transform().matrix();const float absoluteSort=unit->zSortNoOffset();
    commandSource(
        [&] {
            return createNativeObject<MoveNodeCommand>(parent,parent->children.size(),true);
        },
        [&] {
            return editorContext({unit});
        });
    setFrame(unit,world);
    const auto parentSort=parent->zSortNoOffset();
    property<ZSortCommand>(unit,[&] { return absoluteSort-parentSort; });
}
Json scopeOf(NodeRef node) {
    NativeVector<std::uint32_t> ids;
    for(auto p=node->parentPtr();p;p=p->parentPtr())if(p->typeId().find("Composite")!=std::string::npos)ids.insert(ids.begin(),p->getUUID());
    JsonArray out;for(auto id:ids)out.append(id);return out;
}
template<class Values> Json valuesJson(const Values& values) {JsonArray out;for(auto v:values)out.append(v);return out;}
}
Json buildRig(Json state,Json program,Task task) {
    Json result=state;const bool humanoid=stringGet(program,"kind","humanoid")=="humanoid";
    task->runOnMainThread([&] {
        auto puppet=project::incActivePuppet();
        auto root=createSourceNode(
            [&] {
                return createNativeObject<AddNodeCommandT<true>>(compat::RetainedString::fromStaticLiteral("DepthRigRoot"),
                    compat::RetainedString::fromStaticLiteral("::AutoRig"));
            },
            [&] {
                return editorContext({puppet->root});
            });
        root->setName(nicxlive::core::common::NullableString::fromStaticLiteral("AutoRig::DepthRig"));
        if(humanoid) {
            NativeVector<std::uint32_t> hosted;
            auto hierarchy=indexNative(program,"hierarchy");
            for(auto definition:indexNative(hierarchy,"groups").sourceArrayValue())
                if(auto node=indexNative(definition,"parent").sourceObjectRef().if_contains("node"))hosted.push_back(uuid(*node));
            for(auto parents=indexNative(hierarchy,"surface_parents");auto member:parents.sourceObjectRef())
                if(auto node=member.value().sourceObjectRef().if_contains("node"))hosted.push_back(uuid(*node));
            if(!indexNative(hierarchy,"face_origin").is_null())hosted.push_back(uuid(indexNative(hierarchy,"face_origin")));
            JsonArray converted;
            for(auto units=indexNative(hierarchy,"render_units");auto member:units.sourceObjectRef())for(auto unitId:member.value().sourceArrayValue()) {
                auto unit=puppet->findNodeById(uuid(unitId));if(!unit||unit->typeId()!="DynamicComposite")continue;
                bool contains=false;
                for(auto origin:hosted)for(auto cursor=puppet->findNodeById(origin);cursor;cursor=cursor->parentPtr())
                    if(cursor==unit)contains=true;
                if(!contains)continue;
                auto before=serializedJson(unit);
                auto composite=std::dynamic_pointer_cast<nicxlive::core::nodes::Composite>(
                    createSourceNode(
                        [&] {
                            return createNativeObject<ConvertToCommandT<true>>(compat::RetainedString::fromStaticLiteral("Composite"));
                        },
                        [&] {
                            return editorContext({unit});
                        }));
                enforceNative(composite&&composite->getUUID()==uuid(unitId)&&composite->propagateMeshGroup,"Origin composite did not preserve identity and propagate deformation",904);
                auto after=serializedJson(composite);
                for(auto key:{"blend_mode","opacity","masks"})enforceNativeLazy(get(before,key)==get(after,key),[&] { return nativeText({"Origin composite conversion changed ",key}); },908);
                converted.append(unitId);
            }
            assignNative(state,"origin_composites",converted);
        }
        framework::Table<NodeRef> bones,origins,domainGrids;
        framework::Table<double> poseOrigins;
        if(humanoid)for(auto bone:indexNative(indexNative(program,"scaffold"),"bones").sourceArrayValue()) {
            auto add=createNativeObject<AddDepthBoneCommand>();
            add->parent=indexNative(bone,"parent").is_null()?root:readNativeMap(bones,nativeString(indexNative(bone,"parent")),918);
            add->boneId=nativeString(indexNative(bone,"id"));add->restHead=nativeNumbers(indexNative(bone,"head"));
            add->restTail=nativeNumbers(indexNative(bone,"tail"));add->restRoll=static_cast<float>(ngRigNumber(indexNative(bone,"rest_roll")));
            auto created=createSourceNode(
                [&] {
                    return add;
                },
                [&] {
                    return editorContext();
                });bones[nativeString(indexNative(bone,"id"))]=created;
            auto inheritance=createNativeObject<SetDepthBoneConstraintCommand>();inheritance->bone=created;
            inheritance->constraint=compat::ngJsonStringify(objectEvidence({{stringLiteralEvidence("allowParentToTargets"),indexNative(bone,"allow_parent_to_targets")}}));
            commandSource(
                [&] {
                    return inheritance;
                },
                [&] {
                    return editorContext();
                });
            if(indexNative(bone,"lock_to_root").as_bool())property<LockToRootCommand>(created,[] { return true; });
            if(auto pose=bone.sourceObjectRef().if_contains("pose_origin_z")) {
                const auto absolute=ngRigNumber(*pose);
                const auto parentZ=indexNative(bone,"parent").is_null()||indexNative(bone,"lock_to_root").as_bool()?0.:
                    readNativeMap(poseOrigins,nativeString(indexNative(bone,"parent")),933);
                property<TranslationZCommand>(created,[&] { return static_cast<float>(absolute-parentZ); });
                poseOrigins[nativeString(indexNative(bone,"id"))]=absolute;
            }
        }
        if(humanoid) {auto parameters=createNativeObject<AddStandardDepthParametersCommand>(indexNative(program,"native_drivers"));
            parameters->root=root;commandSource(
                [&] {
                    return parameters;
                },
                [&] {
                    return editorContext();
                });}
        if(humanoid)for(auto definition:indexNative(indexNative(program,"hierarchy"),"groups").sourceArrayValue()) {
            auto parent=puppet->findNodeById(uuid(indexNative(indexNative(definition,"parent"),"node")));
            enforceNative(bool(parent),"Material origin parent disappeared",974);
            auto origin=createSourceNode(
                [&] {
                    return createNativeObject<AddNodeCommandT<true>>(compat::RetainedString::fromStaticLiteral("Node"),
                        compat::RetainedString::fromStaticLiteral("::AutoRig"));
                },
                [&] {
                    return editorContext({parent});
                });
            origin->setName(nativeString(indexNative(definition,"id")).toNullableString());auto p=ngRigNumbers(indexNative(definition,"origin"));
            const auto originMatrix=puppet->root->transform().matrix();
            const auto originX=static_cast<float>(readNativeArray(p,0,978));
            const auto originY=static_cast<float>(readNativeArray(p,1,978));
            setFrame(origin,originMatrix*Matrix::translation(originX,originY,0));
            origins[nativeString(indexNative(definition,"id"))]=origin;
        }
        JsonArray targets;
        for(auto carrier:indexNative(program,"carriers").sourceArrayValue()) {
            auto part=puppet->findNodeById(uuid(indexNative(carrier,"part")));enforceNative(bool(part),"Rig part disappeared",984);
            Json target=carrier.sourceObjectRef().duplicate();
            if(humanoid) {
                auto id=nativeString(indexNative(carrier,"domain_id"));NodeRef grid;
                if(auto found=domainGrids.find(id);found!=domainGrids.end())grid=found->second;
                else {
                    auto selector=indexNative(indexNative(indexNative(program,"hierarchy"),"surface_parents"),id.view());
                    auto parent=selector.sourceObjectRef().if_contains("group")?readNativeMap(origins,nativeString(indexNative(selector,"group")),992):
                        puppet->findNodeById(uuid(indexNative(selector,"node")));
                    enforceNative(bool(parent),"Semantic surface parent disappeared",994);
                    grid=createSourceNode(
                        [&] {
                            return createNativeObject<AddNodeCommandT<true>>(compat::RetainedString::fromStaticLiteral("GridDeformer"),
                                compat::RetainedString::fromStaticLiteral("::AutoRig"));
                        },
                        [&] {
                            return editorContext({parent});
                        });
                    grid->setName(nativeText({"AutoRig::",id.view()}).toNullableString());auto frame=ngRigNumbers(indexNative(carrier,"parent_to_root"));
                    const auto gridRootMatrix=puppet->root->transform().matrix();
                    const auto frameX=static_cast<float>(readNativeArray(frame,2,998));
                    const auto frameY=static_cast<float>(readNativeArray(frame,5,998));
                    setFrame(grid,gridRootMatrix*Matrix::translation(frameX,frameY,0));
                    for(auto bone:indexNative(carrier,"bones").sourceArrayValue()) {auto add=createNativeObject<AddDepthBoneSourceCommand>();
                        add->root=root;add->target=grid;add->bone=readNativeMap(bones,nativeString(bone),1000);commandSource(
                            [&] {
                                return add;
                            },
                            [&] {
                                return editorContext();
                            });}
                    if(auto rule=carrier.sourceObjectRef().if_contains("bone_influence_rule")) {
                        auto apply=createNativeObject<SetDepthBoneInfluenceRuleCommand>();apply->root=root;apply->target=grid;
                        apply->rule=compat::ngJsonStringify(*rule);commandSource(
                            [&] {
                                return apply;
                            },
                            [&] {
                                return editorContext();
                            });
                    }
                    domainGrids[id]=grid;
                    for(auto unitId:indexNative(indexNative(indexNative(program,"hierarchy"),"render_units"),id.view()).sourceArrayValue()) {
                        auto unit=puppet->findNodeById(uuid(unitId));enforceNative(bool(unit),"Semantic render unit disappeared",1010);movePreservingFrame(unit,grid);
                    }
                }
                assignNative(target,"grid",grid->getUUID());
            }
            assignNative(target,"mapping",textureMapping(std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(part)));
            compat::Slice<Point2> world;for(auto p:ngRigPoints(indexNative(indexNative(target,"mapping"),"vertices")))world.append(toRoot(part,p[0],p[1]));
            assignNative(target,"world",ngRigPointsJson(world));assignNative(target,"origin",pointJson(toRoot(part,0,0)));
            const auto partInverse=core::math::source_matrix::inverse(part->transform().matrix());
            const auto rootMatrix=puppet->root->transform().matrix();
            const auto direction=partInverse*rootMatrix;
            auto dx=mul(direction,{1,0,0,0}),dy=mul(direction,{0,1,0,0});
            assignNative(target,"root_to_local_direction",array({dx[0],dy[0],dx[1],dy[1]}));
            assignNative(target,"absolute_zsort",static_cast<double>(part->zSortNoOffset()));
            assignNative(target,"relative_zsort",part->relZSort());assignNative(target,"composite_scope",scopeOf(part));targets.append(target);
        }
        if(humanoid&&!indexNative(indexNative(program,"hierarchy"),"face_origin").is_null()) {
            auto origin=puppet->findNodeById(uuid(indexNative(indexNative(program,"hierarchy"),"face_origin")));
            NativeIntegerMap<std::uint32_t, bool> faceParts;NodeRef grid;
            for(auto carrier:indexNative(program,"carriers").sourceArrayValue())if(nativeString(indexNative(carrier,"owner")).view()=="head"&&nativeString(indexNative(carrier,"chart")).view()=="face") {
                faceParts[uuid(indexNative(carrier,"part"))]=true;grid=readNativeMap(domainGrids,nativeString(indexNative(carrier,"domain_id")),1038);
            }
            const auto movable = [&](auto&& self, NodeRef node) -> bool {
                if(node==origin||node==grid)return false;
                if(node->typeId()=="Part"&&!faceParts.contains(node->getUUID()))return false;
                for(auto childRange=node->childrenList();auto child:childRange)if(!self(self,child))return false;return true;
            };
            NativeVector<NodeRef> units;
            for(auto [id,unused]:faceParts) {
                auto part=puppet->findNodeById(id);if(part==origin)continue;auto unit=part;
                while(unit->parentPtr()&&unit->parentPtr()!=origin&&movable(movable,unit->parentPtr()))unit=unit->parentPtr();
                if(unit->parentPtr()!=origin&&std::find(units.begin(),units.end(),unit)==units.end())units.push_back(unit);
            }
            for(auto unit:units)movePreservingFrame(unit,origin);
        }
        for(auto& target:targets) {auto part=puppet->findNodeById(uuid(indexNative(target,"part")));
            assignNative(target,"relative_zsort",part->relZSort());assignNative(target,"composite_scope",scopeOf(part));}
        if(humanoid) {
            NativeIntegerMap<std::uint32_t,framework::Text> assigned;JsonArray sourceGrids;
            for(auto target:targets) {
                auto surface=puppet->findNodeById(uuid(indexNative(target,"grid"))),part=puppet->findNodeById(uuid(indexNative(target,"part")));
                bool reached=false;
                for(auto node=part->parentPtr();node;node=node->parentPtr()) {
                    if(node==surface){reached=true;break;}if(node->typeId()!="GridDeformer")continue;
                    auto domain=nativeString(indexNative(target,"domain_id"));
                    if(auto previous=assigned.find(node->getUUID());previous!=assigned.end()){
                        enforceNative(previous->second==domain,"Imported group grid spans incompatible semantic surfaces",1078);continue;}
                    compat::Slice<Point2> points;
                    auto deformable=std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node);
                    const auto sourceVertices=deformable->getVertices();
                    for(std::size_t i=0;i<sourceVertices.size();++i){auto v=sourceVertices.at(i);points.append(toRoot(node,v.x,v.y));}
                    auto xs=ngRigNumbers(indexNative(target,"xs")),ys=ngRigNumbers(indexNative(target,"ys"));
                    const auto x0=readNativeArray(xs,0,1085);
                    const auto y0=readNativeArray(ys,0,1085);
                    const auto width=readNativeArray(xs,xs.size()-1,1085)-x0;
                    const auto height=readNativeArray(ys,ys.size()-1,1085)-y0;
                    enforceNative(width>0&&height>0,"Semantic surface has degenerate bounds",1086);
                    for(auto& point:points){auto frame=ngRigNumbers(indexNative(target,"parent_to_root"));
                        point[0]-=readNativeArray(frame,2,1089);point[1]-=readNativeArray(frame,5,1089);}
                    auto sampled=registered::ngRigSampleRegisteredDepth(xs,ys,ngRigNumbers(indexNative(target,"depth")),points);
                    auto depth=createNativeObject<SetDepthsCommand>();depth->target=node;
                    for(auto v:sampled){enforceNative(std::isfinite(v),"Imported group surface depth is nonfinite",1096);depth->depths.append(static_cast<float>(v));}
                    commandSource(
                        [&] {
                            return depth;
                        },
                        [&] {
                            return editorContext();
                        });
                    for(auto bone:indexNative(target,"bones").sourceArrayValue()) {auto add=createNativeObject<AddDepthBoneSourceCommand>();
                        add->root=root;add->target=node;add->bone=readNativeMap(bones,nativeString(bone),1103);commandSource(
                            [&] {
                                return add;
                            },
                            [&] {
                                return editorContext();
                            });}
                    assigned[node->getUUID()]=domain;sourceGrids.append(objectEvidence({{stringLiteralEvidence("grid"),node->getUUID()},{stringLiteralEvidence("surface"),indexNative(target,"grid")}}));
                }
                enforceNative(reached,"Material escaped its semantic surface",1109);
            }
            assignNative(state,"source_group_grids",sourceGrids);
        }
        puppet->root->build();puppet->rescanNodes();assignNative(result,"kind",core::settings::SettingsString(
            ngRigString(program,stringLiteralEvidence("kind"),stringLiteralEvidence("humanoid"))));
        Json originIds=core::settings::SettingsJsonObject{};for(auto [name,node]:origins)assignNative(originIds,name,node->getUUID());
        assignNative(result,"material_origins",originIds);assignNative(result,"rigRoot",root->getUUID());assignNative(result,"targets",targets);
        assignNative(result,"program_sha256",indexNative(program,"content_sha256"));
    });
    // Grid processing is completed below, outside the main-thread callback.
    if(humanoid) {
        NativeIntegerMap<std::uint32_t, bool> processed;
        for(auto target:indexNative(result,"targets").sourceArrayValue()) {
            auto id=uuid(indexNative(target,"grid"));if(processed.contains(id))continue;processed[id]=true;
            using Processor=viewport::vertex::automesh::grid::GridAutoMeshProcessor;
            auto processor=createNativeObject<Processor>();processor->maskThreshold=15;processor->margin=0;
            processor->xSegments=1;processor->ySegments=1;processor->scaleX={0,1};processor->scaleY={0,1};
            auto applyGrid=[&] {
                std::shared_ptr<commands::CommandResult> applied;
                task->runOnMainThread([&] {auto grid=project::incActivePuppet()->findNodeById(id);
                    applied=commandSource(
                        [&] {
                            return createNativeObject<commands::automesh::dynamic::ApplyAutoMeshPT<Processor>>( processor,compat::SharedDelegate<bool()>{[task]{return task->isCanceled();}});
                        },
                        [&] {
                            return editorContext({grid});
                        });});
                applied=applied->waitForCompletion();enforceNativeLazy(applied->succeeded,[&] { return applied->message; },1135);
            };
            applyGrid();compat::Slice<double> boundsX,boundsY;
            task->runOnMainThread([&] {auto grid=std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(
                project::incActivePuppet()->findNodeById(id));auto data=serializedJson(grid);
                boundsX=ngRigNumbers(indexNative(data,"grid_axis_x"));boundsY=ngRigNumbers(indexNative(data,"grid_axis_y"));});
            enforceNative(boundsX.size()==2&&boundsY.size()==2&&boundsX[1]>boundsX[0]&&boundsY[1]>boundsY[0],"Grid AutoMesh returned degenerate alpha bounds",1145);
            processor->scaleX=advanced(ngRigNumbers(indexNative(target,"xs")),boundsX);
            processor->scaleY=advanced(ngRigNumbers(indexNative(target,"ys")),boundsY);applyGrid();Json generated;
            task->runOnMainThread([&] {
                auto grid=std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(project::incActivePuppet()->findNodeById(id));
                auto data=serializedJson(grid);auto xs=ngRigNumbers(indexNative(data,"grid_axis_x")),ys=ngRigNumbers(indexNative(data,"grid_axis_y"));
                compat::Slice<Point2> local,world;
                const auto sourceVertices=grid->getVertices();
                for(std::size_t i=0;i<sourceVertices.size();++i){auto p=sourceVertices.at(i);local.append(Point2(p.x,p.y));world.append(toRoot(grid,p.x,p.y));}
                auto values=[&] {
                    const auto sampleXs=ngRigNumbers(indexNative(target,"xs"));
                    const auto sampleYs=ngRigNumbers(indexNative(target,"ys"));
                    const auto sampleDepths=ngRigNumbers(indexNative(target,"depth_model_units"));
                    return registered::ngRigSampleRegisteredDepth(sampleXs,sampleYs,sampleDepths,local);
                }();
                for(auto& value:values)value=std::rint(value/ngRigNumber(indexNative(program,"native_depth_scale"))*1e6)/1e6;
                auto depth=createNativeObject<SetDepthsCommand>();depth->target=grid;
                for(auto v:values)depth->depths.append(static_cast<float>(v));commandSource(
                    [&] {
                        return depth;
                    },
                    [&] {
                        return editorContext();
                    });
                generated=objectEvidence({{stringLiteralEvidence("xs"),valuesJson(xs)},{stringLiteralEvidence("ys"),valuesJson(ys)},{stringLiteralEvidence("depth"),valuesJson(values)},
                    {stringLiteralEvidence("points"),ngRigPointsJson(world)},{stringLiteralEvidence("processor"),"grid"},{stringLiteralEvidence("direct_mesh_definition"),false}});
            });
            for(auto& updated:indexNative(result,"targets").sourceArrayValue())if(uuid(indexNative(updated,"grid"))==id)
                for(auto key:{"xs","ys","depth","points"})assignNative(updated,key,indexNative(generated,key));
            task->previewJson(nativeText({"grid-automesh-",ngRigDigest(indexNative(target,"domain_id")).view().substr(0,16)}),generated);
        }
        task->runOnMainThread([&] {
            auto puppet=project::incActivePuppet();auto root=puppet->findNodeById(uuid(indexNative(result,"rigRoot")));
            for(auto source:get(state,"source_group_grids",JsonArray{}).sourceArrayValue()) {
                auto node=std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(puppet->findNodeById(uuid(indexNative(source,"grid"))));
                for(auto target:indexNative(result,"targets").sourceArrayValue())if(uuid(indexNative(target,"grid"))==uuid(indexNative(source,"surface"))) {
                    auto frame=ngRigNumbers(indexNative(target,"parent_to_root"));compat::Slice<Point2> points;
                    const auto sourceVertices=node->getVertices();
                    for(std::size_t i=0;i<sourceVertices.size();++i){auto v=sourceVertices.at(i);auto p=toRoot(node,v.x,v.y);
                        const auto pointX=p[0]-readNativeArray(frame,2,1187);
                        const auto pointY=p[1]-readNativeArray(frame,5,1187);
                        points.append(Point2{pointX,pointY});}
                    auto values=[&] {
                        const auto sampleXs=ngRigNumbers(indexNative(target,"xs"));
                        const auto sampleYs=ngRigNumbers(indexNative(target,"ys"));
                        const auto sampleDepths=ngRigNumbers(indexNative(target,"depth"));
                        return registered::ngRigSampleRegisteredDepth(sampleXs,sampleYs,sampleDepths,points);
                    }();
                    auto depth=createNativeObject<SetDepthsCommand>();depth->target=node;
                    for(auto value:values)depth->depths.append(static_cast<float>(value));commandSource(
                        [&] {
                            return depth;
                        },
                        [&] {
                            return editorContext();
                        });
                    if(auto rule=target.sourceObjectRef().if_contains("bone_influence_rule")) {
                        auto influence=createNativeObject<SetDepthBoneInfluenceRuleCommand>();influence->root=root;influence->target=node;
                        influence->rule=compat::ngJsonStringify(*rule);commandSource(
                            [&] {
                                return influence;
                            },
                            [&] {
                                return editorContext();
                            });
                    }break;
                }
            }
        });
    }
    return result;
}
}
