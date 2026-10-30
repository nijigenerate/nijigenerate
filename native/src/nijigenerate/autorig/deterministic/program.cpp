#include "program.hpp"
#include "templates.hpp"
#include "anatomy.hpp"
#include "hierarchy.hpp"
#include "registered.hpp"
#include "detail/native_maps.hpp"
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/core/selector/phobos_sort.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>
namespace nijigenerate::autorig::deterministic::program {
using namespace contracts;
using Json=core::settings::SettingsJsonValue;
using JsonArray=core::settings::SettingsJsonArray;
using String=core::settings::SettingsString;
namespace {
#if defined(_WIN32)
using SourceReal=double;
#else
using SourceReal=long double;
#endif
template<class V> using StringMap=nicxlive::core::common::ResourceKeyMap<String,V,
    core::selector::SelectorStringHash,core::selector::SelectorStringEqual>;
template<std::size_t N> String lit(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}
auto sourceFile(){return compat::RetainedString::fromStaticLiteral(
    "source/nijigenerate/autorig/deterministic/program.d");}
Json at(Json value,String key){return value.sourceIndexValue(key.view());}
JsonArray sourceArray(Json value){return value.sourceArrayValue();}
void set(Json& value,String key,Json entry){value.sourceIndexAssignSlot(key)=entry;}
String str(Json value){return value.as_string();}
Json get(Json value,String key,Json fallback={}){return ngRigGet(value,key,fallback);}
String stringGet(Json value,String key,String fallback=String::emptyLiteral()){
    return ngRigString(value,key,fallback);
}
Json object(std::initializer_list<core::settings::SettingsJsonMember> fields){
    core::settings::SettingsJsonObject result;
    for(const auto& field:fields)result[field.name]=field.item;
    return result;
}
Json array(std::initializer_list<Json> values){return JsonArray(values);}
void enforce(bool condition,String message,std::size_t line){
    if(!condition){
        auto copied=message.empty()?compat::RetainedString::fromStaticLiteral("Enforcement failed"):
            contracts::detail::copyContractMessage(message.view());
        throw compat::ThrowableException(std::move(copied),sourceFile(),line);
    }
}
bool starts(String value,String prefix){return value.view().starts_with(prefix.view());}
String operator+(String left,String right){
    return contracts::detail::allocateContractStorage([&]{
        std::string result(left.view());result.append(right.view());return String(std::move(result));
    });
}
Json numbers(compat::Slice<double> values){
    JsonArray result;for(auto value:values)result.append(value);return result;
}
void sortNumbers(compat::Slice<double> values){
    core::selector::detail::phobosQuickSort(std::span<double>(values.data(),values.size()),
        [](double a,double b){return a<b;},values.size(),1024/8);
}
void sortStrings(compat::Slice<String> values){
    core::selector::detail::phobosQuickSort(std::span<String>(values.data(),values.size()),
        [](const String& a,const String& b){return a.view()<b.view();},values.size(),1024/16);
}
template<class T> T& sourceAt(compat::Slice<T>& values,std::size_t index,std::size_t line){
    if(index>=values.size())throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),sourceFile(),line);
    return values.data()[index];
}
template<class Map,class Key> auto& sourceMapAt(Map& map,const Key& key,std::size_t line){
    const auto entry=map.find(key);
    if(entry==map.end())throw compat::ngStaticError<compat::RangeError>(sourceFile(),line);
    return entry->second;
}
double median(compat::Slice<double> values,std::size_t line){
    const double left=sourceAt(values,values.size()/2,line);
    const double right=sourceAt(values,(values.size()-1)/2,line);
    return (left+right)/2;
}
String replaceSide(String subject,String replacement){
    auto first=subject.view().find("{side}");
    if(first==std::string_view::npos)return subject;
    return contracts::detail::allocateContractStorage([&]{
        std::string result;std::size_t start=0;
        do{
            result.append(subject.view().substr(start,first-start));result.append(replacement.view());
            start=first+6;first=subject.view().find("{side}",start);
        }while(first!=std::string_view::npos);
        result.append(subject.view().substr(start));return String(std::move(result));
    });
}
double dmin(double a,double b){return b<a?b:a;}
double dmax(double a,double b){return a<b?b:a;}
double clamp(double value,double lower,double upper){
#ifndef NDEBUG
    if(lower>upper)throw compat::ngStaticError<compat::AssertError>(lit("Lower can't be greater than upper.").retainedString(),
        lit("std/algorithm/comparison.d").retainedString(),588);
#endif
    return value<lower?lower:value>upper?upper:value;
}
std::size_t sizeCast(double value){
    // Source cast has no defined portable result outside the representable domain.
    // Do not invent a SIZE_MAX sentinel or clamp for that target-native domain.
    return static_cast<std::size_t>(value);
}
}
Json ngRigCompileProgram(Json observation,Json evidenceValue) {
    auto rigKind=stringGet(evidenceValue,lit("kind"),lit("humanoid"));
    if(rigKind!=lit("humanoid")) {
        JsonArray targets;compat::Slice<double> sourceXs;
        for(auto m:sourceArray(at(observation,lit("materials"))))if(!at(m,lit("static")).as_bool()&&str(at(m,lit("role")))==lit("face"))
            for(auto p:ngRigPoints(at(m,lit("cloud"))))sourceXs.append(p[0]);
        if(sourceXs.empty())for(auto m:sourceArray(at(observation,lit("materials"))))if(!at(m,lit("static")).as_bool())
            for(auto p:ngRigPoints(at(m,lit("cloud"))))sourceXs.append(p[0]);
        sortNumbers(sourceXs);double center=median(sourceXs,25);
        for(auto m:sourceArray(at(observation,lit("materials"))))if(!at(m,lit("static")).as_bool()) {
            auto cloud=ngRigPoints(at(m,lit("cloud")));compat::Slice<double> xs;for(auto p:cloud)xs.append(p[0]);sortNumbers(xs);
            double position=median(xs,29);
            targets.append(object({{lit("part"),at(m,lit("uuid"))},{lit("path"),at(m,lit("path"))},{lit("owner"),at(m,lit("owner"))},{lit("chart"),at(m,lit("chart"))},{lit("role"),at(m,lit("role"))},
                {lit("side"),!stringGet(m,lit("side_override")).empty()?str(at(m,lit("side_override"))):position<center?lit("R"):lit("L")}}));
        }
        enforce(!targets.empty(),lit("No active local rig materials"),35);
        auto result=object({{lit("schema_version"),lit("rig-native-program-d/1")},{lit("kind"),rigKind},{lit("carriers"),targets},{lit("rootId"),at(observation,lit("rootId"))},
            {lit("source_sha256"),at(observation,lit("source_sha256"))},{lit("observation_sha256"),String(ngRigDigest(observation))},{lit("evidence_sha256"),String(ngRigDigest(evidenceValue))}});
        set(result,lit("content_sha256"),String(ngRigDigest(result)));return result;
    }
    auto prior=templates::ngRigHumanoidPrior(),scaffold=anatomy::ngRigSolveScaffold(evidenceValue,prior);
    double height=ngRigNumber(at(scaffold,lit("body_height"))),padding=height*ngRigNumber(at(prior,lit("bounds_padding_body_height"))),step=height*ngRigNumber(at(prior,lit("grid_step_body_height")));
    compat::Slice<double> faceXs;for(auto m:sourceArray(at(observation,lit("materials"))))if(!at(m,lit("static")).as_bool()&&str(at(m,lit("role")))==lit("face"))for(auto p:ngRigPoints(at(m,lit("cloud"))))faceXs.append(p[0]);
    sortNumbers(faceXs);double center=faceXs.empty()?ngRigPoint(at(at(scaffold,lit("landmarks")),lit("head_root")))[0]:median(faceXs,51);
    StringMap<Json> domains;
    native_detail::NativeIntegerMap<std::uint64_t,String> materialDomains,materialSides;
    native_detail::NativeIntegerMap<std::uint64_t,Json> activeMaterials;
    for(auto m:sourceArray(at(observation,lit("materials"))))if(!at(m,lit("static")).as_bool())activeMaterials[ngRigUnsigned(at(m,lit("uuid")))]=m;
    auto groupingSource=[&](auto&& self,Json material,compat::Slice<std::uint64_t> visited)->Json {
        auto id=ngRigUnsigned(at(material,lit("uuid")));enforce(std::find(visited.begin(),visited.end(),id)==visited.end(),lit("Cyclic clipping receiver in material grouping"),61);
        if(auto receiver=material.sourceObjectRef().if_contains("receiver")) {
            auto source=activeMaterials.find(ngRigUnsigned(*receiver));enforce(source!=activeMaterials.end(),lit("Clipping receiver is outside the active material set"),64);
            auto nextVisited=visited.duplicate();nextVisited.append(id);return self(self,source->second,nextVisited);
        }
        return material;
    };
    for(auto material:sourceArray(at(observation,lit("materials"))))if(!at(material,lit("static")).as_bool()) {
        auto grouping=groupingSource(groupingSource,material,compat::Slice<std::uint64_t>{});auto cloud=ngRigPoints(at(grouping,lit("cloud")));
        compat::Slice<double> sourceXs;for(auto p:cloud)sourceXs.append(p[0]);sortNumbers(sourceXs);
        double position=median(sourceXs,74);
        String side=position<center?lit("R"):lit("L"),role=str(at(grouping,lit("role")));
        if(role==lit("arm")||role==lit("hand")||role==lit("leg")||role==lit("foot")||role==lit("sleeve")) {
            std::size_t left=0;for(auto p:cloud)if(p[0]<center)++left;double fraction=static_cast<double>(left)/cloud.size();
            if(fraction>.15&&fraction<.85)side=lit("Both");
        }
        auto sideOverride=stringGet(material,lit("side_override"),stringGet(grouping,lit("side_override")));if(!sideOverride.empty())side=sideOverride;
        const auto ownerSubject=str(at(grouping,lit("owner")));
        const auto ownerSide=io::imageexport::detail::lowerExtension(side);
        auto owner=replaceSide(ownerSubject,ownerSide);
        const auto chartSubject=str(at(grouping,lit("chart")));
        const auto chartSide=io::imageexport::detail::lowerExtension(side);
        auto chart=replaceSide(chartSubject,chartSide);
        if(owner==lit("torso")&&chart==lit("neck"))chart=lit("body");
        if((starts(owner,lit("arm:"))||starts(owner,lit("leg:")))&&(chart==lit("hand")||chart==lit("foot")))chart=lit("skin");
        auto parent=at(observation,lit("rootId"));auto id=owner+lit("/")+chart+lit("@semantic");
        materialDomains[ngRigUnsigned(at(material,lit("uuid")))]=id;materialSides[ngRigUnsigned(at(material,lit("uuid")))]=side;
        compat::Slice<double> matrix{1.,0.,0.,0.,1.,0.};
        const double inf=std::numeric_limits<double>::infinity();compat::Slice<double> bounds{inf,inf,-inf,-inf};
        for(auto p:cloud){bounds[0]=dmin(bounds[0],p[0]);bounds[1]=dmin(bounds[1],p[1]);bounds[2]=dmax(bounds[2],p[0]);bounds[3]=dmax(bounds[3],p[1]);}
        if(auto nominal=material.sourceObjectRef().if_contains("bounds"))bounds=ngRigNumbers(*nominal);
        if(auto existing=domains.find(id);existing!=domains.end()) {
            auto previous=ngRigNumbers(at(existing->second,lit("local_bounds")));
            const double b0=sourceAt(bounds,0,103),p0=sourceAt(previous,0,103);
            sourceAt(bounds,0,103)=dmin(b0,p0);
            const double b1=sourceAt(bounds,1,103),p1=sourceAt(previous,1,103);
            sourceAt(bounds,1,103)=dmin(b1,p1);
            const double b2=sourceAt(bounds,2,104),p2=sourceAt(previous,2,104);
            sourceAt(bounds,2,104)=dmax(b2,p2);
            const double b3=sourceAt(bounds,3,104),p3=sourceAt(previous,3,104);
            sourceAt(bounds,3,104)=dmax(b3,p3);
            set(existing->second,lit("local_bounds"),numbers(bounds));existing->second.sourceObjectNoRef().at("parts").as_array().append(at(material,lit("uuid")));
        }else domains[id]=object({{lit("id"),id},{lit("owner"),owner},{lit("chart"),chart},{lit("parent"),parent},{lit("parent_to_root"),numbers(matrix)},{lit("local_bounds"),numbers(bounds)},{lit("parts"),array({at(material,lit("uuid"))})}});
    }
    const double inf=std::numeric_limits<double>::infinity();std::array<double,4> extent{inf,inf,-inf,-inf};JsonArray carriers;
    for(auto material:sourceArray(at(observation,lit("materials")))) {
        if(at(material,lit("static")).as_bool())continue;auto role=str(at(material,lit("role")));
        auto domainId=sourceMapAt(materialDomains,ngRigUnsigned(at(material,lit("uuid"))),116);auto assembly=sourceMapAt(domains,domainId,116);
        auto owner=str(at(assembly,lit("owner"))),side=sourceMapAt(materialSides,ngRigUnsigned(at(material,lit("uuid"))),117);
        auto bounds=ngRigNumbers(at(assembly,lit("local_bounds"))),matrix=ngRigNumbers(at(assembly,lit("parent_to_root")));
        const double m0=sourceAt(matrix,0,120),m3=sourceAt(matrix,3,120);
        const double xScale=std::sqrt(m0*m0+m3*m3);
        const double m1=sourceAt(matrix,1,120),m4=sourceAt(matrix,4,120);
        const double yScale=std::sqrt(m1*m1+m4*m4);
        enforce(bounds.size()==4&&bounds[2]>bounds[0]&&bounds[3]>bounds[1],lit("Invalid material bounds"),121);
        bounds[0]-=padding/xScale;bounds[1]-=padding/yScale;bounds[2]+=padding/xScale;bounds[3]+=padding/yScale;
        const double nxValue=std::ceil((bounds[2]-bounds[0])*xScale/step);
        const double nxLower=ngRigNumber(at(prior,lit("minimum_grid_segments")));
        const double nxUpper=ngRigNumber(at(prior,lit("maximum_grid_segments")));
        const auto nx=sizeCast(clamp(nxValue,nxLower,nxUpper));
        const double nyValue=std::ceil((bounds[3]-bounds[1])*yScale/step);
        const double nyLower=ngRigNumber(at(prior,lit("minimum_grid_segments")));
        const double nyUpper=ngRigNumber(at(prior,lit("maximum_grid_segments")));
        const auto ny=sizeCast(clamp(nyValue,nyLower,nyUpper));
        compat::Slice<double> xs,ys;compat::Slice<Point2> points;
        for(std::size_t i=0;i<nx+1;++i)xs.append(std::rint((bounds[0]+(bounds[2]-bounds[0])*i/nx)*1e4)/1e4);
        for(std::size_t i=0;i<ny+1;++i)ys.append(std::rint((bounds[1]+(bounds[3]-bounds[1])*i/ny)*1e4)/1e4);
        compat::Slice<double> rootBounds{inf,inf,-inf,-inf};
        for(auto y:ys)for(auto x:xs) {
            const double a=sourceAt(matrix,0,133)*x;
            const double b=sourceAt(matrix,1,133)*y;
            const double c=sourceAt(matrix,2,133);
            const double px=a+b+c;
            const double d=sourceAt(matrix,3,133)*x;
            const double e=sourceAt(matrix,4,133)*y;
            const double f=sourceAt(matrix,5,133);
            Point2 p{px,d+e+f};points.append(p);
            rootBounds[0]=dmin(rootBounds[0],p[0]);rootBounds[1]=dmin(rootBounds[1],p[1]);
            rootBounds[2]=dmax(rootBounds[2],p[0]);rootBounds[3]=dmax(rootBounds[3],p[1]);
            extent[0]=dmin(extent[0],p[0]);extent[1]=dmin(extent[1],p[1]);
            extent[2]=dmax(extent[2],p[0]);extent[3]=dmax(extent[3],p[1]);
        }
        auto chart=str(at(assembly,lit("chart")));String kind=lit("surface");double offset=0;compat::Slice<String> bones;
        if(owner==lit("head")) {
            bones={lit("Head")};if(chart==lit("hair:back")){kind=lit("back_hair");offset=height*.003;}
            else if(starts(chart,lit("hair"))||starts(chart,lit("headwear"))||starts(chart,lit("ear")))offset=height*.004;
        }else if(owner==lit("torso")) {
            bones={lit("Pelvis"),lit("Spine"),lit("Chest"),lit("Neck")};
            if(chart==lit("skirt")||chart==lit("skirt_back")||chart==lit("apron")){kind=chart==lit("skirt_back")?lit("skirt_back"):lit("skirt_front");bones={lit("Pelvis")};if(chart==lit("apron"))offset=height*.003;}
            else if(chart==lit("tail")){kind=lit("appendage");bones={lit("Pelvis")};offset=-height*.025;}
            else if(chart==lit("neck")){kind=lit("neck");bones={lit("Neck")};}
            else if(chart==lit("attachment:chest"))bones={lit("Chest")};else if(chart==lit("attachment:pelvis"))bones={lit("Pelvis")};
        }else if(starts(owner,lit("arm:"))||starts(owner,lit("leg:"))) {
            compat::Slice<String> names=starts(owner,lit("arm:"))?compat::Slice<String>{lit("UpperArm"),lit("Forearm"),lit("Hand")}:compat::Slice<String>{lit("Thigh"),lit("Shin"),lit("Foot")};
            if(chart==lit("hand"))names={lit("Hand")};if(chart==lit("foot")||chart==lit("attachment:ankle"))names={lit("Foot")};if(chart==lit("attachment:thigh"))names={lit("Thigh")};
            auto tags=side==lit("Both")?compat::Slice<String>{lit("R"),lit("L")}:compat::Slice<String>{side};
            for(auto tag:tags)for(auto name:names)bones.append(name+lit(".")+tag);
            if(chart==lit("sleeve")||chart==lit("shoulder"))offset=ngRigNumber(at(at(evidenceValue,lit("limb_radii")),owner))*.2;
        }else throw compat::ThrowableException((lit("Unsupported anatomical material owner: ")+owner).retainedString(),sourceFile(),164);
        auto domain=object({{lit("owner"),owner},{lit("kind"),kind},{lit("side"),side},{lit("support_bounds"),numbers(rootBounds)},{lit("offset"),offset}});
        if(starts(owner,lit("arm:"))||starts(owner,lit("leg:")))set(domain,lit("radius"),at(at(evidenceValue,lit("limb_radii")),owner));
        compat::Slice<double> depths;if(side==lit("Both"))depths.resize(points.size(),0.);else depths=anatomy::ngRigDepthField(points,domain,scaffold,prior);
        JsonArray boneValues;for(auto b:bones)boneValues.append(b);
        carriers.append(object({{lit("part"),at(material,lit("uuid"))},{lit("path"),at(material,lit("path"))},{lit("role"),at(material,lit("role"))},{lit("chart"),at(assembly,lit("chart"))},
            {lit("unit"),get(material,lit("carrier_unit"),at(material,lit("uuid")))},{lit("owner"),owner},{lit("side"),side},{lit("bounds"),numbers(rootBounds)},{lit("xs"),numbers(xs)},
            {lit("domain_id"),domainId},{lit("parent"),at(assembly,lit("parent"))},{lit("parent_to_root"),at(assembly,lit("parent_to_root"))},{lit("ys"),numbers(ys)},
            {lit("points"),ngRigPointsJson(points)},{lit("depth"),numbers(depths)},{lit("bones"),boneValues},{lit("domain"),domain}}));
    }
    enforce(!carriers.empty(),lit("No active rig materials"),179);double depthScale=dmax(1.,dmax(extent[2]-extent[0],extent[3]-extent[1])*.42/2.9);
    for(auto& carrier:carriers){auto depths=ngRigNumbers(at(carrier,lit("depth")));for(auto& d:depths)d=std::rint(d/depthScale*1e6)/1e6;set(carrier,lit("depth"),numbers(depths));}
    auto keyStorage=domains.keys();
    auto keys=compat::Slice<String>::retainedView(keyStorage.retentionOwner(),keyStorage.data(),keyStorage.size());
    sortStrings(keys);JsonArray assembled;for(auto key:keys)assembled.append(sourceMapAt(domains,key,187));
    JsonArray drivers;
    auto driver=[&](String parameter,String bone,String binding,String priorKey,bool vertical,bool twoDimensional) {
        JsonArray values;auto magnitude=ngRigNumber(at(at(prior,lit("drivers")),priorKey))*std::numbers::pi_v<SourceReal>/180;
        for(double x:{-1.,-.5,0.,.5,1.})for(double y:twoDimensional?compat::Slice<double>{-1.,-.5,0.,.5,1.}:compat::Slice<double>{0.})
            values.append(object({{lit("key"),array({x,y})},{lit("value"),(vertical?y:x)*magnitude}}));
        drivers.append(object({{lit("parameter"),parameter},{lit("bone"),bone},{lit("binding"),binding},{lit("values"),values}}));
    };
    driver(lit("Face::Yaw-Pitch"),lit("Head"),lit("transform.r.y"),lit("head_yaw_degrees"),false,true);
    driver(lit("Face::Yaw-Pitch"),lit("Head"),lit("transform.r.x"),lit("head_pitch_degrees"),true,true);
    driver(lit("Face::Roll"),lit("Neck"),lit("transform.r.z"),lit("head_roll_degrees"),false,false);
    driver(lit("Body::Yaw-Pitch"),lit("Spine"),lit("transform.r.y"),lit("body_yaw_degrees"),false,true);
    driver(lit("Body::Yaw-Pitch"),lit("Spine"),lit("transform.r.x"),lit("body_pitch_degrees"),true,true);
    driver(lit("Body::Roll"),lit("Pelvis"),lit("transform.r.z"),lit("body_roll_degrees"),false,false);
    auto result=object({{lit("schema_version"),lit("rig-native-program-d/1")},{lit("scaffold"),scaffold},{lit("kind"),rigKind},{lit("native_depth_scale"),depthScale},
        {lit("domains"),assembled},{lit("native_drivers"),drivers},{lit("carriers"),carriers},{lit("rootId"),at(observation,lit("rootId"))},{lit("source_sha256"),at(observation,lit("source_sha256"))},
        {lit("observation_sha256"),String(ngRigDigest(observation))},{lit("evidence_sha256"),String(ngRigDigest(evidenceValue))}});
    result=registered::ngRigRegisterProgram(result,observation);
    const auto registeredCarriers=sourceArray(at(result,lit("carriers")));
    const auto registeredScaffold=at(result,lit("scaffold"));
    set(result,lit("hierarchy"),hierarchy::ngRigCompileHierarchy(observation,registeredCarriers,registeredScaffold));
    set(result,lit("content_sha256"),String(ngRigDigest(result)));return result;
}
}
