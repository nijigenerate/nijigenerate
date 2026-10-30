#include "detail/registered.hpp"
#include <array>
#include <string_view>
#include <vector>

namespace nijigenerate::autorig::deterministic::registered {
using namespace detail;
namespace {
Json duplicateObject(const Json& source) {
    SettingsJsonObject result;
    const auto members = source.sourceObjectRef();
    for (const auto& member : members) result[member.name] = member.item;
    return result;
}
}
Json ngRigRegisterProgram(Json program, Json observation) {
    enforce(detail::registeredResourceDigest()=="908B217553C870EA81B81902893AAB43B8D487871814127F048338989E865D71",
        "Embedded anatomical template differs from the original registered resource",196);
    auto common = json::ngParseAutoRigJson(
        compat::RetainedString::fromStaticView(detail::registeredResource()));
    enforce(at(common,"template_count").as_int64()==1 && !at(common,"character_selection").as_bool(),
        "Expected one common anatomical template",199);
    detail::RegisteredStringAA<Json> bones;
    for (auto bone : at(at(program,"scaffold"),"bones").sourceArrayValue()) bones[at(bone,"id").as_string()]=bone;
    auto get = [&](std::string_view name,std::string_view end="head") -> Point2 {
        auto p=ngRigNumbers(at(aa(bones,name,202),end)); return {indexed(p,0,202),indexed(p,1,202)};
    };
    auto pelvis=get("Pelvis"), neck=get("Neck"), vertical=sub(pelvis,neck);
    const auto torso=norm(vertical); auto down=vertical; down[0]/=torso; down[1]/=torso;
    Point2 across={down[1],-down[0]};
    const auto upperArmLeft = get("UpperArm.L");
    const auto upperArmRight = get("UpperArm.R");
    const auto width=std::abs(dot(sub(upperArmLeft,upperArmRight),across));
    const double inf=std::numeric_limits<double>::infinity();
    std::array<double,4> faceBounds={inf,inf,-inf,-inf};
    detail::RegisteredUlongAA<Json> materials;
    for(auto m:at(observation,"materials").sourceArrayValue()) {
        materials[ngRigUnsigned(at(m,"uuid"))]=m;
        if(!at(m,"static").as_bool() && at(m,"role").as_string().view()=="face") {
            auto b=ngRigNumbers(at(m,"bounds"));
            for(std::size_t i=0;i<2;++i) faceBounds[i]=dmin(faceBounds[i],indexed(b,i,212));
            for(std::size_t i=0;i<2;++i) faceBounds[i+2]=dmax(faceBounds[i+2],indexed(b,i+2,213));
        }
    }
    detail::RegisteredStringAA<ChartFrame> frames;
    frames[literalKey("body")]={pelvis,{across[0]*width,vertical[0],across[1]*width,vertical[1]}};
    frames[literalKey("head")]={{(faceBounds[0]+faceBounds[2])/2,(faceBounds[1]+faceBounds[3])/2},
        {faceBounds[2]-faceBounds[0],0.,0.,faceBounds[3]-faceBounds[1]}};
    detail::RegisteredStringAA<Landmarks> landmarks;
    landmarks[literalKey("head")]=ngRigFacialLandmarks(observation);
    for(auto name:compat::Slice<core::settings::SettingsString>{
            literalKey("Pelvis"),literalKey("Spine"),literalKey("Chest"),literalKey("Neck")})
        landmarks[literalKey("body")][lowerString(name)]=get(name.view());
    for(auto side:compat::Slice<core::settings::SettingsString>{literalKey("L"),literalKey("R")}) {
        const auto suffix=lowerString(side);
        landmarks[literalKey("body")][joinEnforceMessage("shoulder_",suffix.view())]=
            get(joinEnforceMessage("UpperArm.",side.view()));
        landmarks[literalKey("body")][joinEnforceMessage("hip_",suffix.view())]=
            get(joinEnforceMessage("Thigh.",side.view()));
        for(auto family:compat::Slice<core::settings::SettingsString>{literalKey("arm"),literalKey("leg")}) {
            const auto proximal=family.view()=="arm"?literalKey("UpperArm"):literalKey("Thigh");
            const auto joint=family.view()=="arm"?literalKey("Forearm"):literalKey("Shin");
            const auto distal=family.view()=="arm"?literalKey("Hand"):literalKey("Foot");
            const auto boneName=[&](core::settings::SettingsString name) {
                return joinEnforceMessage(joinEnforceMessage(name.view(),"."),side.view());
            };
            auto start=get(boneName(proximal)), axis=sub(get(boneName(distal)),start), direction=axis;
            const auto length=norm(axis); direction[0]/=length; direction[1]/=length;
            Point2 right={direction[1],-direction[0]};
            const auto region=core::settings::SettingsString(
                joinEnforceMessage(joinEnforceMessage(family.view(),"/"),suffix.view()));
            frames[region]={start,{right[0]*torso,axis[0],right[1]*torso,axis[1]}};
            landmarks[region]={{literalKey("root"),start},{literalKey("joint"),get(boneName(joint))},
                {literalKey("end"),get(boneName(distal))},{literalKey("tip"),get(boneName(distal),"tail")}};
        }
    }
    auto hipL=get("Thigh.L"),hipR=get("Thigh.R");
    landmarks[literalKey("body")][literalKey("pelvis")]={(hipL[0]+hipR[0])/2,(hipL[1]+hipR[1])/2};
    const auto semanticCharts = at(common,"semantic_charts");
    for(const auto& member:semanticCharts.sourceObjectRef()) {
        auto& frame=aa(frames,member.key(),241);
        const auto spec=member.value();
        const auto& observed=aa(landmarks,member.key(),241);
        frame.install(spec,observed);
    }
    for(auto& [name,bone]:bones) set(bone,"pose_origin_z",ngRigNumber(at(at(common,"bone_z"),name.view()))*torso);
    const auto clavicleLeft = ngRigNumber(at(aa(bones,"Clavicle.L",243),"pose_origin_z"));
    const auto clavicleRight = ngRigNumber(at(aa(bones,"Clavicle.R",244),"pose_origin_z"));
    set(aa(bones,"Chest",243),"pose_origin_z",dmin(clavicleLeft,clavicleRight));
    for(auto spec:at(common,"support_bones").sourceArrayValue()) {
        auto f=aa(frames,at(spec,"frame").as_string().view(),246); auto head=f.fromFrame(ngRigPoint(at(spec,"head"))),tail=f.fromFrame(ngRigPoint(at(spec,"tail")));
        auto z=ngRigNumbers(at(spec,"rest_z")); double pose=ngRigNumber(at(spec,"pose_origin_z"))*torso;
        if(!at(spec,"head_anchor").is_null()) {
            auto anchor=get(at(spec,"head_anchor").as_string().view()),delta=sub(anchor,head); head=anchor;
            tail={tail[0]+delta[0],tail[1]+delta[1]};
            pose=ngRigNumber(at(aa(bones,at(spec,"head_anchor").as_string().view(),252),"pose_origin_z"));
        }
        bones[at(spec,"id").as_string()]=object({objectField("id",at(spec,"id")),
            objectField("head",array({head[0],head[1],indexed(z,0,254)*torso})),
            objectField("tail",array({tail[0],tail[1],indexed(z,1,255)*torso})),
            objectField("rest_roll",at(spec,"rest_roll")),objectField("pose_origin_z",pose)});
    }
    SettingsJsonArray ordered;
    for(auto row:at(common,"support_skeleton").sourceArrayValue()) {
        auto bone=aa(bones,at(row,"bone").as_string().view(),259);
        for(auto key:{literalKey("parent"),literalKey("lock_to_root"),literalKey("allow_parent_to_targets")})
            set(bone,key,at(row,key.view()));
        ordered.append(bone);
    }
    SettingsJsonObject scaffoldOwner;
    auto& scaffold = program.sourceIndexReference("scaffold",scaffoldOwner);
    set(scaffold,"bones",ordered);
    SettingsJsonArray drivers;
    for(auto curve:at(common,"bone_curves").sourceArrayValue()) {
        enforce(at(curve,"interpolation").as_string().view()=="Linear","Unsupported common template interpolation",265);
        auto xs=ngRigNumbers(at(at(curve,"axes"),std::size_t{0}));
        auto ys=ngRigNumbers(at(at(curve,"axes"),std::size_t{1}));
        SettingsJsonArray values; const auto unit=at(curve,"units").as_string();
        enforce(unit.view()=="torso_length" || unit.view()=="radians","Unsupported common bone curve unit",268);
        for(std::size_t i=0;i<xs.size();++i) for(std::size_t j=0;j<ys.size();++j)
            values.append(object({objectField("key",array({indexed(xs,i,269),indexed(ys,j,269)})),
                objectField("value",ngRigNumber(at(at(at(curve,"values"),i),j))*
                    (unit.view()=="torso_length"?torso:1.))}));
        drivers.append(object({objectField("parameter",at(curve,"parameter")),objectField("bone",at(curve,"bone")),
            objectField("binding",at(curve,"property")),objectField("values",values)}));
    }
    set(program,"native_drivers",drivers);
    std::array<double,4> extent={inf,inf,-inf,-inf}; detail::RegisteredStringAA<Json> registered;
    for(auto domain:at(program,"domains").sourceArrayValue()) {
        auto carrier=at(at(program,"carriers"),std::size_t{0});
        for(auto c:at(program,"carriers").sourceArrayValue()) {
            const auto carrierId=at(c,"domain_id").as_string();
            const auto domainId=at(domain,"id").as_string();
            if(carrierId.view()==domainId.view()) { carrier=c; break; }
        }
        const auto owner=at(domain,"owner").as_string();
        bool bilateral=owner.view()=="arm:both" || owner.view()=="leg:both";
        const std::string_view family=owner.view().starts_with("arm:")?"arm":"leg";
        const auto role=bilateral?core::settings::SettingsString(joinEnforceMessage(family,"/r")):
            componentRole(domain);
        enforceLazy(at(common,"components").sourceObjectRef().if_contains(role.view())!=nullptr,
            [&] { return joinEnforceMessage("Missing common template surface: ", role.view()); },282);
        auto component=at(at(common,"components"),role.view());
        enforce(at(component,"orientation_accepted").as_bool() && at(component,"deformations").sourceObjectRef().empty(),"Invalid common template surface authority",284);
        auto f=aa(frames,at(component,"frame").as_string().view(),286); Point2 origin=bilateral?Point2{0.,0.}:f.origin;
        std::array<double,4> bounds={inf,inf,-inf,-inf};
        for(auto id:at(domain,"parts").sourceArrayValue()) { auto b=ngRigNumbers(at(aa(materials,ngRigUnsigned(id),289),"bounds"));
            for(std::size_t i=0;i<2;++i) bounds[i]=dmin(bounds[i],indexed(b,i,290)-origin[i]);
            for(std::size_t i=0;i<2;++i) bounds[i+2]=dmax(bounds[i+2],indexed(b,i+2,291)-origin[i]);
        }
        const auto bodyHeight=ngRigNumber(at(at(program,"scaffold"),"body_height"));
        const auto paddingRatio=ngRigNumber(at(templates::ngRigHumanoidPrior(),"bounds_padding_body_height"));
        const auto padding=bodyHeight*paddingRatio;
        bounds[0]-=padding; bounds[1]-=padding; bounds[2]+=padding; bounds[3]+=padding;
        std::size_t nx=0,ny=0;
        auto resolution=[&](Json c) {
            for(auto r:at(c,"source_resolution").sourceArrayValue()) {
                nx=std::max(nx,static_cast<std::size_t>(ngRigUnsigned(at(r,std::size_t{0}))));
                ny=std::max(ny,static_cast<std::size_t>(ngRigUnsigned(at(r,std::size_t{1}))));
            }
        };
        resolution(component);
        if(bilateral) {
            resolution(at(at(common,"components"),joinEnforceMessage(family,"/l")));
            nx*=2;
        }
        compat::Slice<double> ax,ay;
        if(!bilateral) for(auto label:at(component,"vertex_landmarks").sourceArrayValue()) {
            const auto frameName=at(component,"frame").as_string();
            const auto& frameLandmarks=aa(landmarks,frameName.view(),307);
            const auto landmarkName=label.as_string();
            auto p=sub(aa(frameLandmarks,landmarkName.view(),307),origin);
            if(p[0]>=bounds[0] && p[0]<=bounds[2] && p[1]>=bounds[1] && p[1]<=bounds[3]) { ax.append(p[0]); ay.append(p[1]); }
        }
        auto xs=anchoredAxis(bounds[0],bounds[2],nx,ax),ys=anchoredAxis(bounds[1],bounds[3],ny,ay);
        // Keep the original validation reads even though evaluation below reads each current component again.
        auto cx=ngRigNumbers(at(component,"axis_x")),cy=ngRigNumbers(at(component,"axis_y"));
        auto relief=ngRigNumbers(at(at(component,"depth"),"relief"));
        auto unit=at(at(component,"depth"),"units").as_string().view()=="face width"?faceBounds[2]-faceBounds[0]:torso;
        (void)cx; (void)cy; (void)relief; (void)unit;
        compat::Slice<double> depths; compat::Slice<Point2> points;
        for(auto y:ys) for(auto x:xs) {
            Point2 p={x+origin[0],y+origin[1]}; points.append(p); double depth=0,total=0;
            const auto sides=bilateral?
                compat::Slice<core::settings::SettingsString>{literalKey("r"),literalKey("l")}:
                compat::Slice<core::settings::SettingsString>{literalKey("")};
            for(auto side:sides) {
                auto current=side.empty()?component:
                    at(at(common,"components"),joinEnforceMessage(joinEnforceMessage(family,"/"),side.view()));
                auto field=aa(frames,at(current,"frame").as_string().view(),319); double weight=1;
                if(bilateral) {
                    const auto suffix=side.view()=="r"?literalKey("R"):literalKey("L");
                    const auto names=family=="arm"?
                        compat::Slice<core::settings::SettingsString>{
                            literalKey("UpperArm"),literalKey("Forearm"),literalKey("Hand")}:
                        compat::Slice<core::settings::SettingsString>{
                            literalKey("Thigh"),literalKey("Shin"),literalKey("Foot")};
                    const auto boneName=[&](std::size_t index) {
                        const auto name=indexed(names,index,index==2?324:323);
                        return joinEnforceMessage(joinEnforceMessage(name.view(),"."),suffix.view());
                    };
                    compat::Slice<Point2> line{
                        get(boneName(0)),get(boneName(1)),get(boneName(2)),get(boneName(2),"tail")};
                    double distance=inf;
                    for(std::size_t i=0;i<3;++i) {
                        const auto next=indexed(line,i+1,327),previous=indexed(line,i,327);
                        const auto axis=sub(next,previous);
                        const auto t=clamp(dot(sub(p,indexed(line,i,327)),axis)/dot(axis,axis),0.,1.);
                        distance=dmin(distance,norm({p[0]-indexed(line,i,328)[0]-t*axis[0],
                            p[1]-indexed(line,i,328)[1]-t*axis[1]}));
                    }
                    auto squared=dmax(distance,torso*.002); squared*=squared; weight=1/(squared*squared);
                }
                const auto currentUnit=at(at(current,"depth"),"units").as_string().view()=="face width"?faceBounds[2]-faceBounds[0]:torso;
                const auto planeOffset = ngRigNumber(at(at(current,"depth"),"plane_offset"));
                const auto fieldXs = ngRigNumbers(at(current,"axis_x"));
                const auto fieldYs = ngRigNumbers(at(current,"axis_y"));
                const auto fieldRelief = ngRigNumbers(at(at(current,"depth"),"relief"));
                const auto framePoint = field.toFrame(p);
                const auto sampled = sample(fieldXs,fieldYs,fieldRelief,framePoint);
                const auto surfaceDepth = (planeOffset + sampled) * currentUnit;
                const auto& boneDepths=at(common,"bone_z");
                const auto originName=at(at(current,"depth"),"origin").as_string();
                const auto originDepth=ngRigNumber(at(boneDepths,originName.view()));
                depth += weight * (surfaceDepth + originDepth * torso);
                total+=weight;
            }
            depths.append(depth/total);
        }
        set(carrier,"xs",numbersJson(xs)); set(carrier,"ys",numbersJson(ys)); set(carrier,"points",ngRigPointsJson(points));
        set(carrier,"parent_to_root",array({1.,0.,origin[0],0.,1.,origin[1]}));
        set(carrier,"bounds",array({bounds[0]+origin[0],bounds[1]+origin[1],bounds[2]+origin[0],bounds[3]+origin[1]}));
        set(carrier,"depth_model_units",numbersJson(depths));
        set(carrier,"reference_component",bilateral?
            core::settings::SettingsString(joinEnforceMessage(family,"/both")):role);
        if(auto binding=component.sourceObjectRef().if_contains("bone_binding");binding && !bilateral) {
            set(carrier,"bones",at(*binding,"bones")); set(carrier,"bone_influence_rule",at(*binding,"influence_rule"));
        }
        auto b=ngRigNumbers(at(carrier,"bounds"));
        for(std::size_t i=0;i<2;++i) {
            extent[i]=dmin(extent[i],indexed(b,i,349));
            extent[i+2]=dmax(extent[i+2],indexed(b,i+2,349));
        }
        registered[at(domain,"id").as_string()]=carrier;
    }
    detail::RegisteredStringAA<Json*> byRole;
    for(auto domain:at(program,"domains").sourceArrayValue()) {
        auto& c=aa(registered,at(domain,"id").as_string().view(),354);
        byRole[at(c,"reference_component").as_string()]=&c;
    }
    if(auto body=byRole.find("torso");body!=byRole.end())
        for(auto side:compat::Slice<core::settings::SettingsString>{literalKey("L"),literalKey("R")}) {
        auto arm=byRole.find(joinEnforceMessage("arm/",lowerString(side).view()));
        if(arm==byRole.end()) continue;
        auto shoulder=get(joinEnforceMessage("UpperArm.",side.view())),
            axis=sub(get(joinEnforceMessage("UpperArm.",side.view()),"tail"),shoulder);
        auto fieldAt=[](Json c,Point2 p) {
            const auto matrix = ngRigNumbers(at(c,"parent_to_root"));
            p = sub(p,{indexed(matrix,2,361),indexed(matrix,5,361)});
            const auto xs = ngRigNumbers(at(c,"xs"));
            const auto ys = ngRigNumbers(at(c,"ys"));
            const auto depths = ngRigNumbers(at(c,"depth_model_units"));
            return sample(xs,ys,depths,p);
        };
        const auto bodyZ=fieldAt(*body->second,shoulder),armZ=fieldAt(*arm->second,shoulder);
        compat::Slice<double> weights;
        for(auto p:ngRigPoints(at(*arm->second,"points"))) { auto t=clamp(dot(sub(p,shoulder),axis)/dot(axis,axis),0.,1.); auto remaining=1-t; weights.append((remaining*remaining)*(1+2*t)); }
        auto matrix=ngRigNumbers(at(*arm->second,"parent_to_root")); auto xs=ngRigNumbers(at(*arm->second,"xs")),ys=ngRigNumbers(at(*arm->second,"ys"));
        auto anchor=sample(xs,ys,weights,sub(shoulder,{indexed(matrix,2,370),indexed(matrix,5,370)}));
        enforce(anchor>0,"Arm shoulder is outside its depth field",371);
        auto z=ngRigNumbers(at(*arm->second,"depth_model_units"));
        for(std::size_t i=0;i<z.size();++i) indexed(z,i,373)+=(bodyZ-armZ)*indexed(weights,i,373)/anchor;
        set(*arm->second,"depth_model_units",numbersJson(z));
    }
    detail::RegisteredUlongAA<core::settings::SettingsString> domainsByPart;
    for(auto c:at(program,"carriers").sourceArrayValue())
        domainsByPart[ngRigUnsigned(at(c,"part"))]=at(c,"domain_id").as_string();
    for(auto hair:at(observation,"materials").sourceArrayValue()) if(!at(hair,"static").as_bool() &&
        (at(hair,"role").as_string().view()=="hair_front" || at(hair,"role").as_string().view()=="hair_side"))
        for(auto skin:at(observation,"materials").sourceArrayValue()) if(!at(skin,"static").as_bool() &&
            at(skin,"role").as_string().view()=="face") {
            compat::Slice<Point2> overlap; auto cloud=ngRigPoints(at(hair,"cloud")); compat::Slice<bool> occupied;
            observation::ngRigAlphaCoverage(skin,cloud,text("alpha_runs_32"),&occupied);
            for(std::size_t i=0;i<cloud.size();++i) if(indexed(occupied,i,383)) overlap.append(indexed(cloud,i,383));
            if(overlap.empty()) continue;
            auto front=aa(domainsByPart,ngRigUnsigned(at(hair,"uuid")),385),back=aa(domainsByPart,ngRigUnsigned(at(skin,"uuid")),385);
            auto fc=aa(registered,front.view(),386),bc=aa(registered,back.view(),386); auto fm=ngRigNumbers(at(fc,"parent_to_root")),bm=ngRigNumbers(at(bc,"parent_to_root"));
            auto fz=ngRigNumbers(at(fc,"depth_model_units")),bz=ngRigNumbers(at(bc,"depth_model_units"));
            auto fx=ngRigNumbers(at(fc,"xs")),fy=ngRigNumbers(at(fc,"ys")),bx=ngRigNumbers(at(bc,"xs")),by=ngRigNumbers(at(bc,"ys"));
            double gap=-inf; const auto stride=(overlap.size()+255)/256;
            for(std::size_t i=0;i<overlap.size();i+=stride) {
                const auto p = indexed(overlap,i,392);
                const auto backDepth = sample(bx,by,bz,sub(p,{indexed(bm,2,392),indexed(bm,5,392)}));
                const auto frontDepth = sample(fx,fy,fz,sub(p,{indexed(fm,2,392),indexed(fm,5,392)}));
                gap = dmax(gap,backDepth-frontDepth);
            }
            auto shift=dmax(0.,gap+(faceBounds[2]-faceBounds[0])*.001); for(auto& z:fz) z+=shift;
            set(aa(registered,front.view(),396),"depth_model_units",numbersJson(fz));
        }
    const auto scale=dmax(1.,dmax(extent[2]-extent[0],extent[3]-extent[1])*.42/2.9);
    for(auto& carrier:at(program,"carriers").sourceArrayValue()) {
        auto old=carrier; carrier=duplicateObject(aa(registered,at(carrier,"domain_id").as_string().view(),401));
        for(auto key:{literalKey("part"),literalKey("path"),literalKey("role"),literalKey("unit"),literalKey("side")})
            set(carrier,key,at(old,key.view()));
        auto depths=ngRigNumbers(at(carrier,"depth_model_units")); for(auto& z:depths) z=std::rint(z/scale*1e6)/1e6;
        set(carrier,"depth",numbersJson(depths));
    }
    set(program,"native_depth_scale",scale);
    set(program,"reference_template",object({objectField("sha256",at(common,"content_sha256")),objectField("count",1),
        objectField("reference_count",at(common,"reference_count")),objectField("torso_length",torso),
        objectField("motion_policy",at(common,"motion_policy"))}));
    return program;
}
}
