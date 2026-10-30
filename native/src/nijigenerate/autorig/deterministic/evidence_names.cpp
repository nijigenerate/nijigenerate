#include "detail/evidence.hpp"
namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;
namespace {
bool matches(std::string_view value,const char* pattern) { return std::regex_search(value.begin(),value.end(),std::regex(pattern,std::regex::ECMAScript)); }
bool letter(char c) { return c>='a'&&c<='z'; }
bool digit(char c) { return c>='0'&&c<='9'; }
template<std::size_t N> compat::RetainedStringSlice featureLiteral(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
}
std::string detail::nameTokens(compat::RetainedStringSlice name) {
    while(!name.empty()&&name.view().back()=='\0')name=name.slice(0,name.size()-1);
    while(!name.empty()&&(name.view()[0]=='*'||name.view()[0]=='#'||name.view()[0]==' '))name=name.slice(1,name.size());
    auto lower=io::imageexport::detail::lowerExtension(name);std::string result;
    // std.regex's \s is unicode.White_Space, not the C locale's whitespace class.
    core::i18n::detail::LocaleCaseCursor cursor{lower.view()};
    while(!cursor.empty()) {
        const auto start=cursor.offset;auto c=cursor.take();bool white=(c>=9&&c<=13)||c==32||c==0x85||c==0xa0||c==0x1680||
            (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000;
        if(white||c==':'||c=='-'||c=='_') { if(result.empty()||result.back()!='_')result.push_back('_'); }
        else result.append(lower.view().substr(start,cursor.offset-start));
    }
    while(!result.empty()&&result.front()=='_')result.erase(0,1);
    while(!result.empty()&&result.back()=='_')result.pop_back();
    std::string separated;
    for(std::size_t i=0;i<result.size();++i) { if(i&&((letter(result[i-1])&&digit(result[i]))||(digit(result[i-1])&&letter(result[i]))))separated.push_back('_');separated.push_back(result[i]); }
    result=std::move(separated);
    // These nonoverlapping aliases have no order-dependent replacements.
    for(auto [source,target]:std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"irides","iris"},{"eyewhite","sclera"},{"eye_white","sclera"},{"eyeslash","eyelash"},{"eylid","eyelid"},
        {"innewr","inner"},{"sholder","shoulder"},{"cloths","clothes"},{"sodenhair","side_hair"}}}) {
        std::size_t start=0;
        while((start=result.find(source,start))!=std::string::npos) {
            auto end=start+source.size();if((start==0||!letter(result[start-1]))&&(end==result.size()||!letter(result[end]))) {result.replace(start,source.size(),target);start+=target.size();}else start=end;
        }
    }
    return result;
}
compat::RetainedStringSlice detail::semanticRole(compat::RetainedStringSlice name,Strings ancestors) {
    auto n=nameTokens(name);std::vector<std::string> parents;for(auto parent:ancestors)parents.push_back(nameTokens(parent));
    auto test=[&](const char* pattern,std::string_view value={}){return matches(value.empty()?std::string_view(n):value,pattern);};
    if(test("(^|_)(background|backdrop)(_|$)"))return featureLiteral("background");
    std::string joined;for(std::size_t i=0;i<parents.size();++i){if(i)joined+="_";joined+=parents[i];}
    if(test("fox_decor|plush|baggage|objects",joined+"_"+n))return featureLiteral("pelvis_accessory");
    if(test("(^|_)(mouth|lip|tongue|teeth)(_|$)"))return featureLiteral("face_feature");
    for(const auto& parent:parents)if(test("^mouth(?:_composite)?$",parent))return featureLiteral("face_feature");
    if(test("(^|_)(eyebrow|brow|sclera|eyeball|iris|pupil|canthus|eye_corner|side_eyelash|eyelid|eyeline|lid|lash|eyelash|double_eyelid)(_|$)|(^|_)eye_(white|highlight|light)(_|$)|^nose(?:_[0-9]+)?$"))return featureLiteral("face_feature");
    if(test("^(face|head_skin|head)(?:_[0-9]+)?$"))return featureLiteral("face");
    if(test("^neck(?:_[0-9]+)?$"))return featureLiteral("neck");
    if(test("(^|_)(hair|bang|bangs|braid)(_|$)")){if(test("back"))return featureLiteral("hair_back");if(test("side|cheek|curl|braid"))return featureLiteral("hair_side");return featureLiteral("hair_front");}
    if(test("earring|earwear"))return featureLiteral("headwear");
    if(test("(^|_)(ear|ears)(_|$)"))return featureLiteral("ear");
    if(test("(^|_)(headwear|headband|cap|forehead|temple|head|hair)_(bow|ribbon|flower|watch|cross)|^(headwear|headband|cap)(_|$)"))return featureLiteral("headwear");
    if(test("(^|_)(shoe|shoes|foot|footwear)(_|$)"))return featureLiteral("foot");
    if(test("(^|_)(hand|palm|finger|thumb)(_|$)"))return featureLiteral("hand");
    if(test("handwear"))return featureLiteral("arm");
    if(test("(^|_)(arm|arms|upperarm|forearm)(_|$)"))return featureLiteral("arm");
    if(test("(^|_)(leg|legs|legwear|thigh|shin|pants|socks)(_|$)"))return featureLiteral("leg");
    if(test("(^|_)(sleeve|cuff|shoulder)(_|$)"))return featureLiteral("sleeve");
    if(test("tail"))return featureLiteral("tail");if(test("apron"))return featureLiteral("apron");
    if(test("skirt|bottomwear|train"))return test("back")?featureLiteral("skirt_back"):featureLiteral("skirt");
    if(test("corset|belt|waistwear"))return featureLiteral("waistwear");
    if(test("clothes|topwear|bodice|collar|parker|pocket"))return featureLiteral("bodice");
    if(test("(^|_)(body|torso|chest|pelvis)(_|$)"))return featureLiteral("torso");
    if(test("waist|hip"))return featureLiteral("pelvis_accessory");
    for(auto i=parents.rbegin();i!=parents.rend();++i){const auto& p=*i;
        if(test("hand",p))return featureLiteral("hand");if(test("arms|arm",p))return featureLiteral("sleeve");if(test("legs|leg",p))return featureLiteral("leg");
        if(test("skirt",p))return featureLiteral("skirt");if(test("upper_body|torso|body",p))return featureLiteral("chest_accessory");
        if(test("cap|headwear",p))return featureLiteral("headwear");if(test("back_head|back_hair",p))return featureLiteral("hair_back");if(test("head",p))return featureLiteral("headwear");
    }
    return featureLiteral("");
}
compat::RetainedStringSlice ngRigMaterialFeature(compat::RetainedStringSlice name,Strings ancestors) {
    auto n = nameTokens(name);
    auto test = [&](const char* pattern) { return matches(n, pattern); };
    bool mouth = test("(^|_)(mouth|lip|tongue|teeth)(_|$)");
    for (auto parent : ancestors)
        if (matches(nameTokens(parent), "^mouth(?:_composite)?$")) mouth = true;
    if (mouth) {
        if (test("tongue")) return featureLiteral("mouth_tongue");
        if (test("teeth")) return test("upper") ? featureLiteral("mouth_upper_teeth") :
            featureLiteral("mouth_lower_teeth");
        if (test("outline")) return featureLiteral("mouth_outline");
        if (test("lip")) return test("upper") ? featureLiteral("mouth_upper_lip") :
            featureLiteral("mouth_lower_lip");
        return featureLiteral("mouth");
    }
    if (test("(^|_)(eyebrow|brow)(_|$)")) return featureLiteral("brow");
    if (test("(^|_)(sclera|eyeball)(_|$)")) return featureLiteral("sclera");
    if (test("(^|_)(iris|pupil)(_|$)|(^|_)eye_(highlight|light)(_|$)")) return featureLiteral("iris");
    if (test("(^|_)(canthus|eye_corner|side_eyelash)(_|$)")) return featureLiteral("corner");
    if (test("(^|_)(eyelid|eyeline|lid|lash|eyelash|double_eyelid)(_|$)")) {
        if (test("lower|bottom")) return featureLiteral("lower");
        if (test("double|fold")) return featureLiteral("fold");
        return test("side") ? featureLiteral("corner") : featureLiteral("upper");
    }
    if (test("^nose(?:_[0-9]+)?$")) return featureLiteral("nose");
    return featureLiteral("");
}
Strings ngRigMaterialRoleCandidates(compat::RetainedStringSlice name) {auto role=semanticRole(name,{});return !role.empty()&&role.view()!="background"?Strings{role}:Strings{};}
}
