#include "detail/registered.hpp"
#include "detail/head_template_data.hpp"
#include "detail/hair_surface.hpp"
#include <array>
#include <map>
#include <numbers>
#include <numeric>
#include <set>
#include <string>
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
// Build upper and lower slope-limited envelopes in registered model units.
// Averaging the two preserves the local depth level instead of moving the
// whole surface forward or backward when a transition needs room.
void limitHeadRelief(compat::Slice<double> z, compat::Slice<double> xs,
                     compat::Slice<double> ys, double slopeX, double slopeY) {
    auto lo = z.duplicate(), hi = z.duplicate();
    auto edge = [&](std::size_t from, std::size_t to, double step) {
        lo[to] = std::min(lo[to],lo[from]+step);
        hi[to] = std::max(hi[to],hi[from]-step);
    };
    const auto n = xs.size();
    for (std::size_t j=0; j<ys.size(); ++j) {
        for (std::size_t i=1; i<n; ++i) edge(j*n+i-1,j*n+i,slopeX*(xs[i]-xs[i-1]));
        for (std::size_t i=n-1; i>0; --i) edge(j*n+i,j*n+i-1,slopeX*(xs[i]-xs[i-1]));
    }
    for (std::size_t i=0; i<n; ++i) {
        for (std::size_t j=1; j<ys.size(); ++j) edge((j-1)*n+i,j*n+i,slopeY*(ys[j]-ys[j-1]));
        for (std::size_t j=ys.size()-1; j>0; --j) edge(j*n+i,(j-1)*n+i,slopeY*(ys[j]-ys[j-1]));
    }
    for (std::size_t i=0; i<z.size(); ++i) z[i] = std::midpoint(lo[i],hi[i]);
}

struct NeckSurface {
    Point2 top;
    Point2 down;
    double height;
    double radius;
    double topDepth;
    double baseDepth;
    std::vector<head_surface::Row> outline;

    double station(Point2 p) const { return dot(sub(p,top),down); }
    Point2 across() const { return {down[1],-down[0]}; }
    double distanceToNeck(Point2 p) const {
        const double y=std::max(dot(p,down),outline.front().y);
        const double x=dot(p,across());
        const auto bounds=head_surface::sample(outline,y);
        if (y<=outline.back().y && x>=bounds[0] && x<=bounds[1]) return 0.;
        const auto distanceSquared=[&](Point2 a,Point2 b) {
            const auto edge=sub(b,a);
            const auto delta=sub(Point2{x,y},a);
            const auto t=std::clamp(dot(delta,edge)/dot(edge,edge),0.,1.);
            const auto remainder=sub(delta,{t*edge[0],t*edge[1]});
            return dot(remainder,remainder);
        };
        const auto& last=outline.back();
        double closest=distanceSquared({last.bounds[0],last.y},{last.bounds[1],last.y});
        for (std::size_t i=1;i<outline.size();++i) {
            const auto& a=outline[i-1];
            const auto& b=outline[i];
            for (std::size_t side=0;side<2;++side)
                closest=std::min(closest,distanceSquared({a.bounds[side],a.y},{b.bounds[side],b.y}));
        }
        return std::sqrt(closest);
    }
    double depth(Point2 p, double original) const {
        const double outside=std::max(0.,distanceToNeck(p)-.15*radius);
        // Bound the depth change away from the painted neck in every
        // direction, including where the lower collar flares to the sides.
        const double ramp=.5*radius;
        const double rise=outside<ramp ? outside*outside/(2.*ramp) : outside-.5*ramp;
        const double limit=std::lerp(topDepth,baseDepth,head_surface::ease(station(p)/height))+.70*rise;
        return std::min(original,limit);
    }
};

