#include "detail/physics.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
namespace nijigenerate::autorig::deterministic::physics {
using namespace detail;
namespace {
template<class Array> decltype(auto) arrayAt(Array& values, std::size_t index, std::size_t sourceLine) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
        sourceLine);
    return (values[index]);
}
template<class Range> Slice<std::uint64_t> materializeIds(const Range& sorted) {
    auto result=physics_detail::newArray<std::uint64_t>(sorted.size());
    for(std::size_t index=0;index<sorted.size();++index)result[index]=sorted[index];
    return result;
}

core::settings::SettingsString replaceStructuralSide(core::settings::SettingsString input,
        const core::settings::SettingsString& side,
        std::source_location source=std::source_location::current()) {
    constexpr std::string_view needle="{side}";
    const auto text=input.view();
    auto position=text.find(needle);
    if(position==text.npos)return input;
    try {
        std::string result;
        std::size_t copied=0;
        do {
            result.append(text.substr(copied,position-copied));
            if(!side.empty())result.append(side.view());
            copied=position+needle.size();
            position=text.find(needle,copied);
        }while(position!=text.npos);
        result.append(text.substr(copied));
        if(result.empty())return {};
        return core::settings::SettingsString(std::move(result));
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

core::settings::SettingsString structuralChart(const core::settings::SettingsString& owner,
        const core::settings::SettingsString& suffix,
        std::source_location source=std::source_location::current()) {
    try {
        std::string result(owner.view());
        result.push_back('/');
        if(!suffix.empty())result.append(suffix.view());
        return core::settings::SettingsString(std::move(result));
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

core::settings::SettingsString structuralLabel(core::settings::SettingsString chart,
        std::uint64_t id, bool displayName,
        std::source_location source=std::source_location::current()) {
    constexpr std::string_view prefix="Physics::",idSeparator="/",nameSeparator="::";
    try {
        const auto decimal=std::to_string(id);
        std::string result;
        if(displayName)result.append(prefix);
        if(!chart.empty())result.append(chart.view());
        result.append(displayName?nameSeparator:idSeparator);
        result.append(decimal);
        return core::settings::SettingsString(std::move(result));
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

core::settings::SettingsString structuralSideName(core::settings::SettingsString prefix,
        const core::settings::SettingsString& side,
        std::source_location source=std::source_location::current()) {
    try {
        std::string result(prefix.view());
        if(!side.empty())result.append(side.view());
        return core::settings::SettingsString(std::move(result));
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

Slice<Point2> structuralJoints(Point2 a,Point2 b,Point2 c,
        std::source_location source=std::source_location::current()) {
    try {
        return Slice<Point2>{a,b,c};
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

struct StructuralIndexDeleter {
    void operator()(evidence::AlphaSupportIndex* value) const noexcept {
        std::destroy_at(value);
        nicxlive::core::d_runtime::SourceAllocator<evidence::AlphaSupportIndex>{}.deallocate(value,1);
    }
};

std::unique_ptr<evidence::AlphaSupportIndex,StructuralIndexDeleter> structuralIndex(
        PhysicsAssets& assets,std::uint64_t id) {
    nicxlive::core::d_runtime::SourceAllocator<evidence::AlphaSupportIndex> allocator;
    // Ordinary class storage precedes evaluation of its constructor input.
    auto* storage=allocator.allocate(1);
    try {
        std::construct_at(storage,physicsMapValue(assets,id,355).points);
    }catch(...) {
        allocator.deallocate(storage,1);
        throw;
    }
    return std::unique_ptr<evidence::AlphaSupportIndex,StructuralIndexDeleter>(storage);
}
}
Json ngRigPhysicsStructure(Json state,Json program,PhysicsAssets& assets,std::shared_ptr<framework::AutoRigTaskContext> task) {
    assets=PhysicsAssets{};enforce(stringGet(program,"kind","humanoid").view()=="humanoid","Physics requires anatomical support",138);
    auto policy=ngRigPhysicsPolicy(),evidenceValue=state.sourceIndexValue("evidence");
    auto rules=templates::ngRigMaterialRoles().sourceIndexValue("rules").sourceArrayValue();
    auto landmarks=evidenceValue.sourceIndexValue("landmarks");
    PhysicsStringMap<Point2> joints;
    for(const auto& e:landmarks.sourceObjectRef())
        physicsMapSlot(joints,e.name)=ngRigPoint(e.item.sourceIndexValue("xy"));
    PhysicsUuidMap<Json> carriers,materials;
    for(auto carrier:program.sourceIndexValue("carriers").sourceArrayValue())
        physicsMapSlot(carriers,ngRigUnsigned(carrier.sourceIndexValue("part")))=carrier;
    for(auto material:state.sourceIndexValue("materials").sourceArrayValue())
    if(!material.sourceIndexValue("static").as_bool()){
        auto id=ngRigUnsigned(material.sourceIndexValue("uuid"));auto row=duplicate(material);Json role;
        for(auto rule:rules){
            const auto ruleId=at(rule,"id").as_string();
            const auto materialRole=at(material,"role").as_string();
            if(ruleId.view()==materialRole.view()){role=rule;break;}
        }
        enforce(role.is_object(),"Physics material has no structural role",150);
        const core::settings::SettingsString tag=io::imageexport::detail::lowerExtension(
            compat::RetainedStringSlice{at(physicsMapValue(carriers,id,151),"side").as_string()});
        const auto owner=replaceStructuralSide(at(role,"owner").as_string(),tag);
        const auto chartSuffix=replaceStructuralSide(at(role,"chart").as_string(),tag);
        const auto chart=structuralChart(owner,chartSuffix);
        set(row,"owner",owner);set(row,"chart",chart);set(row,"usage",at(role,"usage"));set(row,"rule",at(role,"id"));set(row,"side",tag);
        if(material.sourceObjectRef().if_contains("receiver"))set(row,"usage","decoration");
        else {auto current=currentRole(at(material,"name").as_string());if(!current.is_null()&&at(current,"usage").as_string().view()=="decoration")set(row,"usage",at(current,"usage"));}
        physicsMapSlot(materials,id)=row;
    }
    auto ids=[&] {
        auto sorted=[&] {
            try {
                return materials.keys();
            } catch(const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch(const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        if(sorted.size()>1)std::sort(sorted.data(),sorted.data()+sorted.size());
        return materializeIds(sorted);
    }();
    Array inventory,candidates;PhysicsUuidMap<std::size_t> inventoryIndex;
    for(auto id:ids){ngRigCheckpoint(task);auto m=physicsMapValue(materials,id,169);
        const auto usageText=at(m,"usage").as_string();
        const auto ownerText=at(m,"owner").as_string();
        const auto usage=usageText.view(),owner=ownerText.view();
        const auto name=io::imageexport::detail::lowerExtension(compat::RetainedStringSlice{at(m,"name").as_string()});
        physicsMapSlot(inventoryIndex,id)=inventory.size();
        auto row=object({
            {jsonLiteral("target"),id},
            {jsonLiteral("name"),at(m,"name")},
            {jsonLiteral("owner"),at(m,"owner")},
            {jsonLiteral("chart"),at(m,"chart")},
            {jsonLiteral("decision"),Json(jsonLiteral("exclude"))}});
        inventory.append(row);
        auto exclude=[&]<std::size_t N>(const char (&reason)[N],bool unresolved=false){
            set(row,"reason",jsonLiteral(reason));
            if(unresolved)set(row,"decision","unresolved");
        };
        auto matches=[&]<std::size_t N>(const char (&pattern)[N]){
            return physicsRegexSearch(name,jsonLiteral(pattern));
        };
        bool mixed=usage=="anatomy"&&owner.starts_with("arm:")&&matches("sleeve|cloth|frill");
        auto current=currentRole(at(m,"name").as_string());
        if(!current.is_null()&&contains({"anatomy","terminal","feature"},at(current,"usage").as_string().view())&&!mixed){exclude("Human body/terminal/feature candidate protected by current anatomy rules, including stale garment classifications");continue;}
        if(contains({"anatomy","terminal","feature"},usage)&&!mixed){exclude("Human body, joints, hands/fingers, feet/shoes or face: skeleton motion only");continue;}
        if(usage=="decoration"){exclude("Receiver decoration handled with its supported surface");continue;}
        if(usage=="covering"&&!owner.starts_with("arm:")&&!owner.starts_with("leg:")){exclude("Fitted torso/neck covering; no automatic chest/body sway");continue;}
        physicsMapSlot(assets,id)=ngRigPhysicsAsset(m,task);
        auto p=physicsMapValue(assets,id,193).points;bool included=false;
        struct Cleanup { PhysicsAssets& assets;std::uint64_t id;bool& included;~Cleanup(){if(!included)assets.erase(id);} } cleanup{assets,id,included};
        if(p.empty()){exclude("Empty alpha");continue;}
        const auto ruleText=at(m,"rule").as_string();
        const auto rule=ruleText.view();
        auto profile=jsonLiteral("hanging"),pattern=jsonLiteral("one_end");
        core::settings::SettingsString bone;Json limb;Point2 fixed,a,b,c,supportAxis;bool hasAxis=false;
        const auto side=at(m,"side").as_string().view()=="r"?jsonLiteral("R"):
            at(m,"side").as_string().view()=="l"?jsonLiteral("L"):jsonLiteral("");
        if((owner.starts_with("arm:")||owner.starts_with("leg:"))&&side.empty()){exclude("Shared or unknown limb side requires separate structural support regions",true);continue;}
        if(owner.starts_with("arm:")){
            a=physicsMapValue(joints,structuralSideName(jsonLiteral("shoulder."),side),205);
            b=physicsMapValue(joints,structuralSideName(jsonLiteral("elbow."),side),205);
            c=physicsMapValue(joints,structuralSideName(jsonLiteral("wrist."),side),205);
            const auto measuredRadius=ngRigNumber(at(at(evidenceValue,"limb_radii"),owner));
            const auto protectionScale=ngRigNumber(at(policy,"limb_protection_radius_scale"));
            const auto radius=measuredRadius*protectionScale;auto chain=structuralJoints(a,b,c);
            if(auto tip=joints.find(structuralSideName(jsonLiteral("hand_tip."),side));tip!=joints.end())chain.append(tip->second);
            double maximum=0;for(auto point:p)maximum=dmax(maximum,chainDistance(point,chain));
            if(maximum<=radius*1.2){exclude("Human body or fitted limb surface: entire alpha lies within the measured anatomical chain protection region");
                set(row,"structural_authority",object({
                    {jsonLiteral("joints"),ngRigPointsJson(chain)},
                    {jsonLiteral("measured_radius"),radius},
                    {jsonLiteral("max_alpha_distance"),maximum}}));
                continue;}
            limb=object({
                {jsonLiteral("joints"),ngRigPointsJson(structuralJoints(a,b,c))},
                {jsonLiteral("radius"),radius},
                {jsonLiteral("side"),side}});
            if(rule=="shoulder"||matches("cuff")){fixed=rule=="shoulder"?a:c;profile=jsonLiteral("frill");
                bone=structuralSideName(rule=="shoulder"?jsonLiteral("UpperArm."):jsonLiteral("Hand."),side);
                if(matches("cuff")&&!matches("bow|tassel|ribbon")){supportAxis=sub(c,b);hasAxis=true;}}
            else if(rule=="sleeve"||mixed){fixed=a;pattern=jsonLiteral("two_ends");profile=jsonLiteral("sleeve");
                bone=structuralSideName(jsonLiteral("UpperArm."),side);}
            else {exclude("Unresolved arm accessory material",true);continue;}
        }else if(owner.starts_with("leg:")){
            if(rule=="thigh_accessory"){exclude("Fitted thigh band");continue;}
            if(!matches("bow|ribbon|tassel|frill")){exclude("Fitted ankle covering");continue;}
            fixed=physicsMapValue(joints,structuralSideName(jsonLiteral("ankle."),side),225);
            bone=structuralSideName(jsonLiteral("Foot."),side);
            supportAxis=sub(fixed,physicsMapValue(joints,structuralSideName(jsonLiteral("knee."),side),226));hasAxis=true;
        }else if(usage=="covering"){exclude("Fitted torso/neck covering; no automatic chest/body sway");continue;}
        else if(owner=="head"){
            bone=jsonLiteral("Head");if(rule.starts_with("hair")){fixed=physicsMapValue(joints,jsonLiteral("head_top"),231);profile=jsonLiteral("hair");}
            else if(rule=="ear"){fixed=physicsMapValue(joints,jsonLiteral("head_top"),233);profile=jsonLiteral("ear");supportAxis=sub(fixed,physicsMapValue(joints,jsonLiteral("neck_base"),234));hasAxis=true;
                auto upward=unit(supportAxis);Slice<double> projection;for(auto point:p)projection.append(dot(sub(point,fixed),upward));
                if(percentile(projection,.95)<=0){exclude("Human body/head side ear without a verified protruding free region; skeleton motion only");continue;}}
            else if(matches("bow|ribbon|tassel|frill"))fixed=physicsMapValue(joints,jsonLiteral("head_top"),240);else{exclude("Rigid fitted head covering");continue;}
        }else if(owner=="torso"){
            if(contains({"skirt","skirt_back","apron"},rule)){fixed=physicsMapValue(joints,jsonLiteral("waist"),243);profile=jsonLiteral("sheet");bone=jsonLiteral("Pelvis");}
            else if(rule=="tail"){fixed=physicsMapValue(joints,jsonLiteral("pelvis"),244);bone=jsonLiteral("Pelvis");}
            else if(contains({"chest_accessory","pelvis_accessory"},rule)&&matches("bow|ribbon|tassel|chain")){fixed=physicsMapValue(joints,rule=="chest_accessory"?jsonLiteral("neck_base"):jsonLiteral("waist"),246);bone=rule=="chest_accessory"?jsonLiteral("Chest"):jsonLiteral("Pelvis");}
            else {exclude("Rigid or unresolved torso accessory");continue;}
        }else{exclude("No anatomical support",true);continue;}
        if(matches("chain|tassel|ribbon|bow")){profile=jsonLiteral("hanging");pattern=jsonLiteral("one_end");}
        Slice<double> distances;for(auto point:p)distances.append(norm(sub(point,fixed)));auto root=section(p,distances,.05,false),direction=sub(mean(p),root);
        if(norm(direction)<1e-6){exclude("No free direction",true);continue;}direction=unit(direction);Slice<double> projection;
        for(auto point:p)projection.append(dot(sub(point,root),direction));auto free=section(p,projection,.95,true);
        if(hasAxis){direction=unit(supportAxis);projection={};for(auto point:p)projection.append(dot(point,direction));if(profile.view()!="ear")root=section(p,projection,.05,false);free=section(p,projection,.95,true);}
        if(pattern.view()=="two_ends"){direction=unit(sub(c,a));projection={};for(auto point:p)projection.append(dot(sub(point,a),direction));root=section(p,projection,.05,false);free=section(p,projection,.95,true);}
        auto spec=object({
            {jsonLiteral("id"),structuralLabel(at(m,"chart").as_string(),id,false)},
            {jsonLiteral("name"),structuralLabel(at(m,"chart").as_string(),id,true)},
            {jsonLiteral("target"),id},
            {jsonLiteral("parent_bone"),bone},
            {jsonLiteral("profile"),profile},
            {jsonLiteral("pattern"),pattern},
            {jsonLiteral("fixed"),point(root)},
            {jsonLiteral("free"),point(free)},
            {jsonLiteral("limb"),limb},
            {jsonLiteral("owner"),ownerText},
            {jsonLiteral("protected_pixels"),Array{}},
            {jsonLiteral("protection_margin"),1.}});
        if(!limb.is_null()){
            auto chain=ngRigPoints(at(limb,"joints"));if(auto tip=joints.find(structuralSideName(jsonLiteral("hand_tip."),side));tip!=joints.end()){chain.append(tip->second);set(limb,"joints",ngRigPointsJson(chain));}
            if(mixed){exclude("Combined body/cloth lacks resolved anatomical surface boundary; protect entire Part until structural region recognition is verified",true);continue;}
            set(spec,"protection_margin",dmax(1.,ngRigNumber(at(limb,"radius"))*.08));set(spec,"mixed_anatomy_cloth",false);
        }
        auto f=ngRigPhysicsFrame(root,free);set(spec,"length",f.length);Slice<Point2> sampled;auto stride=std::max<std::size_t>(1,p.size()/10000);
        for(std::size_t i=0;i<p.size();i+=stride)sampled.append(arrayAt(p,i,288));auto weights=ngRigPhysicsWeights(spec,sampled,policy);std::size_t moving=0;for(auto w:weights)moving+=w>1e-6;
        const auto freeFraction=static_cast<double>(moving)/weights.size();
        const auto minimumFreeFraction=ngRigNumber(at(policy,"minimum_free_fraction"));
        if(freeFraction<minimumFreeFraction){exclude("No supported free cloth outside protected anatomy/joints",true);continue;}
        if(matches("chain")){exclude("Chain/rigid pendant segmentation and multiple anchors not yet resolved",true);continue;}
        set(row,"decision","include");set(row,"reason","Anatomical support with local free region");set(row,"pattern",pattern);set(row,"profile",profile);
        set(spec,"targets",array({id}));candidates.append(spec);included=true;
    }
    for(auto id:ids)if(at(physicsMapValue(materials,id,300),"usage").as_string().view()=="decoration"){
        auto receiver=get(physicsMapValue(materials,id,301),"receiver");Slice<std::uint64_t> visited;
        while(!receiver.is_null()){
            auto receiverId=ngRigUnsigned(receiver);
            if(contains(visited,receiverId))break;
            visited.append(receiverId);
            auto material=materials.find(receiverId);if(material==materials.end()||at(material->second,"usage").as_string().view()!="decoration")break;
            auto next=get(material->second,"receiver");if(next.is_null())break;receiver=next;}
        std::size_t selected=SIZE_MAX,largest=0;
        for(std::size_t i=0;i<candidates.size();++i){
            auto spec=candidates[i];
            bool receiverMatches;
            if(!receiver.is_null())receiverMatches=receiver==at(spec,"target");
            else{
                const auto specChart=at(physicsMapValue(materials,ngRigUnsigned(at(spec,"target")),314),"chart").as_string();
                const auto materialChart=at(physicsMapValue(materials,id,314),"chart").as_string();
                receiverMatches=specChart.view()==materialChart.view()&&
                    contains({"sheet","hair","sleeve"},at(spec,"profile").as_string().view());
            }
            if(receiverMatches){
                const auto count=physicsMapValue(assets,ngRigUnsigned(at(spec,"target")),316).points.size();
                if(selected==SIZE_MAX||count>largest){selected=i;largest=count;}
            }
        }
        if(selected!=SIZE_MAX){physicsMapSlot(assets,id)=ngRigPhysicsAsset(physicsMapValue(materials,id,320),task);
            Object retainedTargets;
            arrayAt(candidates,selected,321).sourceIndexReference("targets",retainedTargets).as_array().append(id);
            auto row=arrayAt(inventory,physicsMapValue(inventoryIndex,id,322),322);set(row,"decision","carried");set(row,"reason","Primary observed receiver field");set(row,"carrier",at(arrayAt(candidates,selected,323),"id"));}
    }
    Slice<std::size_t> fronts;
    for(std::size_t i=0;i<candidates.size();++i){
        auto spec=candidates[i];
        if(contains({"skirt","apron"},at(physicsMapValue(materials,ngRigUnsigned(at(spec,"target")),327),"rule").as_string().view())&&
                at(spec,"profile").as_string().view()=="sheet")fronts.append(i);
    }
    if(!fronts.empty()){
        auto base=arrayAt(candidates,arrayAt(fronts,0,330),330);Slice<Point2> cloud;Slice<std::uint64_t> targets;
        for(auto i:fronts){cloud.appendRange(physicsMapValue(assets,ngRigUnsigned(at(arrayAt(candidates,i,332),"target")),332).points);for(auto id:at(arrayAt(candidates,i,333),"targets").sourceArrayValue()){
            auto n=ngRigUnsigned(id);
            if(!contains(targets,n))targets.append(n);
        }}
        Slice<double> y;for(auto p:cloud)y.append(p[1]);auto root=section(cloud,y,.05,false),free=section(cloud,y,.95,true);
        set(base,"fixed",point(root));set(base,"free",point(free));set(base,"name","Physics::WaistCloth");set(base,"id","waist-cloth");set(base,"length",ngRigPhysicsFrame(root,free).length);
        if(targets.size()>1)std::sort(targets.data(),targets.data()+targets.size());
        auto sortedTargets=materializeIds(targets);
        set(base,"targets",array(sortedTargets));Array merged;
        for(std::size_t i=0;i<candidates.size();++i)
            if(i==arrayAt(fronts,0,341)||!contains(fronts,i))merged.append(candidates[i]);
        candidates=merged;
    }
    for(auto child:candidates){auto childId=ngRigUnsigned(at(child,"target"));
        const auto childNameText=io::imageexport::detail::lowerExtension(
            compat::RetainedStringSlice{at(physicsMapValue(materials,childId,345),"name").as_string()});
        const auto childName=childNameText.view();
        if(!physicsRegexSearch(childNameText,jsonLiteral("bow|tassel|ribbon")))continue;double best=INFINITY;Json selected;
        for(auto host:candidates){
            const auto hostGroupId=at(host,"id").as_string();
            const auto childGroupId=at(child,"id").as_string();
            if(hostGroupId.view()==childGroupId.view())continue;
            const auto hostOwner=at(host,"owner").as_string();
            const auto childOwner=at(child,"owner").as_string();
            if(hostOwner.view()!=childOwner.view())continue;
            const auto hostId=ngRigUnsigned(at(host,"target"));
            const auto hostNameText=io::imageexport::detail::lowerExtension(
                compat::RetainedStringSlice{at(physicsMapValue(materials,hostId,350),"name").as_string()});
            const auto hostName=hostNameText.view();
            bool compatible;
            if(at(child,"owner").as_string().view()=="head"){
                compatible=childName.find("tassel")!=childName.npos&&hostName.find("bow")!=hostName.npos&&
                    at(physicsMapValue(materials,hostId,352),"rule").as_string().view()=="headwear";
            }else{
                compatible=contains({"sheet","sleeve","frill"},at(host,"profile").as_string().view());
                if(compatible){
                    const auto hostChart=at(physicsMapValue(materials,hostId,353),"chart").as_string();
                    const auto childChart=at(physicsMapValue(materials,childId,353),"chart").as_string();
                    compatible=hostChart.view()==childChart.view();
                }
            }
            if(!compatible)continue;
            const auto index=structuralIndex(assets,hostId);
            const auto distance=index->distance(ngRigPoint(at(child,"fixed")));
            const auto hostLength=ngRigNumber(at(host,"length"));
            const auto attachmentFraction=ngRigNumber(at(policy,"attachment_distance_fraction"));
            const auto hostThreshold=hostLength*attachmentFraction;
            const auto childThreshold=ngRigNumber(at(child,"length"))*.15;
            if(distance>dmax(hostThreshold,childThreshold))continue;
            bool better=distance<best;
            if(!better&&distance==best){
                const auto hostIdText=at(host,"id").as_string();
                const auto selectedIdText=at(selected,"id").as_string();
                better=hostIdText.view()<selectedIdText.view();
            }
            if(better){best=distance;selected=host;}
        }
        if(!selected.is_null()){set(child,"support_group",at(selected,"id"));set(child,"support_anchor",at(child,"fixed"));set(arrayAt(inventory,physicsMapValue(inventoryIndex,childId,363),363),"support_group",at(selected,"id"));}
    }
    return object({
        {jsonLiteral("schema_version"),Json(jsonLiteral("rig-physics-structure-d/1"))},
        {jsonLiteral("inventory"),inventory},
        {jsonLiteral("groups"),candidates},
        {jsonLiteral("policy_sha256"),core::settings::SettingsString(ngRigDigest(policy))}});
}
}
