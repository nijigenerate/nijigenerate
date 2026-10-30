#include "detail/evidence.hpp"
namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;
void ngRigPropagateClippingVisibility(JsonArray& materials) {
    UnsignedEvidenceMap<std::uint64_t,std::size_t> indices;
    {
        const auto indexed=materials;
        for(std::size_t i=0;i<indexed.size();++i) {
            auto material=indexed[i];
            indices[ngRigUnsigned(atEvidence(material,"uuid"))]=i;
        }
    }
    bool changed;
    do {changed=false;for(auto active=materials;auto& m:active)if(atEvidence(m,"active").as_bool())if(auto r=evidenceObject(m).if_contains("receiver")) {
        auto i=indices.find(ngRigUnsigned(*r));if(i!=indices.end()&&!atEvidence(materials[i->second],"active").as_bool()){setEvidence(m,"active",false);changed=true;}
    }}while(changed);
}
void ngRigResolveGroupReceivers(JsonArray& materials,JsonArray groups,bool classified) {
    UnsignedEvidenceMap<std::uint64_t,bool> materialIds,groupIds;
    for(const auto initial=materials;auto m:initial)materialIds[ngRigUnsigned(atEvidence(m,"uuid"))]=true;
    for(auto g:groups)groupIds[ngRigUnsigned(atEvidence(g,"uuid"))]=true;
    for(auto current=materials;auto& m:current) {
        auto receiver=evidenceObject(m).if_contains("receiver");if(!receiver)continue;
        auto original=getEvidence(m,"mask_receiver",*receiver);auto id=ngRigUnsigned(original);
        if(materialIds.contains(id)||!groupIds.contains(id))continue;
        Json selected;double order=std::numeric_limits<double>::infinity();
        for(const auto candidates=materials;auto c:candidates) {
            if(!atEvidence(c,"active").as_bool())continue;if(classified&&getEvidence(c,"static",false).as_bool())continue;
            auto ancestors=evidenceObject(c).if_contains("ancestor_ids");if(!ancestors)continue;bool belongs=false;
            for(auto a:ancestors->sourceArrayValue())if(ngRigUnsigned(a)==id)belongs=true;
            auto candidateOrder=ngRigNumber(atEvidence(c,"source_order"));if(belongs&&candidateOrder<order){selected=atEvidence(c,"uuid");order=candidateOrder;}
        }
        setEvidence(m,"mask_receiver",original);
        if(order==std::numeric_limits<double>::infinity()){if(classified)setEvidence(m,"static",true);else setEvidence(m,"active",false);}
        else setEvidence(m,"receiver",selected);
    }
}
void ngRigPropagateClippingStatic(JsonArray& materials,JsonArray groups) {
    UnsignedEvidenceMap<std::uint64_t,std::size_t> indices;
    {
        const auto indexed=materials;
        for(std::size_t i=0;i<indexed.size();++i) {
            auto material=indexed[i];
            indices[ngRigUnsigned(atEvidence(material,"uuid"))]=i;
        }
    }
    bool changed;
    do {
        compat::Slice<bool> previous;for(const auto initial=materials;auto m:initial)previous.append(getEvidence(m,"static",false).as_bool());
        for(auto current=materials;auto& m:current) {auto r=evidenceObject(m).if_contains("receiver");if(!r)continue;auto original=getEvidence(m,"mask_receiver",*r);
            auto index=indices.find(ngRigUnsigned(original));if(index!=indices.end()&&getEvidence(materials[index->second],"static",false).as_bool())setEvidence(m,"static",true);}
        ngRigResolveGroupReceivers(materials,groups,true);changed=false;
        {
            const auto current=materials;
            for(std::size_t i=0;i<current.size();++i) {
                auto material=current[i];
                if(previous[i]!=getEvidence(material,"static",false).as_bool())changed=true;
            }
        }
    }while(changed);
}
Json ngRigClassifyMaterials(Json observation,Json options,std::shared_ptr<framework::AutoRigTaskContext> context) {
    auto specification=templates::ngRigMaterialRoles(),overrides=getEvidence(options,"materials",SettingsJsonObject{});
    JsonArray materials;compat::Slice<bool> explicitRole;
    for(const auto originals=atEvidence(observation,"materials").sourceArrayValue();auto original:originals) {
        ngRigCheckpoint(context);auto record=duplicateEvidence(original);Strings ancestors;
        if(auto p=evidenceObject(original).if_contains("ancestors"))for(auto parent:p->sourceArrayValue())ancestors.append(parent.as_string());
        else {
            auto path = atEvidence(original,"path").as_string();
            std::size_t start = 0;
            for (;;) {
                const auto end = path.view().find('/', start);
                if (end == std::string_view::npos) break;
                ancestors.append(path.slice(start, end));
                start = end + 1;
            }
        }
        auto role=semanticRole(atEvidence(original,"name").as_string(),ancestors);
        auto feature=ngRigMaterialFeature(atEvidence(original,"name").as_string(),ancestors);
        if(role.view()=="pelvis_accessory")feature=compat::RetainedString::fromStaticLiteral("");
        auto overrideRecord=evidenceObject(overrides).if_contains(atEvidence(original,"path").as_string().view());
        if(overrideRecord) {
            feature=ngRigString(*overrideRecord,stringLiteralEvidence("feature"),feature);
            const std::array<std::string_view,16> valid{"","mouth","mouth_tongue","mouth_upper_teeth","mouth_lower_teeth","mouth_outline","mouth_upper_lip","mouth_lower_lip","brow","sclera","iris","corner","upper","lower","fold","nose"};
            enforceEvidenceLazy(std::find(valid.begin(),valid.end(),feature.view())!=valid.end(),[&]{return "Unknown material feature override: "+std::string(feature.view());},294);
            if(auto side=evidenceObject(*overrideRecord).if_contains("side")){auto s=(*side).as_string().view();enforceEvidence(s.empty()||s=="L"||s=="R","Unknown model side override",299);setEvidence(record,"side_override",*side);}
        }
        bool overridden=overrideRecord&&evidenceObject(*overrideRecord).if_contains("role");
        if(overridden){role=atEvidence(*overrideRecord,"role").as_string();bool valid=false;for(auto rule:atEvidence(specification,"rules").sourceArrayValue())if(atEvidence(rule,"id").as_string().view()==role.view())valid=true;enforceEvidenceLazy(valid,[&]{return "Unknown material role override: "+std::string(role.view());},308);}
        bool active=atEvidence(original,"active").as_bool(),stationary=overrideRecord&&getEvidence(*overrideRecord,"static",false).as_bool();
        setEvidence(record,"static",stationary||!active||role.view()=="background");setEvidence(record,"role",core::settings::SettingsString(role));setEvidence(record,"feature",core::settings::SettingsString(feature));
        setEvidence(record,"semantic_source",overridden?literalEvidence("explicit_override"):literalEvidence("model_name_and_ancestry_candidate"));
        const auto lowerName = io::imageexport::detail::lowerExtension(atEvidence(original,"name").as_string());
        std::string lower(lowerName.view());
        std::replace(lower.begin(),lower.end(),'-','_');compat::RetainedStringSlice side;
        for(auto token:split(lower,'_')){if(token=="l"||token=="left")side=compat::RetainedString::fromStaticLiteral("L");if(token=="r"||token=="right")side=compat::RetainedString::fromStaticLiteral("R");}
        setEvidence(record,"side_hint",core::settings::SettingsString(overrideRecord?ngRigString(*overrideRecord,stringLiteralEvidence("side"),side):side));materials.append(record);explicitRole.append(overridden);
    }
    compat::Slice<Point2> faceSupport,footSupport;
    for(auto m:materials)if(!atEvidence(m,"static").as_bool()){if(atEvidence(m,"role").as_string().view()=="face")faceSupport.append(ngRigPoints(atEvidence(m,"cloud")));if(atEvidence(m,"role").as_string().view()=="foot")footSupport.append(ngRigPoints(atEvidence(m,"cloud")));}
    auto supportBounds=[](compat::Slice<Point2> points){const double inf=std::numeric_limits<double>::infinity();std::array<double,4>b{inf,inf,-inf,-inf};for(auto p:points)for(std::size_t i=0;i<2;++i){b[i]=dmin(b[i],p[i]);b[i+2]=dmax(b[i+2],p[i]);}return b;};
    if(!faceSupport.empty()&&!footSupport.empty()) {
        auto fb=supportBounds(faceSupport),feet=supportBounds(footSupport);compat::Slice<double> footY;for(auto p:footSupport)footY.append(p[1]);
        for(auto& m:materials)if(!atEvidence(m,"static").as_bool()&&atEvidence(m,"role").as_string().view()=="ear") {auto b=supportBounds(ngRigPoints(atEvidence(m,"cloud")));
            if(b[2]-b[0]>4*(fb[2]-fb[0])&&b[3]-b[1]>.85*(feet[3]-fb[1])&&b[3]>=quantile(footY,.5)&&ngRigNumber(getEvidence(m,"opaque_perimeter_coverage",0.))>=.65){setEvidence(m,"static",true);setEvidence(m,"semantic_source",literalEvidence("full_body_backdrop_alpha_perimeter"));}}
    }
    ngRigPropagateClippingStatic(materials,getEvidence(observation,"groups",JsonArray{}).sourceArrayValue());
    // D leaves out-of-range floating conversions unspecified. Avoid C++ UB there.
    auto id32=[](Json v){auto x=ngRigNumber(v);return x>=0&&x<4294967296.?static_cast<std::uint32_t>(x):UINT32_MAX;};
    UnsignedEvidenceMap<std::uint32_t,std::size_t> byId;for(std::size_t i=0;i<materials.size();++i)byId[id32(atEvidence(materials[i],"uuid"))]=i;
    compat::Slice<bool> visiting,resolved;visiting.resize(materials.size());resolved.resize(materials.size());
    const auto inherit=[&](auto&& self,std::size_t i) -> void {
        if(resolved[i]||visiting[i])return;visiting[i]=true;
        if(!explicitRole[i])if(auto receiver=evidenceObject(materials[i]).if_contains("receiver"))if(auto target=byId.find(id32(*receiver));target!=byId.end()) {
            self(self,target->second);auto role=atEvidence(materials[target->second],"role").as_string();
            if(!role.empty()&&role.view()!="background") {setEvidence(materials[i],"role",role);auto featureOverride=evidenceObject(overrides).if_contains(atEvidence(materials[i],"path").as_string().view());
                bool explicitFeature=featureOverride&&evidenceObject(*featureOverride).if_contains("feature");
                if(!explicitFeature&&atEvidence(materials[i],"feature").as_string().empty())setEvidence(materials[i],"feature",atEvidence(materials[target->second],"feature"));
                setEvidence(materials[i],"semantic_source",literalEvidence("imported_clipping_receiver"));}
        }
        visiting[i]=false;resolved[i]=true;
    };
    for(std::size_t i=0;i<materials.size();++i)inherit(inherit,i);
    compat::Slice<std::size_t> known;
    compat::Slice<std::shared_ptr<AlphaSupportIndex>> indices;
    for(std::size_t i=0;i<materials.size();++i)if(!atEvidence(materials[i],"static").as_bool()&&!atEvidence(materials[i],"role").as_string().view().empty()) {auto cloud=ngRigPoints(atEvidence(materials[i],"cloud"));if(!cloud.empty()){known.append(i);indices.append(allocateAlphaSupportIndex(cloud));}}
    for(std::size_t i=0;i<materials.size();++i) {
        if(atEvidence(materials[i],"static").as_bool()||!atEvidence(materials[i],"role").as_string().view().empty())continue;ngRigCheckpoint(context);
        if(known.empty()){setEvidence(materials[i],"role",literalEvidence("local"));setEvidence(materials[i],"semantic_source",literalEvidence("local_mechanism_without_anatomy"));continue;}
        auto cloud=ngRigPoints(atEvidence(materials[i],"cloud"));compat::Slice<double> ys;for(auto p:cloud)ys.append(p[1]);auto threshold=quantile(ys,.05);
        compat::Slice<Point2> roots;for(auto p:cloud)if(p[1]<=threshold)roots.append(p);
        double best=std::numeric_limits<double>::infinity();std::size_t support=0;JsonArray ranked;
        for(std::size_t j=0;j<known.size();++j) {auto candidate=known[j];ngRigCheckpoint(context);compat::Slice<double> distances;
            for(auto point:roots)distances.append(indices[j]->distance(point));auto distance=quantile(distances,.5);
            ranked.append(objectEvidence({{stringLiteralEvidence("part"),atEvidence(materials[candidate],"uuid")},{stringLiteralEvidence("distance"),distance}}));
            if(distance<best||(distance==best&&atEvidence(materials[candidate],"path").as_string().view()<atEvidence(materials[support],"path").as_string().view())){best=distance;support=candidate;}
        }
        sort(ranked,[](Json a,Json b){return ngRigNumber(atEvidence(a,"distance"))<ngRigNumber(atEvidence(b,"distance"));});
        setEvidence(materials[i],"role",atEvidence(materials[support],"role"));setEvidence(materials[i],"support_part",atEvidence(materials[support],"uuid"));
        setEvidence(materials[i],"support_candidates",ranked.slice(0,std::min(std::size_t{4},ranked.size())));setEvidence(materials[i],"semantic_source",literalEvidence("alpha_proximal_support_candidate"));
    }
    for(auto& m:materials)if(!atEvidence(m,"static").as_bool()) {
        if(atEvidence(m,"role").as_string().view()=="local"){setEvidence(m,"owner",literalEvidence("local"));setEvidence(m,"chart",literalEvidence("local"));continue;}
        for(auto rule:atEvidence(specification,"rules").sourceArrayValue())if(atEvidence(rule,"id").as_string().view()==atEvidence(m,"role").as_string().view()){setEvidence(m,"owner",atEvidence(rule,"owner"));setEvidence(m,"chart",atEvidence(rule,"chart"));break;}
        enforceEvidence(evidenceObject(m).if_contains("owner"),"Unsupported automatic material role",422);
    }
    auto result=duplicateEvidence(observation);setEvidence(result,"materials",materials);setEvidence(result,"options",options);return result;
}
}
