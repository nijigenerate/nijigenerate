#include "detail/evidence.hpp"
#include <numeric>
#include <set>
namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;
namespace {
// Resolve long opposing leg surfaces before proximity can attach them to
// the clothing that meets their upper edge.
void classifyLegGeometry(JsonArray& materials, Json overrides, const std::array<double,4>& face) {
    const double width=face[2]-face[0], height=face[3]-face[1];
    if (width<=0. || height<=0.) return;
    const double center=std::midpoint(face[0],face[2]);
    struct Leg {
        std::size_t index;
        double top,bottom,span,solid;
    };
    std::array<bool,2> hasLeg{};
    std::array<std::vector<Leg>,2> candidates;
    for (std::size_t i=0;i<materials.size();++i) {
        auto material=materials[i];
        if (atEvidence(material,"static").as_bool()) continue;
        const auto cloud=ngRigPoints(atEvidence(material,"cloud"));
        if (cloud.empty()) continue;
        std::size_t rightCount=0;
        double minX=INFINITY,minY=INFINITY,maxX=-INFINITY,maxY=-INFINITY;
        for (const auto p : cloud) {
            rightCount+=p[0]<center;
            minX=std::min(minX,p[0]); maxX=std::max(maxX,p[0]);
            minY=std::min(minY,p[1]); maxY=std::max(maxY,p[1]);
        }
        const double rightFraction=double(rightCount)/cloud.size();
        const auto role=atEvidence(material,"role").as_string().view();
        if (role=="leg") {
            hasLeg[0]|=rightFraction>.15;
            hasLeg[1]|=rightFraction<.85;
        }
        const auto* manual=material.sourceObjectRef().if_contains("receiver");
        auto* override=evidenceObject(overrides).if_contains(atEvidence(material,"path").as_string().view());
        if ((role!="" && role!="leg") || manual ||
            (override && evidenceObject(*override).if_contains("role"))) continue;
        const double top=(minY-face[1])/height, bottom=(maxY-face[1])/height;
        const double span=(maxX-minX)/width;
        if (top<1.25 || top>3.65 || bottom-top<1.35 || span<.20 || span>1.05 ||
            (rightFraction>.16 && rightFraction<.84)) continue;
        const auto dimensions=ngRigNumbers(getEvidence(material,"texture_size",JsonArray{}));
        if (dimensions.size()!=2 || dimensions[0]<=0. || dimensions[1]<=0.) continue;
        const auto runs=getEvidence(material,"alpha_runs_128",JsonArray{}).sourceArrayValue();
        double area=0.;
        for (std::size_t j=1;j<runs.size();j+=2) area+=ngRigNumber(runs[j]);
        const double solid=area/(dimensions[0]*dimensions[1]);
        if (solid<.17) continue;
        candidates[rightFraction>.5 ? 0 : 1].push_back({i,top,bottom,span,solid});
    }
    if (hasLeg[0] && hasLeg[1]) return;
    const Leg* selectedRight=nullptr;
    const Leg* selectedLeft=nullptr;
    double best=-INFINITY;
    for (const auto& right : candidates[0]) for (const auto& left : candidates[1]) {
        if (std::abs(right.top-left.top)>.48 || std::abs(right.bottom-left.bottom)>.55 ||
            std::abs(right.span-left.span)>.42) continue;
        const double score=(right.bottom-right.top)+(left.bottom-left.top)+right.solid+left.solid-
            2.*std::abs(right.top-left.top)-std::abs(right.bottom-left.bottom);
        if (score>best) { best=score; selectedRight=&right; selectedLeft=&left; }
    }
    if (!selectedRight) return;
    const std::array selected{selectedRight,selectedLeft};
    for (std::size_t side=0;side<hasLeg.size();++side) {
        if (hasLeg[side]) continue;
        auto& leg=materials[selected[side]->index];
        setEvidence(leg,"role",literalEvidence("leg"));
        setEvidence(leg,"semantic_source",literalEvidence("bilateral_leg_alpha_support"));
    }
}

void classifyFacialGeometry(JsonArray& materials, Json overrides, const std::array<double,4>& face) {
    struct Surface {
        std::size_t index;
        std::uint64_t id, receiver;
        double x, y, width, height, pale;
        std::size_t clippedParts = 0;
    };
    const double width = face[2]-face[0], height = face[3]-face[1];
    if (width <= 0. || height <= 0.) return;
    std::vector<Surface> surfaces;
    for (std::size_t i = 0; i < materials.size(); ++i) {
        auto material = materials[i];
        if (atEvidence(material,"static").as_bool() ||
            !evidenceObject(material).if_contains("pale_alpha_fraction")) continue;
        auto* override = evidenceObject(overrides).if_contains(atEvidence(material,"path").as_string().view());
        if (override && (evidenceObject(*override).if_contains("role") ||
                         evidenceObject(*override).if_contains("feature"))) continue;
        auto cloud = ngRigPoints(atEvidence(material,"cloud"));
        if (cloud.empty()) continue;
        auto left = cloud[0][0], right = left, top = cloud[0][1], bottom = top;
        for (const auto& point : cloud) {
            left = std::min(left,point[0]); right = std::max(right,point[0]);
            top = std::min(top,point[1]); bottom = std::max(bottom,point[1]);
        }
        const auto x = ((left+right)*.5-face[0])/width, y = ((top+bottom)*.5-face[1])/height;
        const auto w = (right-left)/width, h = (bottom-top)/height;
        if (x < .06 || x > .94 || y < .24 || y > .98 || w < .004 || w > .46 || h < .004 || h > .30) continue;
        surfaces.push_back({i,ngRigUnsigned(atEvidence(material,"uuid")),
            ngRigUnsigned(getEvidence(material,"receiver",0)),x,y,w,h,
            ngRigNumber(atEvidence(material,"pale_alpha_fraction"))});
    }
    for (auto& parent : surfaces)
        for (const auto& child : surfaces)
            parent.clippedParts += child.receiver == parent.id;
    auto assign = [&](const Surface& surface, auto feature, auto reason) {
        auto& material = materials[surface.index];
        setEvidence(material,"role",literalEvidence("face_feature"));
        setEvidence(material,"feature",feature);
        setEvidence(material,"semantic_source",reason);
    };
    const Surface* rightEye = nullptr;
    const Surface* leftEye = nullptr;
    double bestPair = -std::numeric_limits<double>::infinity();
    const auto aperture = [](const Surface& eye) {
        return eye.width >= .10 && eye.height >= .035 && eye.height <= .24 && eye.y <= .75 &&
            (eye.pale >= .60 || eye.clippedParts > 0);
    };
    for (const auto& right : surfaces) {
        if (!aperture(right) || right.x >= .44) continue;
        for (const auto& left : surfaces) {
            if (!aperture(left) || left.x <= .56 || std::abs(left.y-right.y) > .07 ||
                std::abs(left.width-right.width) > .10 || std::abs(left.height-right.height) > .08) continue;
            const double score = left.pale+right.pale+.23*(bool(left.clippedParts)+bool(right.clippedParts)) -
                3.*std::abs(left.y-right.y)-2.*std::abs(left.width-right.width)-
                2.*std::abs(left.x+right.x-1.)-.7*std::abs((left.x-right.x)-.49);
            if (score > bestPair) { bestPair = score; rightEye = &right; leftEye = &left; }
        }
    }
    if (!rightEye) return;
    assign(*rightEye,literalEvidence("sclera"),literalEvidence("bilateral_eye_aperture"));
    assign(*leftEye,literalEvidence("sclera"),literalEvidence("bilateral_eye_aperture"));
    for (const auto* eye : {rightEye,leftEye}) {
        for (const auto& candidate : surfaces) {
            if (candidate.id == eye->id) continue;
            const double dx = std::abs(candidate.x-eye->x), dy = candidate.y-eye->y;
            if (candidate.receiver == eye->id ||
                (dx < .22*eye->width && std::abs(dy) < .4*eye->height &&
                 candidate.width > .22*eye->width && candidate.width < .85*eye->width &&
                 candidate.height > .42*eye->height && candidate.pale < .35 && candidate.width/candidate.height < 1.8))
                assign(candidate,literalEvidence("iris"),literalEvidence("eye_aperture_contents"));
        }
    }
    const double center = (rightEye->x+leftEye->x)*.5, eyeRow = (rightEye->y+leftEye->y)*.5;
    const Surface* mouth = nullptr;
    double bestMouth = -std::numeric_limits<double>::infinity();
    for (const auto& candidate : surfaces) {
        if (std::abs(candidate.x-center) > .10 || candidate.y < std::max(.65,eyeRow+.13) ||
            candidate.y > .96 || candidate.width < .055 || candidate.width > .40 ||
            candidate.height > .18 || candidate.width < 1.3*candidate.height) continue;
        const double score = .8*bool(candidate.clippedParts) + candidate.width -
            4.*std::abs(candidate.x-center) - std::abs(candidate.y-.84) - .25*candidate.pale;
        if (score > bestMouth) { bestMouth = score; mouth = &candidate; }
    }
    if (!mouth) return;
    assign(*mouth,literalEvidence("mouth"),literalEvidence("central_lower_face_aperture"));
    for (const auto& candidate : surfaces) {
        if (candidate.receiver != mouth->id) continue;
        const auto oldFeature = atEvidence(materials[candidate.index],"feature").as_string().view();
        if (oldFeature.starts_with("mouth_")) continue;
        assign(candidate,candidate.pale > .65 ?
            (candidate.y < mouth->y ? literalEvidence("mouth_upper_teeth") : literalEvidence("mouth_lower_teeth")) :
            literalEvidence("mouth_tongue"), literalEvidence("mouth_aperture_contents"));
    }
    const Surface* nose = nullptr;
    double bestNose = -std::numeric_limits<double>::infinity();
    for (const auto& candidate : surfaces) {
        if (std::abs(candidate.x-center) > .10 || candidate.y < eyeRow+.06 ||
            candidate.y > mouth->y-.055 || candidate.width > .19 || candidate.height > .13 ||
            candidate.receiver || candidate.clippedParts) continue;
        const double score = -4.*std::abs(candidate.x-center) -
            1.5*std::abs(candidate.y-std::lerp(eyeRow,mouth->y,.60));
        if (score > bestNose) { bestNose = score; nose = &candidate; }
    }
    if (nose) assign(*nose,literalEvidence("nose"),literalEvidence("central_feature_between_eyes_and_mouth"));
}
}
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
    std::string phase = "material classification inputs";
    try {
    auto specification=templates::ngRigMaterialRoles(),overrides=getEvidence(options,"materials",SettingsJsonObject{});
    JsonArray materials;compat::Slice<bool> explicitRole;
    for(const auto originals=atEvidence(observation,"materials").sourceArrayValue();auto original:originals) {
        ngRigCheckpoint(context);auto record=duplicateEvidence(original);Strings ancestors;
        phase = "material ancestry: " + std::string(atEvidence(original,"path").as_string().view());
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
    phase = "face and foot support";
    for(auto m:materials)if(!atEvidence(m,"static").as_bool()){if(atEvidence(m,"role").as_string().view()=="face")faceSupport.append(ngRigPoints(atEvidence(m,"cloud")));if(atEvidence(m,"role").as_string().view()=="foot")footSupport.append(ngRigPoints(atEvidence(m,"cloud")));}
    auto supportBounds=[](compat::Slice<Point2> points){const double inf=std::numeric_limits<double>::infinity();std::array<double,4>b{inf,inf,-inf,-inf};for(auto p:points)for(std::size_t i=0;i<2;++i){b[i]=dmin(b[i],p[i]);b[i+2]=dmax(b[i+2],p[i]);}return b;};
    // Assess complete alpha support in an anatomical frame before proximal inheritance.
    // Names propose anchors; spatial evidence decides whether unlabelled surfaces belong
    // to the head or trunk. Explicit user assignments always take precedence.
    compat::Slice<Point2> torsoSupport;
    for(auto m:materials)if(!atEvidence(m,"static").as_bool()&&
        atEvidence(m,"role").as_string().view()=="torso")torsoSupport.append(ngRigPoints(atEvidence(m,"cloud")));
    const auto fractionInside=[](compat::Slice<Point2> cloud,const std::array<double,4>& box) {
        double count=0;
        for(auto p:cloud)if(p[0]>=box[0]&&p[0]<=box[2]&&p[1]>=box[1]&&p[1]<=box[3])++count;
        return cloud.empty()?0.:count/cloud.size();
    };
    // A disc clipped by a wider eye aperture is an iris, even when its name says
    // "eyeball". The mask source, not that ambiguous name, defines the eye corners.
    if(!faceSupport.empty()) {
        const auto face=supportBounds(faceSupport);const double fw=face[2]-face[0],fh=face[3]-face[1];
        classifyLegGeometry(materials, overrides, face);
        classifyFacialGeometry(materials, overrides, face);
        if (fw > 0. && fh > 0.) {
            // A long, narrow surface descending from shoulder height on each
            // side can supply an arm when neither arm was classified upstream.
            std::array<bool,2> hasArm{};
            std::array<std::size_t,2> armCandidate{materials.size(),materials.size()};
            std::array<double,2> bestArm{};
            std::array<std::vector<Point2>,2> distalAnchors;
            const double center=std::midpoint(face[0],face[2]);
            for (auto material : materials) {
                if (atEvidence(material,"static").as_bool() || atEvidence(material,"role").as_string().view()!="hand")
                    continue;
                const auto cloud=ngRigPoints(atEvidence(material,"cloud"));
                if (cloud.empty()) continue;
                const auto b=supportBounds(cloud);
                if (b[2]-b[0]<.16*fw || b[2]-b[0]>.78*fw ||
                    b[3]-b[1]<.15*fh || b[3]-b[1]>.60*fh) continue;
                const Point2 point{std::midpoint(b[0],b[2]),std::midpoint(b[1],b[3])};
                distalAnchors[point[0]<center ? 0 : 1].push_back(point);
            }
            for (std::size_t i=0; i<materials.size(); ++i) {
                auto material=materials[i];
                if (atEvidence(material,"static").as_bool()) continue;
                const auto role=atEvidence(material,"role").as_string().view();
                const auto cloud=ngRigPoints(atEvidence(material,"cloud"));
                if (cloud.empty()) continue;
                const auto b=supportBounds(cloud);
                const auto middle=std::midpoint(b[0],b[2]);
                const std::size_t side=middle<center ? 0 : 1;
                if (role=="arm") hasArm[side]=true;
                if (explicitRole[i] || (!role.empty() && role!="chest_accessory") ||
                    evidenceObject(material).if_contains("receiver")) continue;
                const double h=b[3]-b[1], w=b[2]-b[0];
                if (b[1]<face[1]+.74*fh || b[1]>face[1]+1.75*fh || h<1.12*fh || h>2.9*fh ||
                    w<.19*fw || w>.98*fw || w>.48*h ||
                    (side==0 ? b[2]>face[0]+.24*fw : b[0]<face[2]-.24*fw)) continue;
                double score=h/w;
                if (!distalAnchors[side].empty()) {
                    Point2 end{0.,0.};
                    std::size_t count=0;
                    for (const auto point : cloud) if (point[1]>b[3]-.12*h) {
                        end[0]+=point[0]; end[1]+=point[1]; ++count;
                    }
                    for (auto& value : end) value/=count;
                    double distance=INFINITY;
                    for (const auto hand : distalAnchors[side])
                        distance=std::min(distance,std::hypot((hand[0]-end[0])/fw,(hand[1]-end[1])/fh));
                    score-=4.*distance;
                }
                if (score>bestArm[side]) { bestArm[side]=score; armCandidate[side]=i; }
            }
            if (armCandidate[0]!=materials.size() && armCandidate[1]!=materials.size())
                for (std::size_t side=0; side<hasArm.size(); ++side) {
                    if (hasArm[side]) continue;
                    auto& arm=materials[armCandidate[side]];
                    setEvidence(arm,"role",literalEvidence("arm"));
                    setEvidence(arm,"semantic_source",literalEvidence("bilateral_arm_alpha_support"));
                }
            // A decorated garment or prop can carry its own eyes and mouth.
            // Candidate labels cannot make artwork outside the head define its landmarks.
            const std::array<double, 4> facialRegion{
                face[0] - .15 * fw, face[1] - .15 * fh,
                face[2] + .15 * fw, face[3] + .15 * fh};
            for (std::size_t i = 0; i < materials.size(); ++i) {
                auto& material = materials[i];
                if (explicitRole[i] || atEvidence(material, "static").as_bool()) continue;
                const auto role = atEvidence(material, "role").as_string().view();
                const auto feature = atEvidence(material, "feature").as_string().view();
                if (role != "face_feature" && feature.empty()) continue;
                auto* overrideRecord = evidenceObject(overrides).if_contains(
                    atEvidence(material, "path").as_string().view());
                if (overrideRecord && evidenceObject(*overrideRecord).if_contains("feature")) continue;
                const auto cloud = ngRigPoints(atEvidence(material, "cloud"));
                if (fractionInside(cloud, facialRegion) >= .5) continue;
                setEvidence(material, "feature", literalEvidence(""));
                if (role == "face_feature") setEvidence(material, "role", literalEvidence(""));
            }
        }
        for(std::size_t i=0;i<materials.size();++i) {
            auto& iris=materials[i];const auto feature=atEvidence(iris,"feature").as_string().view();
            auto receiver=evidenceObject(iris).if_contains("receiver");
            if(!receiver||(feature!="iris"&&feature!="sclera")||atEvidence(iris,"static").as_bool())continue;
            auto override=evidenceObject(overrides).if_contains(atEvidence(iris,"path").as_string().view());
            if(explicitRole[i]||(override&&evidenceObject(*override).if_contains("feature")))continue;
            for(std::size_t j=0;j<materials.size();++j)if(atEvidence(materials[j],"uuid")==*receiver) {
                auto& base=materials[j];auto baseOverride=evidenceObject(overrides).if_contains(atEvidence(base,"path").as_string().view());
                const auto baseFeature=atEvidence(base,"feature").as_string().view();
                if(explicitRole[j]||(baseOverride&&evidenceObject(*baseOverride).if_contains("feature"))||
                    (!baseFeature.empty()&&baseFeature!="sclera")||atEvidence(base,"static").as_bool())break;
                const auto a=supportBounds(ngRigPoints(atEvidence(iris,"cloud")));
                const auto b=supportBounds(ngRigPoints(atEvidence(base,"cloud")));
                const double aw=a[2]-a[0],ah=a[3]-a[1],bw=b[2]-b[0],bh=b[3]-b[1];
                if(bw<=0||bh<=0||bw>.45*fw||bh>.4*fh||aw<.2*bw||aw>1.1*bw||ah>2*bh||
                    b[0]<face[0]||b[2]>face[2]||b[1]<face[1]||b[3]>face[3]||
                    (a[0]+a[2])*.5<b[0]||(a[0]+a[2])*.5>b[2])break;
                setEvidence(base,"role",literalEvidence("face_feature"));setEvidence(base,"feature",literalEvidence("sclera"));
                setEvidence(base,"semantic_source",literalEvidence("eye_aperture_clipping_base"));
                setEvidence(iris,"feature",literalEvidence("iris"));
                setEvidence(iris,"semantic_source",literalEvidence("iris_clipped_to_eye_aperture"));
                break;
            }
        }
    }
    for(std::size_t i=0;i<materials.size();++i) {
        auto& m=materials[i];if(explicitRole[i]||atEvidence(m,"static").as_bool())continue;
        const auto role=atEvidence(m,"role").as_string().view();
        const auto cloud=ngRigPoints(atEvidence(m,"cloud"));if(cloud.empty())continue;
        const auto b=supportBounds(cloud);const double width=b[2]-b[0],height=b[3]-b[1];
        if(width<=0||height<=0)continue;
        // Compact support enclosing the face is a head surface, not a trunk surface.
        // Long hair, isolated facial details and small accessories fail these bounds.
        if(role.empty()&&!faceSupport.empty()) {
            const auto face=supportBounds(faceSupport);
            const double fw=face[2]-face[0],fh=face[3]-face[1];
            const double covered=fractionInside(faceSupport,b);
            const std::array<double,4> headBox{face[0]-.4*fw,face[1]-.6*fh,
                face[2]+.4*fw,face[3]+.35*fh};
            if(fw>0&&fh>0&&covered>.8&&fractionInside(cloud,headBox)>.95&&
                width>.8*fw&&width<1.8*fw&&height>.8*fh&&height<1.8*fh&&b[1]<face[1]+.1*fh) {
                setEvidence(m,"role",literalEvidence("hair_back"));
                setEvidence(m,"semantic_source",literalEvidence("compact_surface_encloses_face"));
                setEvidence(m,"anatomical_support_fraction",covered);
                continue;
            }
        }
        if(torsoSupport.empty())continue;
        const auto body=supportBounds(torsoSupport);
        const double bw=body[2]-body[0],bh=body[3]-body[1];if(bw<=0||bh<=0)continue;
        const double overlap=dmin(b[2],body[2])-dmax(b[0],body[0]);
        const std::array<double,4> chestBox{body[0]-.12*bw,body[1]-.08*bh,
            body[2]+.12*bw,body[1]+.65*bh};
        // An upper-trunk lobe must have area and remain within the chest region.
        // Small ornaments, thin cords and laterally extending arms are excluded.
        if((role.empty()||role=="bodice")&&fractionInside(cloud,chestBox)>.94&&
            width>.25*bw&&width<1.2*bw&&height>.16*bh&&height<.65*bh&&
            overlap>.75*width&&width/height>.5&&width/height<4.) {
            setEvidence(m,"role",literalEvidence("bodice"));
            setEvidence(m,"semantic_source",literalEvidence("upper_trunk_surface_support"));
            setEvidence(m,"anatomical_support_fraction",fractionInside(cloud,chestBox));
            continue;
        }
        // Require alpha support through successive chest/waist bands. A bounding
        // box alone cannot distinguish a continuous dress from disconnected panels.
        if(role.empty()||role=="skirt"||role=="skirt_back"||role=="apron") {
            bool continuous=true;
            for(int band=0;band<5;++band) {
                compat::Slice<double> xs;
                const double low=body[1]+(.15+.1*band)*bh;
                for(auto p:cloud)if(p[1]>=low&&p[1]<low+.1*bh)xs.append(p[0]);
                if(xs.size()<3||quantile(xs,.9)-quantile(xs,.1)<.35*bw)continuous=false;
            }
            if(continuous&&b[1]<body[1]+.25*bh&&b[3]>body[1]+.65*bh&&
                overlap>.6*bw&&width<2*bw) {
                setEvidence(m,"role",literalEvidence("bodice"));
                setEvidence(m,"semantic_source",literalEvidence("continuous_garment_covers_torso"));
            }
        }
    }
    // Resolve head-rooted trailing surfaces before a generic tail is attached to the pelvis.
    // Use upper alpha support and connected continuation, including anonymously named parts.
    if(!faceSupport.empty()) {
        const auto face=supportBounds(faceSupport);
        const double fw=face[2]-face[0],fh=face[3]-face[1],cell=fw*.035;
        double faceOrder=0;
        for(auto m:materials)if(atEvidence(m,"role").as_string().view()=="face")
            faceOrder=dmax(faceOrder,ngRigNumber(getEvidence(m,"source_order",0.)));
        std::set<std::pair<int,int>> support;
        auto addSupport=[&](const compat::Slice<Point2>& cloud) {
            for(auto p:cloud)support.emplace(static_cast<int>(std::floor(p[0]/cell)),static_cast<int>(std::floor(p[1]/cell)));
        };
        if(cell>0&&fh>0)for(std::size_t pass=0;pass<materials.size();++pass) {
            bool changed=false;
            for(std::size_t i=0;i<materials.size();++i) {
                auto& m=materials[i];if(explicitRole[i]||atEvidence(m,"static").as_bool())continue;
                const auto role=atEvidence(m,"role").as_string().view();
                if(role!="tail"&&!role.empty())continue;
                auto cloud=ngRigPoints(atEvidence(m,"cloud"));if(cloud.empty())continue;
                const auto b=supportBounds(cloud);const double height=b[3]-b[1],width=b[2]-b[0];
                if(height<.7*fh||width<.08*fw||width>3*fw||b[1]>face[3]-.1*fh||
                    ngRigNumber(getEvidence(m,"source_order",0.))>=faceOrder)continue;
                const std::array<double,4> rootBox{face[0]-.5*fw,face[1]-.7*fh,face[2]+.5*fw,face[1]+.45*fh};
                const double rootFraction=fractionInside(cloud,rootBox);
                bool attached=b[1]<face[1]+.25*fh&&rootFraction>.06;
                double connected=0,count=0;
                if(!attached&&!support.empty())for(auto p:cloud)if(p[1]<b[1]+.2*height) {
                    ++count;const int x=static_cast<int>(std::floor(p[0]/cell)),y=static_cast<int>(std::floor(p[1]/cell));
                    bool hit=false;
                    for(int dx=-1;dx<=1&&!hit;++dx)for(int dy=-1;dy<=1;++dy)
                        if(support.contains({x+dx,y+dy})){hit=true;break;}
                    if(hit)++connected;
                }
                if(!attached&&(count==0||connected/count<.6))continue;
                setEvidence(m,"role",literalEvidence("hair_back"));
                setEvidence(m,"semantic_source",literalEvidence("head_rooted_connected_trailing_surface"));
                setEvidence(m,"anatomical_support_fraction",attached?rootFraction:connected/count);
                addSupport(cloud);changed=true;
            }
            if(!changed)break;
        }
    }
    if(!faceSupport.empty()&&!footSupport.empty()) {
        auto fb=supportBounds(faceSupport),feet=supportBounds(footSupport);compat::Slice<double> footY;for(auto p:footSupport)footY.append(p[1]);
        for(auto& m:materials)if(!atEvidence(m,"static").as_bool()&&atEvidence(m,"role").as_string().view()=="ear") {auto b=supportBounds(ngRigPoints(atEvidence(m,"cloud")));
            if(b[2]-b[0]>4*(fb[2]-fb[0])&&b[3]-b[1]>.85*(feet[3]-fb[1])&&b[3]>=quantile(footY,.5)&&ngRigNumber(getEvidence(m,"opaque_perimeter_coverage",0.))>=.65){setEvidence(m,"static",true);setEvidence(m,"semantic_source",literalEvidence("full_body_backdrop_alpha_perimeter"));}}
    }
    phase = "clipping groups";
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
    phase = "known material support";
    compat::Slice<std::shared_ptr<AlphaSupportIndex>> indices;
    for(std::size_t i=0;i<materials.size();++i)if(!atEvidence(materials[i],"static").as_bool()&&!atEvidence(materials[i],"role").as_string().view().empty()) {auto cloud=ngRigPoints(atEvidence(materials[i],"cloud"));if(!cloud.empty()){known.append(i);indices.append(allocateAlphaSupportIndex(cloud));}}
    for(std::size_t i=0;i<materials.size();++i) {
        phase = "spatial support: " + std::string(atEvidence(materials[i],"path").as_string().view());
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
        phase = "material ownership: " + std::string(atEvidence(m,"path").as_string().view());
        if(atEvidence(m,"role").as_string().view()=="local"){setEvidence(m,"owner",literalEvidence("local"));setEvidence(m,"chart",literalEvidence("local"));continue;}
        for(auto rule:atEvidence(specification,"rules").sourceArrayValue())if(atEvidence(rule,"id").as_string().view()==atEvidence(m,"role").as_string().view()){setEvidence(m,"owner",atEvidence(rule,"owner"));setEvidence(m,"chart",atEvidence(rule,"chart"));break;}
        enforceEvidence(evidenceObject(m).if_contains("owner"),"Unsupported automatic material role",422);
    }
    auto result=duplicateEvidence(observation);setEvidence(result,"materials",materials);setEvidence(result,"options",options);return result;
    } catch (const compat::ThrowableException& error) {
        throw compat::ThrowableException(copiedEvidenceMessage(phase + ": " + std::string(error.msg.view())),
            compat::RetainedString::fromStaticLiteral("nijigenerate/autorig/deterministic/evidence_classify.cpp"), 0);
    }
}
}
