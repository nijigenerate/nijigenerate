#include "detail/evidence.hpp"
#include <nijigenerate/compat/range_error.hpp>
namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;
Json ngRigDeriveEvidence(Json observation) {
    auto materials=atEvidence(observation,"materials").sourceArrayValue();bool hasFace=false,humanoidSupport=false;std::size_t active=0,mouthCount=0;
    for(auto m:materials)if(!atEvidence(m,"static").as_bool()) {
        ++active;auto role=atEvidence(m,"role").as_string().view();hasFace=hasFace||role=="face";
        humanoidSupport=humanoidSupport||role=="torso"||role=="bodice"||role=="leg"||role=="arm";
        if(ngRigString(m,stringLiteralEvidence("feature"),compat::RetainedString::emptyLiteral()).view().starts_with("mouth"))++mouthCount;
    }
    enforceEvidence(active>0,"Imported model has no active deforming material",441);
    auto kind=hasFace&&humanoidSupport?compat::RetainedString::fromStaticLiteral("humanoid"):hasFace?compat::RetainedString::fromStaticLiteral("face"):mouthCount==active?compat::RetainedString::fromStaticLiteral("mouth"):compat::RetainedString::fromStaticLiteral("local");
    if(kind.view()!="humanoid")return objectEvidence({{stringLiteralEvidence("kind"),core::settings::SettingsString(kind)},{stringLiteralEvidence("source_sha256"),atEvidence(observation,"source_sha256")},{stringLiteralEvidence("observation_sha256"),core::settings::SettingsString(ngRigDigest(observation))}});
    auto select=[&](std::initializer_list<std::string_view> roles,std::string side="",double center=0.) {
        compat::Slice<Point2> points;
        for(auto m:materials) {
            if(atEvidence(m,"static").as_bool())continue;bool selected=false;
            for(auto role:roles)if(atEvidence(m,"role").as_string().view()==role)selected=true;if(!selected)continue;
            for(auto p:ngRigPoints(atEvidence(m,"cloud")))if(side.empty()||((p[0]<center)==(side=="R")))points.append(p);
        }
        return points;
    };
    auto face=select({"face"}),torso=select({"torso","bodice","waistwear"});
    enforceEvidence(!face.empty()&&!torso.empty(),"Humanoid evidence needs visible face and torso support",458);
    compat::Slice<double> faceX,faceY;for(auto p:face){faceX.append(p[0]);faceY.append(p[1]);}
    auto center=quantile(faceX,.5);auto neckPoints=select({"neck"});
    Json landmarks=SettingsJsonObject{},radii=SettingsJsonObject{};
    auto put=[&](core::settings::SettingsString role,Point2 p,core::settings::SettingsString provenance=compat::RetainedString::fromStaticLiteral("measured")) {
        setEvidence(landmarks,role,objectEvidence({{stringLiteralEvidence("xy"),pointJson(p)},{stringLiteralEvidence("provenance"),provenance},{stringLiteralEvidence("weight"),1.},{stringLiteralEvidence("method"),literalEvidence("PSD alpha quantile section")}}));
    };
    auto facial=registered::ngRigFacialLandmarks(observation);
    const auto facialPoint = [&](std::string_view key, std::size_t line) {
        const auto found = facial.find(key);
        if (found == facial.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/evidence.d"), line);
        return found->second;
    };
    const auto rightInner = facialPoint("eye_r_inner",469);
    const auto rightOuter = facialPoint("eye_r_outer",469);
    const auto rightEye = midpoint(rightInner, rightOuter);
    const auto leftInner = facialPoint("eye_l_inner",470);
    const auto leftOuter = facialPoint("eye_l_outer",470);
    const auto leftEye = midpoint(leftInner, leftOuter);
    auto headFrame = neck::ngRigInferHeadFrame(face, rightEye, leftEye);
    put(stringLiteralEvidence("head_top"),ngRigPoint(atEvidence(headFrame,"head_top")));put(stringLiteralEvidence("head_root"),ngRigPoint(atEvidence(headFrame,"head_root")));
    const auto bareTorso = select({"torso"});
    const auto clothedTorso = select({"bodice", "waistwear"});
    auto neckInference = neck::ngRigInferNeckBase(face, neckPoints, bareTorso, clothedTorso, headFrame);
    auto neckBase=ngRigPoint(atEvidence(neckInference,"xy"));put(stringLiteralEvidence("neck_base"),neckBase,atEvidence(neckInference,"provenance").as_string());
    setNestedEvidence(landmarks,"neck_base","method",atEvidence(neckInference,"method"));
    for(std::string side:{"R","L"}) {
        auto arm=select({"arm"},side,center),hand=select({"hand"},side,center);if(arm.empty())arm=select({"sleeve"},side,center);arm.append(hand);
        auto leg=select({"leg"},side,center),foot=select({"foot"},side,center);enforceEvidenceLazy(!arm.empty()&&!leg.empty(),[&]{return "Missing anatomical limb support: "+side;},481);
        auto armStart=ngRigMeasuredSection(arm,.01),armEnd=ngRigMeasuredSection(hand.empty()?arm:hand,.99),armAxis=minus(armEnd,armStart);
        put("shoulder."+side,ngRigMeasuredSection(arm,.02,armAxis));put("elbow."+side,ngRigMeasuredSection(arm,.52,armAxis),stringLiteralEvidence("prior"));
        put("wrist."+side,ngRigMeasuredSection(hand.empty()?arm:hand,hand.empty()?.82:.03,armAxis),hand.empty()?stringLiteralEvidence("prior"):stringLiteralEvidence("measured"));put("hand_tip."+side,armEnd);
        auto hip=ngRigMeasuredSection(leg,.01),distal=ngRigMeasuredSection(foot.empty()?leg:foot,.98),legAxis=minus(distal,hip);
        put("hip."+side,hip,stringLiteralEvidence("prior"));put("knee."+side,ngRigMeasuredSection(leg,.47,legAxis),stringLiteralEvidence("prior"));
        put("ankle."+side,ngRigMeasuredSection(foot.empty()?leg:foot,foot.empty()?.87:.08,legAxis),foot.empty()?stringLiteralEvidence("prior"):stringLiteralEvidence("measured"));put("foot_tip."+side,distal);
        for(std::string family:{"arm","leg"}) {
            auto cloud=family=="arm"?arm:leg;auto axis=family=="arm"?armAxis:legAxis;auto length=norm(axis);for(auto& x:axis)x/=length;
            compat::Slice<double> t;for(auto p:cloud)t.append(p[0]*axis[0]+p[1]*axis[1]);
            auto low=quantile(t,.01),high=quantile(t,.99);compat::Slice<double> widths;
            for(int station=0;station<15;++station) {
                double target=low+(.15+.7*station/14)*(high-low);compat::Slice<double> cross;
                for(std::size_t i=0;i<cloud.size();++i)if(std::abs(t[i]-target)<=dmax(1.,(high-low)*.025))cross.append(cloud[i][0]*axis[1]-cloud[i][1]*axis[0]);
                if(!cross.empty())widths.append(quantile(cross,1)-quantile(cross,0));
            }
            setEvidence(radii,family+":"+(side=="R"?"r":"l"),dmax(1.,quantile(widths,.5)/2));
        }
    }
    const auto leftHip = ngRigPoint(atEvidence(atEvidence(landmarks, "hip.L"), "xy"));
    const auto rightHip = ngRigPoint(atEvidence(atEvidence(landmarks, "hip.R"), "xy"));
    auto pelvis = midpoint(leftHip, rightHip); put(stringLiteralEvidence("pelvis"), pelvis, stringLiteralEvidence("prior"));
    auto torsoAxis=minus(pelvis,neckBase);double lengthSquared=torsoAxis[0]*torsoAxis[0]+torsoAxis[1]*torsoAxis[1];
    enforceEvidence(lengthSquared>0,"Degenerate observed torso axis",514);
    auto chest=ngRigMeasuredSection(torso,.4),waist=ngRigMeasuredSection(torso,.9);
    double chestFraction=((chest[0]-neckBase[0])*torsoAxis[0]+(chest[1]-neckBase[1])*torsoAxis[1])/lengthSquared;
    double waistFraction=((waist[0]-neckBase[0])*torsoAxis[0]+(waist[1]-neckBase[1])*torsoAxis[1])/lengthSquared;
    bool skinTorso=false;for(auto m:materials)if(!atEvidence(m,"static").as_bool()&&atEvidence(m,"role").as_string().view()=="torso")skinTorso=true;
    double gap=ngRigNumber(atEvidence(atEvidence(templates::ngRigHumanoidPrior(),"torso_axis"),"minimum_station_gap"));
    bool measuredStations=skinTorso&&chestFraction>gap&&waistFraction-chestFraction>gap&&1-waistFraction>gap;
    if(!measuredStations){chestFraction=.35;waistFraction=.75;}
    put(stringLiteralEvidence("chest"),{neckBase[0]+chestFraction*torsoAxis[0],neckBase[1]+chestFraction*torsoAxis[1]},stringLiteralEvidence("prior"));
    put(stringLiteralEvidence("waist"),{neckBase[0]+waistFraction*torsoAxis[0],neckBase[1]+waistFraction*torsoAxis[1]},stringLiteralEvidence("prior"));
    for(auto role:{"chest","waist"})setNestedEvidence(landmarks,role,"method",measuredStations?
        literalEvidence("observed garment station projected onto the shared anatomical torso axis"):
        literalEvidence("common ordered torso stations; garment sections fall outside anatomical station constraints"));
    Point2 faceLow={quantile(faceX,.01),quantile(faceY,.01)},faceHigh={quantile(faceX,.99),quantile(faceY,.99)};
    const auto leftShoulder = ngRigPoint(atEvidence(atEvidence(landmarks, "shoulder.L"), "xy"));
    const auto rightShoulder = ngRigPoint(atEvidence(atEvidence(landmarks, "shoulder.R"), "xy"));
    auto shoulderWidth = norm(minus(leftShoulder, rightShoulder));
    Json volumes=SettingsJsonObject{};
    setEvidence(volumes,"head",objectEvidence({{stringLiteralEvidence("center"),pointJson(midpoint(faceLow,faceHigh))},{stringLiteralEvidence("radii"),array({(faceHigh[0]-faceLow[0])/2,(faceHigh[1]-faceLow[1])/2})}}));
    setEvidence(volumes,"torso",objectEvidence({{stringLiteralEvidence("center"),pointJson(midpoint(neckBase,pelvis))},{stringLiteralEvidence("radii"),array({shoulderWidth*.45,norm(torsoAxis)/2})}}));
    JsonArray identity;
    identity.resize(3);
    identity[0] = array({1.,0.,0.});
    identity[1] = array({0.,1.,0.});
    identity[2] = array({0.,0.,1.});
    auto result=objectEvidence({{stringLiteralEvidence("kind"),literalEvidence("humanoid")},{stringLiteralEvidence("landmarks"),landmarks},{stringLiteralEvidence("neck_inference"),neckInference},{stringLiteralEvidence("head_frame"),headFrame},
        {stringLiteralEvidence("volumes"),volumes},{stringLiteralEvidence("limb_radii"),radii},{stringLiteralEvidence("source_to_model"),identity},{stringLiteralEvidence("observation_sha256"),core::settings::SettingsString(ngRigDigest(observation))},{stringLiteralEvidence("source_sha256"),atEvidence(observation,"source_sha256")}});
    for(std::string family:{"arm","leg"})setNestedEvidence(result,"limb_radii",family+":both",(ngRigNumber(atEvidence(radii,family+":r"))+ngRigNumber(atEvidence(radii,family+":l")))/2);
    auto sourceOptions=atEvidence(observation,"options");
    if(auto overrides=evidenceObject(sourceOptions).if_contains("landmarks"))for(const auto& member:evidenceObject(*overrides)) {
        auto role=member.name;enforceEvidenceLazy(evidenceObject(landmarks).if_contains(role.view()),[&]{return "Unknown anatomical landmark override: "+std::string(role.view());},545);
        setNestedEvidence(result,"landmarks",role,objectEvidence({{stringLiteralEvidence("xy"),pointJson(ngRigPoint(member.value()))},{stringLiteralEvidence("provenance"),literalEvidence("visual_estimate")},{stringLiteralEvidence("weight"),1.}}));
    }
    return result;
}
}
