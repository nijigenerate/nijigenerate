#include <nijigenerate/autorig/patterns/core.hpp>
#include <nijigenerate/autorig/patterns/detection.hpp>
#include <iostream>
#include <stdexcept>
#include <functional>

using namespace nijigenerate::autorig::patterns;
using O=boost::json::object;using A=boost::json::array;
void expect(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
Json part(int id,double w=2,double h=2) {
    return O{{"uuid",id},{"type","Part"},{"name","unclassified"},{"children",A{}},
        {"mesh",O{{"verts",A{-w,-h,w,-h,w,h,-w,h}},{"indices",A{0,1,2,0,2,3}}}}};
}
Json binding(int node,std::string property,std::function<Json(int,int)> value) {
    A samples,flags;for(int x=-1;x<=1;++x){A row,set;for(int y=-1;y<=1;++y){row.push_back(value(x,y));set.push_back(true);}
        samples.push_back(row);flags.push_back(set);}
    return O{{"node",node},{"param_name",property},{"values",samples},{"isSet",flags},{"interpolate_mode","Linear"}};
}
Json parameter(int id,Json b) {
    return O{{"uuid",id},{"name","anonymous control"},{"is_vec2",true},{"min",A{-1,-1}},{"max",A{1,1}},
        {"defaults",A{0,0}},{"axis_points",A{A{0.,0.5,1.},A{0.,0.5,1.}}},{"bindings",A{b}}};
}
Json fixture() {
    auto longPart=part(2,4,0.5),surface=part(3,5,5),compact=part(4,0.1,0.1),tail=part(6,4,0.5),tail2=part(7,4,0.5),outer=part(8,8,8);
    longPart.as_object()["children"]=A{compact};
    longPart.as_object()["masks"]=A{O{{"source",3},{"mode","Mask"}},O{{"source",8},{"mode","DodgeMask"}}};
    surface.as_object()["masks"]=A{O{{"source",8},{"mode","Mask"}}};
    longPart.as_object()["blend_mode"]="Multiply";
    longPart.as_object()["weldedLinks"]=A{O{{"targetUUID",3}},O{{"targetUUID",8}}};
    tail.as_object()["blend_mode"]="ClipToLower";
    auto physics=[](int id,int output){return O{{"uuid",id},{"type","SimplePhysics"},{"param",output}};};
    Json root=O{{"uuid",1},{"type","Node"},{"children",A{O{{"uuid",20},{"type","Composite"},
        {"children",A{longPart,surface,tail,tail2,outer,physics(30,100),physics(31,103)}}}}}};
    A params;
    params.push_back(parameter(100,binding(2,"deform",[](int x,int){return A{A{-0.4*x,-0.05*x},A{0.4*x,-0.05*x},A{0.4*x,0.05*x},A{-0.4*x,0.05*x}};})));
    params.push_back(parameter(101,binding(3,"deform",[](int x,int){return A{A{0,4.5*x},A{0,4.5*x},A{0,-4.5*x},A{0,-4.5*x}};})));
    params.push_back(parameter(102,binding(2,"deform",[](int x,int){return A{A{0,0},A{0,0},A{0.4*x,0},A{0,0}};})));
    params.push_back(parameter(103,binding(2,"deform",[](int x,int y){return A{A{0.1*x,0.1*y},A{0.1*x,0.1*y},A{0.1*x,0.1*y},A{0.1*x,0.1*y}};})));
    params.push_back(parameter(104,binding(2,"transform.r.z",[](int x,int){return x*0.1;})));
    params.push_back(parameter(105,binding(2,"opacity",[](int x,int){return x>0?0:1;})));
    params.push_back(parameter(106,binding(2,"zSort",[](int x,int){return x*2.;})));
    params.push_back(parameter(107,binding(2,"tint.r",[](int x,int){return 1+x*0.1;})));
    params.push_back(parameter(108,binding(2,"deform",[](int x,int y){return A{A{0.,0.},A{0.,0.},A{0.3*x*y,0.},A{0.,0.}};})));
    params.push_back(parameter(109,binding(20,"transform.t.x",[](int x,int){return x*5.;})));
    params.push_back(parameter(110,binding(30,"outputScale.x",[](int x,int){return 1+x*0.1;})));
    params.push_back(parameter(120,binding(100,"X",[](int x,int){return x*0.5;})));
    return O{{"nodes",root},{"param",params}};
}
void rename(Json& v) {
    if(v.is_object()) {if(v.as_object().contains("name"))v.as_object()["name"]="Arbitrary unrelated text";
        for(auto& field:v.as_object())rename(field.value());}
    if(v.is_array())for(auto& item:v.as_array())rename(item);
}
int main() {
    try {
        const auto model=fixture();const auto observed=ngDetectStructuralPatterns(model);
        for(const auto& spec:ngPatternSpecs()) {
            const auto& entry=observed.as_object().at(spec.id).as_object();
            expect(!entry.at("matches").as_array().empty() || !entry.at("candidates").as_array().empty(),std::string("Missing positive detector: ")+spec.id);
            for(const auto* kind:{"matches","candidates"})for(const auto& match:entry.at(kind).as_array()) {
                expect(!match.as_object().at("evidence").as_array().empty(),"Missing evidence");
                expect(match.as_object().at("requires_review").as_bool()==(std::string(kind)=="candidates"),"Invalid review gate");
            }
            std::cout<<spec.id<<" positive detection passed\n";
        }
        auto changed=model;rename(changed);expect(ngDetectStructuralPatterns(changed)==observed,"Detection depends on names");
        auto separable=model;
        auto& separableParameters=separable.as_object().at("param").as_array();
        for(auto i=separableParameters.begin();i!=separableParameters.end();++i)
            if(i->as_object().at("uuid").to_number<int>()==108){separableParameters.erase(i);break;}
        expect(ngDetectStructuralPatterns(separable).as_object().at("p03").as_object().at("matches").as_array().empty(),
            "Additive independent axes mistaken for cross-input residual");
        Json blank=O{{"nodes",O{{"uuid",1},{"type","Node"},{"children",A{part(2)}}}},{"param",nullptr}};
        const auto negative=ngDetectStructuralPatterns(blank);
        for(const auto& spec:ngPatternSpecs()) {
            const auto& entry=negative.as_object().at(spec.id).as_object();
            expect(entry.at("matches").as_array().empty() && entry.at("candidates").as_array().empty(),std::string("False positive on neutral square: ")+spec.id);
        }
        auto ring=blank;
        ring.as_object().at("nodes").as_object().at("children").as_array()[0].as_object()["mesh"]=O{
            {"verts",A{-2,-2,2,-2,2,2,-2,2,-1,-1,1,-1,1,1,-1,1}},
            {"indices",A{0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7}}};
        expect(!ngDetectStructuralPatterns(ring).as_object().at("p27").as_object().at("candidates").as_array().empty(),
            "Mesh loop was not detected");
        auto disconnected=blank;
        disconnected.as_object().at("nodes").as_object().at("children").as_array()[0].as_object()["mesh"]=O{
            {"verts",A{-2,-2,-1,-2,-2,-1,1,1,2,1,1,2}},{"indices",A{0,1,2,3,4,5}}};
        expect(!ngDetectStructuralPatterns(disconnected).as_object().at("p34").as_object().at("candidates").as_array().empty(),
            "Disconnected material regions were not detected");
        auto contact=blank;
        auto adjacent=part(3);adjacent.as_object()["transform"]=O{{"trans",A{4,0,0}}};
        contact.as_object().at("nodes").as_object().at("children").as_array().push_back(adjacent);
        expect(!ngDetectStructuralPatterns(contact).as_object().at("p16").as_object().at("candidates").as_array().empty(),
            "Coincident boundary seam was not proposed");
        contact.as_object().at("nodes").as_object().at("children").as_array()[1].as_object()["transform"]=O{{"trans",A{3,0,0}}};
        expect(ngDetectStructuralPatterns(contact).as_object().at("p16").as_object().at("candidates").as_array().empty(),
            "Overlapping bounds without nearby boundary vertices mistaken for seam");
        auto constant=blank;constant.as_object()["param"]=A{parameter(50,binding(2,"opacity",[](int,int){return 1.;})),
            parameter(51,binding(2,"zSort",[](int,int){return 0.;}))};
        auto constants=ngDetectStructuralPatterns(constant);
        expect(constants.as_object().at("p12").as_object().at("matches").as_array().empty(),"Constant zSort considered animated");
        expect(constants.as_object().at("p13").as_object().at("matches").as_array().empty(),"Constant opacity considered animated");
        auto unset=constant;auto& b=unset.as_object().at("param").as_array()[0].as_object().at("bindings").as_array()[0].as_object();
        b["values"].as_array()[2].as_array()[2]=0.;b["isSet"].as_array()[2].as_array()[2]=false;
        expect(ngDetectStructuralPatterns(unset).as_object().at("p13").as_object().at("matches").as_array().empty(),"Unset key counted as evidence");
        b["isSet"].as_array()[1].as_array()[1]=false;b["isSet"].as_array()[2].as_array()[2]=true;
        expect(!ngDetectStructuralPatterns(unset).as_object().at("p13").as_object().at("matches").as_array().empty(),
            "Variation lost when neutral key is implicit");
        auto selected=ngDetectPattern("p27",model,O{});
        expect(selected.as_object().at("matches").as_array().empty() && !selected.as_object().at("candidates").as_array().empty(),
            "Ambiguous ribbon topology auto-selected");
        bool blocked=false;
        try{ngDeformPattern("p27",model,selected,O{{"controls",A{}}});}catch(const std::invalid_argument&){blocked=true;}
        expect(blocked,"Unreviewed structural candidate reached deformation");
        auto candidate=selected.as_object().at("candidates").as_array()[0];
        auto approved=ngDetectPattern("p27",model,O{{"mode","manual"},{"matches",A{candidate}}});
        expect(!approved.as_object().at("matches").as_array()[0].as_object().at("requires_review").as_bool(),
            "Explicit manual replacement did not resolve candidate review");
        auto direct=ngDetectPattern("p13",model,O{});
        expect(!direct.as_object().at("parts").as_array().empty(),"Detector output not wired to Part selection");
        auto empty=ngDetectPattern("p13",model,O{{"mode","manual"},{"matches",A{}}});
        expect(empty.as_object().at("matches").as_array().empty() && empty.as_object().at("candidates").as_array().empty(),"Manual empty selection fell back to detector");
        bool canceled=false;try{ngDetectStructuralPatterns(model,[]{throw std::runtime_error("cancel");});}catch(const std::runtime_error&){canceled=true;}
        expect(canceled,"Detector ignored cancellation");
        std::cout<<"All detector positive/negative/name-invariance/review-gate tests passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