void fitNeckSurface(Json& surface, const NeckSurface& neck, const auto& depthAt,
                    const Json* skin=nullptr) {
    const auto frame=ngRigNumbers(at(surface,"parent_to_root"));
    const Point2 origin{frame[2],frame[5]};
    const auto oldXs=ngRigNumbers(at(surface,"xs")), oldYs=ngRigNumbers(at(surface,"ys"));
    const auto oldDepths=ngRigNumbers(at(surface,"depth_model_units"));
    auto xs=oldXs.duplicate(), ys=oldYs.duplicate();
    const auto add=[&](compat::Slice<double>& axis,double value) {
        const auto bounds=std::ranges::minmax_element(axis);
        if (value>*bounds.min && value<*bounds.max &&
            std::ranges::none_of(axis,[&](double existing) {return std::abs(existing-value)<.015*neck.radius;}))
            axis.append(value);
    };
    if (skin) {
        // Sample the actual parent Grid's field at the neck, using its rows
        // and columns so the child Grid does not smooth across that contact.
        const auto skinFrame=ngRigNumbers(at(*skin,"parent_to_root"));
        const double reach=2.5*neck.radius;
        for (const auto x : ngRigNumbers(at(*skin,"xs"))) {
            const double world=x+skinFrame[2];
            if (std::abs(world-neck.top[0])<reach) add(xs,world-origin[0]);
        }
        const double bottom=neck.top[1]+neck.outline.back().y-dot(neck.top,neck.down);
        for (const auto y : ngRigNumbers(at(*skin,"ys"))) {
            const double world=y+skinFrame[5];
            if (world>=neck.top[1]-neck.radius && world<=bottom+reach) add(ys,world-origin[1]);
        }
    } else {
        const auto across=neck.across();
        const double first=dot(neck.top,neck.down);
        const double last=neck.outline.back().y;
        // Resolve the curved collar edge throughout the painted neck. A
        // single cell across the lower flare interpolates from the chest.
        const double length=last-neck.outline.front().y;
        const auto segments=static_cast<std::size_t>(std::clamp(std::ceil(length/(.4*neck.radius)),1.,24.));
        for (std::size_t i=1;i<segments;++i) {
            const double along=std::lerp(neck.outline.front().y,last,static_cast<double>(i)/segments);
            const auto row=head_surface::sample(neck.outline,along);
            add(ys,std::midpoint(row[0],row[1])*across[1]+along*neck.down[1]-origin[1]);
        }
        for (const double along : {first,first+.5*neck.height,first+neck.height,
                std::midpoint(first+neck.height,last),last,last+.65*neck.radius,last+1.5*neck.radius}) {
            const auto row=head_surface::sample(neck.outline,along);
            const double center=std::midpoint(row[0],row[1]);
            add(ys,center*across[1]+along*neck.down[1]-origin[1]);
            for (const double lateral : {row[0]-.7*neck.radius,row[0],center,row[1],row[1]+.7*neck.radius})
                add(xs,lateral*across[0]+along*neck.down[0]-origin[0]);
        }
    }
    std::ranges::sort(xs);
    std::ranges::sort(ys);
    compat::Slice<Point2> points;
    compat::Slice<double> depths;
    for (const auto y : ys) for (const auto x : xs) {
        const Point2 point{x+origin[0],y+origin[1]};
        points.append(point);
        depths.append(depthAt(point,sample(oldXs,oldYs,oldDepths,{x,y})));
    }
    set(surface,"xs",numbersJson(xs));
    set(surface,"ys",numbersJson(ys));
    set(surface,"points",ngRigPointsJson(points));
    set(surface,"depth_model_units",numbersJson(depths));
}

}
Json ngRigRegisterProgram(Json program, Json observation) {
    enforce(detail::registeredResourceDigest()=="C7DA02A179F7B5223CA9D0892B8D50329B62CDC8F71E31799C342A1D5DD01AF7",
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
        try {
            frame.install(spec,observed);
        } catch (const compat::ThrowableException& error) {
            throw compat::ThrowableException(contracts::detail::copyContractMessage(
                std::string(member.key())+": "+std::string(error.msg.view())), registeredSourceFile(), 241);
        }
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
    const auto options = contracts::ngRigGet(observation, literalKey("options"), Json{});
    const auto headYaw = options.is_object()
        ? contracts::ngRigGet(options, literalKey("head_yaw_degrees"), Json{})
        : Json{};
    if (!headYaw.is_null()) {
        const double degrees = ngRigNumber(headYaw);
        enforce(degrees > 0. && degrees < 90., "Head yaw must be between 0 and 90 degrees", 265);
    }
    for(auto curve:at(common,"bone_curves").sourceArrayValue()) {
        enforce(at(curve,"interpolation").as_string().view()=="Linear","Unsupported common template interpolation",265);
        auto xs=ngRigNumbers(at(at(curve,"axes"),std::size_t{0}));
        auto ys=ngRigNumbers(at(at(curve,"axes"),std::size_t{1}));
        SettingsJsonArray values; const auto unit=at(curve,"units").as_string();
        enforce(unit.view()=="torso_length" || unit.view()=="radians","Unsupported common bone curve unit",268);
        const auto parameter = at(curve,"parameter");
        const auto bone = at(curve,"bone");
        const auto property = at(curve,"property");
        const bool sharedBodyYaw = parameter.as_string().view() == "Body::Yaw-Pitch" &&
            bone.as_string().view() == "Spine" && property.as_string().view() == "transform.r.y";
        double scale = sharedBodyYaw ? .5 : (unit.view()=="torso_length" ? torso : 1.);
        if (!headYaw.is_null() && parameter.as_string().view() == "Face::Yaw-Pitch" &&
            bone.as_string().view() == "Head" && property.as_string().view() == "transform.r.y") {
            const auto end = at(at(curve,"values"),xs.size()-1);
            const auto templateMagnitude = std::abs(ngRigNumber(at(end,std::size_t{0})));
            scale = ngRigNumber(headYaw) * std::numbers::pi / (180. * templateMagnitude);
        }
        for(std::size_t i=0;i<xs.size();++i) for(std::size_t j=0;j<ys.size();++j)
            values.append(object({objectField("key",array({indexed(xs,i,269),indexed(ys,j,269)})),
                objectField("value",ngRigNumber(at(at(at(curve,"values"),i),j))*scale)}));
        drivers.append(object({objectField("parameter",parameter),objectField("bone",bone),
            objectField("binding",property),objectField("values",values)}));
        // Pelvis-owned surfaces need yaw too. Spine inherits half of the turn
        // through its parent and contributes the remaining half locally.
        if (sharedBodyYaw)
            drivers.append(object({objectField("parameter",parameter),objectField("bone",literalKey("Pelvis")),
                objectField("binding",property),objectField("values",values)}));
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
        // Authored local depth detail can require more samples than the reference mesh.
        if (component.sourceObjectRef().if_contains("sampling_resolution")) {
            auto requested = ngRigNumbers(at(component,"sampling_resolution"));
            nx = std::max(nx,static_cast<std::size_t>(requested[0]));
            ny = std::max(ny,static_cast<std::size_t>(requested[1]));
        }
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
                auto surfaceDepth = (planeOffset + sampled) * currentUnit;
                if (role.view()=="hair_back") {
                    // The posterior cap meets the skull's middle plane at its
                    // rim, not the anterior facial surface. Keep hanging hair
                    // behind the neck instead of pulling it onto that surface.
                    const auto face=at(at(common,"components"),"face");
                    const auto faceDepth=at(face,"depth");
                    auto faceFrame=aa(frames,at(face,"frame").as_string().view(),338);
                    const double front=(ngRigNumber(at(faceDepth,"plane_offset"))+
                        sample(ngRigNumbers(at(face,"axis_x")),ngRigNumbers(at(face,"axis_y")),
                            ngRigNumbers(at(faceDepth,"relief")),faceFrame.toFrame(p)))*currentUnit;
                    const double rx=(2*p[0]-faceBounds[0]-faceBounds[2])/(faceBounds[2]-faceBounds[0]);
                    const double ry=(2*p[1]-faceBounds[1]-faceBounds[3])/(faceBounds[3]-faceBounds[1]);
                    const double hemisphere=std::sqrt(std::max(0.,1-rx*rx-ry*ry));
                    const double thickness=std::max(0.,front-surfaceDepth);
                    const double lower=std::clamp(ry,0.,1.);
                    const double attachment=1.-lower*lower*(3.-2.*lower);
                    surfaceDepth+=attachment*thickness*.5*(1.-hemisphere);
                    // Keep the rear mass behind the face while preserving the
                    // curvature of the cap and the profile of hanging locks.
                    surfaceDepth-=(faceBounds[2]-faceBounds[0])*(.10 + .07*hemisphere);
                }
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
        if (at(domain,"chart").as_string().view() != "neck")
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
    // Seed the fitted head's placement and establish rear-hair depth.
    // A successfully fitted head supplies the final facial depth below.
    const double faceWidth = faceBounds[2]-faceBounds[0];
    const double yaw = headYaw.is_null() ? 30. : ngRigNumber(headYaw);
    const double tanYaw = std::tan(yaw*std::numbers::pi/180.);
    for (auto domain : at(program,"domains").sourceArrayValue()) {
        auto& surface = aa(registered,at(domain,"id").as_string().view(),399);
        const auto role = at(surface,"reference_component").as_string();
        const auto name = role.view();
        if (name!="face" && name!="hair_back") continue;
        auto depth = ngRigNumbers(at(surface,"depth_model_units"));
        if (name=="face") for (auto& value : depth) value+=faceWidth*.09;
        // At strong yaw, too large a depth change per unit of horizontal
        // distance reverses the screen order of adjacent samples. Spread
        // the relief before projection, leaving the neutral XY grid intact.
        if (tanYaw > .65)
            limitHeadRelief(depth,ngRigNumbers(at(surface,"xs")),ngRigNumbers(at(surface,"ys")),
                .68/tanYaw,.49);
        if (name == "hair_back" && !depth.empty()) {
            double sum = 0.;
            for (const double z : depth) sum += z;
            const double mean = sum/depth.size();
            for (auto& z : depth) z = std::lerp(mean,z,1./3.);
        }
        set(surface,"depth_model_units",numbersJson(depth));
    }
    std::optional<hair_surface::Shell> hairShell;
    std::vector<contracts::Point3> facialDepthMesh;
    struct SideHairLayer {
        Json* surface;
        compat::Slice<double> drawOrder;
        std::uint64_t sourceOrder;
    };
    std::map<std::string,std::vector<SideHairLayer>> sideLayers;
    const auto torsoComponent=at(at(common,"components"),"torso");
    const auto torsoDepth=at(torsoComponent,"depth");
    const auto torsoXs=ngRigNumbers(at(torsoComponent,"axis_x"));
    const auto torsoYs=ngRigNumbers(at(torsoComponent,"axis_y"));
    const auto torsoRelief=ngRigNumbers(at(torsoDepth,"relief"));
    auto torsoFrame=aa(frames,at(torsoComponent,"frame").as_string().view(),401);
    const double torsoPlane=ngRigNumber(at(torsoDepth,"plane_offset"))+
        ngRigNumber(at(at(common,"bone_z"),at(torsoDepth,"origin").as_string().view()));
    const auto torsoZ = [&](Point2 p) {
        return torso*(torsoPlane+sample(torsoXs,torsoYs,torsoRelief,torsoFrame.toFrame(p)));
    };
    Json neckMaterial, faceMaterial;
    std::size_t faceArea=0;
    for (const auto& material : at(observation,"materials").sourceArrayValue()) {
        if (at(material,"static").as_bool() || at(material,"role").as_string().view()!="neck") continue;
        if (neckMaterial.is_null() || ngRigUnsigned(at(material,"source_order"))<
            ngRigUnsigned(at(neckMaterial,"source_order"))) neckMaterial=material;
    }
    for (const auto& material : at(observation,"materials").sourceArrayValue()) {
        if (at(material,"static").as_bool() || at(material,"role").as_string().view()!="face") continue;
        const auto runs=at(material,"alpha_runs_32").sourceArrayValue();
        std::size_t area=0;
        for (std::size_t i=1;i<runs.size();i+=2) area+=ngRigUnsigned(runs[i]);
        if (area>faceArea) { faceArea=area; faceMaterial=material; }
    }
    const auto face=byRole.find("face");
    if (face!=byRole.end() && faceArea>0 && faceWidth>0. && faceBounds[3]>faceBounds[1]) {
        // Fit the common depth frame to the face's chin and the upper neck.
        // All head consumers use this same frame for the anatomical joints.
        const auto field=*face->second;
        const auto frame=ngRigNumbers(at(field,"parent_to_root"));
        const auto depth=ngRigNumbers(at(field,"depth_model_units"));
        const double center=std::midpoint(faceBounds[0],faceBounds[2]);
        const double chinDepth=sample(ngRigNumbers(at(field,"xs")),ngRigNumbers(at(field,"ys")),
            depth,{center-frame[2],faceBounds[3]-frame[5]});
        const auto& sections=head_template::neck;
        const auto contour=observation::ngRigAlphaContour(faceMaterial);
        const auto outline=ngRigPoints(at(contour,"points"));
        double left=inf,right=-inf,top=inf,chin=-inf;
        for (const auto p : outline) {
            left=std::min(left,p[0]); right=std::max(right,p[0]);
            top=std::min(top,p[1]); chin=std::max(chin,p[1]);
        }
        if (!(right>left && chin>top)) {
            left=faceBounds[0]; top=faceBounds[1]; right=faceBounds[2]; chin=faceBounds[3];
        }
        const double w=right-left, h=chin-top;
        const double x=std::midpoint(left,right);
        const auto faceXs=ngRigNumbers(at(field,"xs"));
        const auto faceYs=ngRigNumbers(at(field,"ys"));
        const double faceDet=frame[0]*frame[4]-frame[1]*frame[3];
        const auto sampleFace = [&](Point2 p) {
            const double tx=p[0]-frame[2], ty=p[1]-frame[5];
            if (std::abs(faceDet)<1e-10) return chinDepth;
            return sample(faceXs,faceYs,depth,{(frame[4]*tx-frame[1]*ty)/faceDet,
                (frame[0]*ty-frame[3]*tx)/faceDet});
        };
        const double chinZ=std::isfinite(w) && w>0. ? sampleFace({x,chin}) : chinDepth;
        const double originZ=chinZ-head_template::chinDepth*w;
        const head_template::Fit fit{x,top,w,h,originZ,w};
        auto leftJaw=fit.point(head_template::leftJaw), rightJaw=fit.point(head_template::rightJaw);
        auto leftEar=fit.point(head_template::leftEar), rightEar=fit.point(head_template::rightEar);
        auto crown=fit.point(head_template::crown.front());
        double neckCenter=x, neckWidth=sections.front()[1]*w;
        double neckFront=fit.depth(sections.front()[2]), neckBackDepth=fit.depth(sections.front()[3]);
        const double templateNeckCenterZ=std::midpoint(neckFront,neckBackDepth);
        const double neckHalfDepth=.5*(neckFront-neckBackDepth);
        double neckJunctionY=top+head_surface_reference::nape[1]*h;
        std::optional<contracts::Point3> neckReference;
        if (!neckMaterial.is_null()) {
            const auto neckContour=observation::ngRigAlphaContour(neckMaterial);
            const auto neckOutline=ngRigPoints(at(neckContour,"points"));
            double neckTop=inf, lo=inf, hi=-inf;
            for (const auto p : neckOutline) {
                neckTop=std::min(neckTop,p[1]);
                if (std::abs(p[1]-chin)<.06*w) {lo=std::min(lo,p[0]);hi=std::max(hi,p[0]);}
            }
            if (hi>lo) { neckCenter=std::midpoint(lo,hi); neckWidth=(hi-lo)*.5; }
            const double neckSkin=torsoZ({neckCenter,neckTop+.04*w});
            neckFront=std::min(chinZ-.24*w,neckSkin)-.125*w;
            neckBackDepth=neckSkin-(.125+.37)*w;
            leftJaw[1]=rightJaw[1]=neckTop+.04*w;
            neckJunctionY=leftJaw[1];
            const Point2 neckTopPoint{neckCenter,neckJunctionY};
            const double neckHeight=dot(sub(neck,neckTopPoint),down);
            const double radius=std::min(neckWidth,.30*width);
            const head_surface::Frame neckFrame{{down[1],-down[0]},down};
            auto rows=head_surface::outlineRows(neckOutline,neckFrame,
                ngRigNumber(at(neckContour,"pixel_step")));
            const auto body=byRole.find("torso");
            if (neckHeight>.1*radius && radius>0. && !rows.empty() && body!=byRole.end()) {
                auto& skin=*body->second;
                const auto frame=ngRigNumbers(at(skin,"parent_to_root"));
                const auto xs=ngRigNumbers(at(skin,"xs")), ys=ngRigNumbers(at(skin,"ys"));
                const auto depths=ngRigNumbers(at(skin,"depth_model_units"));
                const auto offset=Point2{frame[2],frame[5]};
                const double shoulderDepth=std::midpoint(sample(xs,ys,depths,sub(upperArmLeft,offset)),
                    sample(xs,ys,depths,sub(upperArmRight,offset)));
                // The shoulder samples describe the outer torso, while the
                // neck joins it further back. Keep the upper neck in front
                // of the fitted nape but behind the base of the neck.
                const double baseDepth=shoulderDepth-.65*radius;
                const double topDepth=std::min(neckFront,baseDepth-.65*radius);
                const NeckSurface profile{neckTopPoint,down,neckHeight,radius,topDepth,
                    baseDepth,std::move(rows)};
                fitNeckSurface(skin,profile,[&](Point2 p,double z) {return profile.depth(p,z);});
                const auto skinFrame=ngRigNumbers(at(skin,"parent_to_root"));
                const auto skinXs=ngRigNumbers(at(skin,"xs")), skinYs=ngRigNumbers(at(skin,"ys"));
                const auto skinDepths=ngRigNumbers(at(skin,"depth_model_units"));
                const double pixel=ngRigNumber(at(neckContour,"pixel_step"));
                double upperLeft=inf, upperRight=-inf;
                for (const auto p : neckOutline)
                    if (std::abs(p[1]-neckJunctionY)<=pixel) {
                        upperLeft=std::min(upperLeft,p[0]);
                        upperRight=std::max(upperRight,p[0]);
                    }
                const Point2 attachment{upperRight>upperLeft ? std::midpoint(upperLeft,upperRight) : neckCenter,
                    neckJunctionY};
                const double front=sample(skinXs,skinYs,skinDepths,
                    sub(attachment,{skinFrame[2],skinFrame[5]}));
                neckReference=contracts::Point3{attachment[0],attachment[1],front-neckHalfDepth};
                for (const auto& domain : at(program,"domains").sourceArrayValue()) {
                    if (at(domain,"owner").as_string().view()!="torso") continue;
                    const auto chart=at(domain,"chart").as_string().view();
                    if (chart!="topwear_front" && chart!="attachment:chest") continue;
                    auto& surface=aa(registered,at(domain,"id").as_string().view(),401);
                    if (&surface==&skin) continue;
                    fitNeckSurface(surface,profile,[&](Point2 p,double z) {
                        const double above=profile.outline.front().y-dot(p,down);
                        const double distance=profile.distanceToNeck(p);
                        const double weight=(1.-head_surface::ease(above/radius))*
                            (1.-head_surface::ease((distance-.60*radius)/(3.5*radius)));
                        const double depth=sample(skinXs,skinYs,skinDepths,sub(p,{skinFrame[2],skinFrame[5]}));
                        return std::lerp(z,depth,weight);
                    },&skin);
                }
            }
            const auto eyes=aa(landmarks,"head",402);
            double eye=std::midpoint(aa(eyes,"eye_l_outer",402)[1],aa(eyes,"eye_r_outer",402)[1]);
            std::vector<double> irisRows;
            for (const auto& material : at(observation,"materials").sourceArrayValue()) {
                if (at(material,"static").as_bool() ||
                    ngRigString(material,text("feature"),text("")).view()!="iris") continue;
                for (const auto p : ngRigPoints(at(material,"cloud"))) irisRows.push_back(p[1]);
            }
            if (!irisRows.empty()) {
                const auto middle=irisRows.begin()+irisRows.size()/2;
                std::nth_element(irisRows.begin(),middle,irisRows.end());
                eye=*middle;
            }
            // Transfer the relative ear height from the finished mesh.
            // Its lower rows have already been reparameterized at the jaw.
            constexpr double eyeHeight=.60;
            constexpr double earHeight=(head_template::leftEar[1]+head_template::rightEar[1])*.5;
            constexpr double jawHeight=head_template::leftJaw[1];
            leftEar[1]=rightEar[1]=std::lerp(eye,leftJaw[1],(earHeight-eyeHeight)/(jawHeight-eyeHeight));
            const double neckAxis=neckSkin-.185*w;
            const double earZ=std::midpoint(leftEar[2],rightEar[2]);
            leftEar[2]+=neckAxis-earZ;
            rightEar[2]+=neckAxis-earZ;
            const double jawZ=std::midpoint(leftJaw[2],rightJaw[2]);
            leftJaw[2]+=neckSkin-jawZ;
            rightJaw[2]+=neckSkin-jawZ;
        }
        const auto savePoint = [](const contracts::Point3& p) {return array({p[0],p[1],p[2]});};
        contracts::Point3 headOffset{0.,0.,0.};
        const auto eyes=aa(landmarks,"head",402);
        const auto leftEye=aa(eyes,"eye_l_outer",402), rightEye=aa(eyes,"eye_r_outer",402);
        const auto anatomicalFrame=head_surface::alignedFrame(leftEye,rightEye);
        const auto painted=head_surface::outlineRows(outline,anatomicalFrame,ngRigNumber(at(contour,"pixel_step")));
        const auto eyeRow=anatomicalFrame.local({std::midpoint(leftEye[0],rightEye[0]),
            std::midpoint(leftEye[1],rightEye[1])})[1];
        const auto neckRow=anatomicalFrame.local({neckCenter,neckJunctionY});
        if (painted.size()>=3) {
            const auto& bottom=painted.back();
            const auto chinPoint=anatomicalFrame.model({std::midpoint(bottom.bounds[0],bottom.bounds[1]),
                bottom.y,0.});
            if (auto fitted=head_surface::fit(painted,anatomicalFrame,eyeRow,
                sampleFace({chinPoint[0],chinPoint[1]}),{neckRow[0],neckRow[1],neckBackDepth})) {
                const contracts::Point3 fallbackPivot{fitted->nape[0],fitted->nape[1],
                    std::midpoint(neckFront,fitted->nape[2])};
                const auto pivot=neckReference.value_or(fallbackPivot);
                if (neckReference) {
                    // The reference origin comes from the standard 3D neck
                    // section, before the torso field supplies its own
                    // limits. Using that field on both sides cancels the
                    // depth correction when the head turns.
                    // The fitted surface is already registered to the artwork
                    // in X and Y. The neck sets the pose pivot independently.
                    headOffset[2]=pivot[2]-templateNeckCenterZ;
                    fitted->translate(headOffset);
                    for (const auto& domain : at(program,"domains").sourceArrayValue()) {
                        if (at(domain,"owner").as_string().view()!="head") continue;
                        auto& surface=aa(registered,at(domain,"id").as_string().view(),402);
                        auto depths=ngRigNumbers(at(surface,"depth_model_units"));
                        for (auto& z : depths) z+=headOffset[2];
                        set(surface,"depth_model_units",numbersJson(depths));
                    }
                }
                facialDepthMesh=fitted->mesh;
                const auto savePoints = [&](const std::vector<contracts::Point3>& points) {
                    SettingsJsonArray result;
                    for (const auto p : points) result.append(savePoint(p));
                    return Json(result);
                };
                set(program,"head_fitted_surface",object({
                    objectField("points",savePoints(fitted->mesh)),
                    objectField("left_section",savePoints(fitted->earLine[0])),
                    objectField("right_section",savePoints(fitted->earLine[1])),
                    objectField("left_lower_section",savePoints(fitted->lowerEarLine[0])),
                    objectField("right_lower_section",savePoints(fitted->lowerEarLine[1])),
                    objectField("left_ear",savePoint(fitted->ear[0])),
                    objectField("right_ear",savePoint(fitted->ear[1])),
                    objectField("left_jaw",savePoint(fitted->jaw[0])),
                    objectField("right_jaw",savePoint(fitted->jaw[1])),
                    objectField("left_upper",savePoint(fitted->upper[0])),
                    objectField("right_upper",savePoint(fitted->upper[1])),
                    objectField("chin",savePoint(fitted->chin)),
                    objectField("nape",savePoint(fitted->nape))}));
                const bool hasHair=std::ranges::any_of(byRole,[](const auto& member) {
                    const auto role=member.first.view();
                    return role=="hair_front" || role.starts_with("hair_side/");
                });
                if (hasHair) hairShell=hair_surface::fit(fitted->mesh,anatomicalFrame,w);
                if (hairShell) {
                    std::array<double,4> hairBounds{inf,inf,-inf,-inf};
                    for (const auto& domain : at(program,"domains").sourceArrayValue()) {
                        const auto& surface=aa(registered,at(domain,"id").as_string().view(),401);
                        const auto role=at(surface,"reference_component").as_string().view();
                        if (role!="hair_front" && !role.starts_with("hair_side/")) continue;
                        const auto b=ngRigNumbers(at(surface,"bounds"));
                        hairBounds[0]=std::min(hairBounds[0],b[0]);
                        hairBounds[1]=std::min(hairBounds[1],b[1]);
                        hairBounds[2]=std::max(hairBounds[2],b[2]);
                        hairBounds[3]=std::max(hairBounds[3],b[3]);
                    }
                    double maxYaw=0.;
                    for (const auto& driver : drivers) {
                        if (at(driver,"parameter").as_string().view()!="Face::Yaw-Pitch" ||
                            at(driver,"bone").as_string().view()!="Head" ||
                            at(driver,"binding").as_string().view()!="transform.r.y") continue;
                        for (const auto& key : at(driver,"values").sourceArrayValue())
                            maxYaw=std::max(maxYaw,std::abs(ngRigNumber(at(key,"value"))));
                    }
                    compat::Slice<double> rearX,rearY,rearZ,rearFrame;
                    double crownJoin=inf;
                    const double faceTop=painted.front().y;
                    if (const auto rear=byRole.find("hair_back"); rear!=byRole.end()) {
                        const auto& surface=*rear->second;
                        rearX=ngRigNumbers(at(surface,"xs"));
                        rearY=ngRigNumbers(at(surface,"ys"));
                        rearZ=ngRigNumbers(at(surface,"depth_model_units"));
                        rearFrame=ngRigNumbers(at(surface,"parent_to_root"));
                        Json rearMaterial;
                        for (const auto& material : at(observation,"materials").sourceArrayValue()) {
                            if (at(material,"static").as_bool() ||
                                at(material,"role").as_string().view()!="hair_back") continue;
                            const auto bounds=ngRigNumbers(at(material,"bounds"));
                            if (bounds[0]<x && x<bounds[2] && bounds[1]<top &&
                                (rearMaterial.is_null() || bounds[1]<ngRigNumbers(at(rearMaterial,"bounds"))[1]))
                                rearMaterial=material;
                        }
                        if (!rearMaterial.is_null()) {
                            for (const auto& material : at(observation,"materials").sourceArrayValue()) {
                                const auto role=at(material,"role").as_string().view();
                                if (at(material,"static").as_bool() || (role!="hair_front" && role!="hair_side")) continue;
                                compat::Slice<Point2> roots;
                                for (const auto p : ngRigPoints(at(material,"cloud")))
                                    if (anatomicalFrame.local(p)[1]<faceTop) roots.append(p);
                                if (roots.empty()) continue;
                                compat::Slice<bool> overlap;
                                observation::ngRigAlphaCoverage(rearMaterial,roots,text("alpha_runs_32"),&overlap);
                                for (std::size_t i=0;i<roots.size();++i)
                                    if (overlap[i]) crownJoin=std::min(crownJoin,anatomicalFrame.local(roots[i])[1]);
                            }
                        }
                    }
                    const auto joinCrown=[&](Point2 p,double depth,double floor) {
                        const double y=anatomicalFrame.local(p)[1];
                        if (!std::isfinite(crownJoin) || y>=faceTop || faceTop-crownJoin<.02*w)
                            return std::pair{depth,floor};
                        const double weight=1.-head_surface::ease((y-crownJoin)/(faceTop-crownJoin));
                        const double rx=p[0]-rearFrame[2], ry=p[1]-rearFrame[5];
                        const double det=rearFrame[0]*rearFrame[4]-rearFrame[1]*rearFrame[3];
                        if (std::abs(det)<=1e-9) return std::pair{depth,floor};
                        const double rear=sample(rearX,rearY,rearZ,{
                            (rearFrame[4]*rx-rearFrame[1]*ry)/det,(rearFrame[0]*ry-rearFrame[3]*rx)/det});
                        const double attached=std::lerp(depth,rear,weight);
                        const double minimum=std::isfinite(floor)?std::lerp(floor,std::min(floor,rear),weight):floor;
                        return std::pair{attached,minimum};
                    };
                    if (maxYaw>1e-6 && hairBounds[2]>hairBounds[0] && hairBounds[3]>hairBounds[1]) {
                        hair_surface::limitProjection(*hairShell,fitted->mesh,hairBounds,w,
                            .62/std::tan(maxYaw),.40,joinCrown);
                        if (std::isfinite(crownJoin)) set(program,"front_hair_crown_join",array({crownJoin,faceTop}));
                    }
                }
                if (!neckMaterial.is_null()) {
                    auto updated = at(scaffold,"bones").sourceArrayValue().duplicate();
                    for (auto& bone : updated) {
                        if (at(bone,"id").as_string().view()!="Head") continue;
                        const auto head=ngRigNumbers(at(bone,"head"));
                        const auto tail=ngRigNumbers(at(bone,"tail"));
                        set(bone,"head",array({pivot[0],pivot[1],head[2]}));
                        set(bone,"tail",array({pivot[0]+tail[0]-head[0],pivot[1]+tail[1]-head[1],tail[2]}));
                        set(bone,"pose_origin_z",pivot[2]);
                    }
                    set(scaffold,"bones",updated);
                    set(program,"head_neck_pivot",savePoint(pivot));
                }
            }
        }
        const auto placed=[&](contracts::Point3 p) {
            for (std::size_t i=0; i<3; ++i) p[i]+=headOffset[i];
            return savePoint(p);
        };
        set(program,"head_anatomical_fit",array({fit.centerX+headOffset[0],fit.faceTop+headOffset[1],
            fit.width,fit.height,fit.depthOrigin+headOffset[2],fit.depthScale}));
        set(program,"head_anatomical_landmarks",object({
            objectField("left_ear",placed(leftEar)),objectField("right_ear",placed(rightEar)),
            objectField("left_jaw",placed(leftJaw)),objectField("right_jaw",placed(rightJaw)),
            objectField("crown",placed(crown)),
            objectField("neck_front",neckReference ? (*neckReference)[2]+neckHalfDepth : neckFront),
            objectField("neck_back",neckReference ? (*neckReference)[2]-neckHalfDepth : neckBackDepth)}));
        for (const auto& domain : at(program,"domains").sourceArrayValue()) {
            if (at(domain,"chart").as_string().view()!="neck") continue;
            auto& surface=aa(registered,at(domain,"id").as_string().view(),401);
            const auto points=ngRigPoints(at(surface,"points"));
            compat::Slice<double> depths;
            for (const auto p : points) {
                const double v=(p[1]-fit.faceTop)/fit.height;
                const auto next=std::ranges::upper_bound(sections,v,{},[](const auto& section) {return section[0];});
                const auto i=std::clamp<std::ptrdiff_t>(next-sections.begin(),1,sections.size()-1);
                const auto& a=sections[i-1];
                const auto& b=sections[i];
                const double t=std::clamp((v-a[0])/(b[0]-a[0]),0.,1.);
                const double radius=neckWidth*std::lerp(a[1],b[1],t)/sections.front()[1];
                const double u=std::clamp((p[0]-neckCenter)/radius,-1.,1.);
                const double front=neckFront+w*(std::lerp(a[2],b[2],t)-sections.front()[2]);
                const double back=neckBackDepth+w*(std::lerp(a[3],b[3],t)-sections.front()[3]);
                depths.append(std::midpoint(front,back)+(front-back)*.5*std::sqrt(1.-u*u));
            }
            set(surface,"depth_model_units",numbersJson(depths));
        }
    }
    for (const auto& carrier : at(program,"carriers").sourceArrayValue()) {
        const auto domain=at(carrier,"domain_id").as_string();
        auto& surface=aa(registered,domain.view(),401);
        const auto role=at(surface,"reference_component").as_string().view();
        if (!role.starts_with("hair_side/")) continue;
        auto material=aa(materials,ngRigUnsigned(at(carrier,"part")),401);
        if (material.sourceObjectRef().if_contains("receiver")) continue;
        sideLayers[std::string(role)].push_back({&surface,
            ngRigNumbers(ngRigGet(material,literalKey("draw_order_zsort_path"),Json(SettingsJsonArray{}))),
            ngRigUnsigned(at(material,"source_order"))});
    }
    if (hairShell) {
        set(program,"front_hair_shell",hair_surface::encode(*hairShell));
        const auto filmDepth=[&](Json& surface, double offset) {
            compat::Slice<double> depths;
            for (const auto p : ngRigPoints(at(surface,"points"))) depths.append(hairShell->depth(p)+offset);
            set(surface,"depth_model_units",numbersJson(depths));
            set(surface,"hair_shell_offset",offset);
        };
        if (const auto fringe=byRole.find("hair_front"); fringe!=byRole.end()) {
            filmDepth(*fringe->second,.012*faceWidth);
        }
    }
    const auto compareDrawOrder = [](const auto& a, const auto& b) {
        for (std::size_t i=0; i<std::min(a.size(),b.size()); ++i) {
            if (a[i]>b[i]) return -1; // Larger zSort is drawn farther back.
            if (a[i]<b[i]) return 1;
        }
        return 0;
    };
    const auto faceOrder = faceMaterial.is_null() ? compat::Slice<double>{} :
        ngRigNumbers(ngRigGet(faceMaterial,literalKey("draw_order_zsort_path"),Json(SettingsJsonArray{})));
    std::set<Json*> paintedSideHair;
    for (auto& [side,layers] : sideLayers) {
        std::stable_sort(layers.begin(),layers.end(),[&](const auto& a,const auto& b) {
            // Old observations can lack a zSort path. Keep their source order.
            if (a.drawOrder.empty()!=b.drawOrder.empty()) return !a.drawOrder.empty();
            const int order=compareDrawOrder(a.drawOrder,b.drawOrder);
            if (order) return order<0;
            if (a.drawOrder.size()!=b.drawOrder.size()) return a.drawOrder.size()<b.drawOrder.size();
            return a.sourceOrder<b.sourceOrder;
        });
        for (std::size_t rank=0; rank<layers.size(); ++rank) {
            auto& layer=layers[rank];
            auto& surface=*layer.surface;
            if (!paintedSideHair.insert(&surface).second) continue;
            const int sideOfFace=compareDrawOrder(layer.drawOrder,faceOrder);
            const auto points=ngRigPoints(at(surface,"points"));
            auto depths=ngRigNumbers(at(surface,"depth_model_units"));
            const double rankFactor=(rank+1.)/layers.size();
            const double gap=(.012+.005*(sideOfFace<0 ? 1.-rankFactor : rankFactor))*faceWidth;
            double shift=hairShell ? gap : 0.;
            if (hairShell) for (std::size_t i=0;i<points.size();++i) depths[i]=hairShell->depth(points[i]);
            if (sideOfFace && face!=byRole.end()) {
                const auto& skin=*face->second;
                const auto xs=ngRigNumbers(at(skin,"xs")), ys=ngRigNumbers(at(skin,"ys"));
                const auto z=ngRigNumbers(at(skin,"depth_model_units"));
                const auto m=ngRigNumbers(at(skin,"parent_to_root"));
                const double det=m[0]*m[4]-m[1]*m[3];
                if (std::abs(det)>1e-10) {
                    shift=sideOfFace<0 ? std::min(0.,shift) : std::max(0.,shift);
                    for (std::size_t i=0;i<points.size();++i) {
                        const double x=points[i][0]-m[2], y=points[i][1]-m[5];
                        const double skinDepth=sample(xs,ys,z,{(m[4]*x-m[1]*y)/det,(m[0]*y-m[3]*x)/det});
                        const double offset=skinDepth+sideOfFace*gap-depths[i];
                        shift=sideOfFace<0 ? std::min(shift,offset) : std::max(shift,offset);
                    }
                    set(surface,"hair_depth_side_of_face",sideOfFace);
                }
            }
            for (auto& depth : depths) depth+=shift;
            set(surface,"depth_model_units",numbersJson(depths));
            if (hairShell) set(surface,"hair_shell_offset",shift);
        }
    }
    if (hairShell) for (const auto& domain : at(program,"domains").sourceArrayValue()) {
        auto& surface=aa(registered,at(domain,"id").as_string().view(),401);
        const auto part=at(surface,"reference_component").as_string().view();
        if ((!part.starts_with("hair_side/") || paintedSideHair.contains(&surface)) && part!="hair_front") continue;
        if (surface.sourceObjectRef().if_contains("hair_shell_offset")) continue;
        const double offset=.012*faceWidth;
        compat::Slice<double> depths;
        for (const auto p : ngRigPoints(at(surface,"points"))) depths.append(hairShell->depth(p)+offset);
        set(surface,"depth_model_units",numbersJson(depths));
        set(surface,"hair_shell_offset",offset);
    }
    // Hair layers already have their own fitted surface and placement.
    // Apply facial relief after that placement so padded face samples cannot
    // shift an entire side-hair layer.
    const auto& headLandmarks=aa(landmarks,"head",402);
    const auto mouthCenter=aa(headLandmarks,"mouth",402);
    const auto mouthAxis=sub(aa(headLandmarks,"mouth_l",402),aa(headLandmarks,"mouth_r",402));
    const auto mouthWidth=norm(mouthAxis);
    Point2 mouthRadius{.5*mouthWidth,0.};
    for (const auto& material : at(observation,"materials").sourceArrayValue()) {
        if (at(material,"static").as_bool() || at(material,"role").as_string().view()!="face_feature")
            continue;
        const auto feature=ngRigGet(material,literalKey("feature"),Json(literalKey(""))).as_string().view();
        if (!feature.starts_with("mouth")) continue;
        const auto b=ngRigNumbers(at(material,"bounds"));
        for (std::size_t axis=0;axis<2;++axis)
            mouthRadius[axis]=std::max({mouthRadius[axis],mouthCenter[axis]-b[axis],b[axis+2]-mouthCenter[axis]});
    }
    if (!facialDepthMesh.empty()) for (const auto& domain : at(program,"domains").sourceArrayValue()) {
        auto& surface=aa(registered,at(domain,"id").as_string().view(),401);
        if (at(surface,"reference_component").as_string().view()!="face") continue;
        const auto xs=ngRigNumbers(at(surface,"xs")),ys=ngRigNumbers(at(surface,"ys"));
        const auto frame=ngRigNumbers(at(surface,"parent_to_root"));
        const auto mouthLocal=sub(mouthCenter,{frame[2],frame[5]});
        const auto neutralDepth=ngRigNumbers(at(surface,"depth_model_units"));
        const bool mouthValid=mouthWidth>1e-6 && mouthRadius[1]>1e-6;
        const double mouthDepth=mouthValid ? head_surface::frontDepthAt(facialDepthMesh,mouthCenter) : 0.;
        const double mouthReference=mouthValid ? sample(xs,ys,neutralDepth,mouthLocal) : 0.;
        compat::Slice<double> depths;
        for (const auto p : ngRigPoints(at(surface,"points"))) {
            double z=head_surface::frontDepthAt(facialDepthMesh,p);
            if (mouthValid) {
                const auto offset=sub(p,mouthCenter);
                const double distance=std::max(std::abs(offset[0])/mouthRadius[0],
                    std::abs(offset[1])/mouthRadius[1]);
                const double weight=1.-head_surface::ease(distance-1.);
                if (weight>0.) {
                    // Preserve the template's local depth differences across all
                    // mouth layers at the fitted surface's depth at the center.
                    const auto local=sub(p,{frame[2],frame[5]});
                    const double mouth=mouthDepth+sample(xs,ys,neutralDepth,local)-mouthReference;
                    z=std::lerp(z,mouth,weight);
                }
            }
            depths.append(z);
        }
        // The shared grid also carries the eyes and mouth. Apply the same
        // bound to the final fitted relief that the initial face field uses.
        if (tanYaw > .65)
            limitHeadRelief(depths,xs,ys,.68/tanYaw,.49);
        set(surface,"depth_model_units",numbersJson(depths));
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
    if (faceBounds[2]>faceBounds[0] && faceBounds[3]>faceBounds[1])
        set(program,"head_surface_fit",array({faceBounds[0],faceBounds[1],faceBounds[2],faceBounds[3],
        ngRigNumber(at(at(common,"bone_z"),"Head"))*torso}));
    set(program,"reference_template",object({objectField("sha256",at(common,"content_sha256")),objectField("count",1),
        objectField("reference_count",at(common,"reference_count")),objectField("torso_length",torso),
        objectField("motion_policy",at(common,"motion_policy"))}));
    return program;
}
}
