#include "core.hpp"
#include "detection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace nijigenerate::autorig::patterns {
namespace {
using O = boost::json::object;
using A = boost::json::array;
using Id = std::uint32_t;
using V = std::array<double, 2>;
const Json null;
const Json& at(const O& v, std::string_view key) {
    if (auto* item=v.if_contains(key)) return *item;
    return null;
}
const Json& at(const Json& v, std::string_view key) {
    if (v.is_object()) if (auto* item = v.as_object().if_contains(key)) return *item;
    return null;
}
const A& arr(const Json& v) { static const A empty; return v.is_array() ? v.as_array() : empty; }
double num(const Json& v, double fallback = 0) { return v.is_number() ? v.to_number<double>() : fallback; }
Id id(const Json& v) { return static_cast<Id>(num(v)); }
double component(const Json& v, std::size_t i, double fallback = 0) {
    return arr(v).size() > i ? num(arr(v)[i], fallback) : fallback;
}
struct Frame {
    double a=1, b=0, c=0, d=1, x=0, y=0;
    bool valid=true;
    V vector(V p) const { return {a*p[0]+b*p[1], c*p[0]+d*p[1]}; }
    V inverseVector(V p) const {
        double det=a*d-b*c;
        return {(d*p[0]-b*p[1])/det, (-c*p[0]+a*p[1])/det};
    }
};
Frame compose(Frame p, Frame q) {
    return {p.a*q.a+p.b*q.c, p.a*q.b+p.b*q.d, p.c*q.a+p.d*q.c, p.c*q.b+p.d*q.d,
        p.a*q.x+p.b*q.y+p.x, p.c*q.x+p.d*q.y+p.y, p.valid && q.valid};
}
struct Node {
    const Json* raw=nullptr;
    Id parent=0;
    Frame frame;
    std::vector<V> points;
    V low{}, high{}, center{};
};
struct Scene {
    std::map<Id, Node> nodes;
    std::map<Id, const Json*> parameters;
    Frame root;
    void read(const Json& raw, Id parent=0, Frame inherited={}) {
        const auto& tr=at(raw,"transform");
        double r=component(at(tr,"rot"),2), sx=component(at(tr,"scale"),0,1), sy=component(at(tr,"scale"),1,1);
        Frame local{std::cos(r)*sx,-std::sin(r)*sy,std::sin(r)*sx,std::cos(r)*sy,
            component(at(tr,"trans"),0),component(at(tr,"trans"),1),
            std::abs(component(at(tr,"rot"),0))<1e-8 && std::abs(component(at(tr,"rot"),1))<1e-8 && std::abs(sx*sy)>1e-8};
        if (!parent) root=local;
        if (parent && at(raw,"lockToRoot")==true) inherited=root;
        Node n; n.raw=&raw; n.parent=parent; n.frame=compose(inherited,local);
        const auto& vs=arr(at(at(raw,"mesh"),"verts"));
        for (std::size_t i=0;i+1<vs.size();i+=2) {
            V v{num(vs[i]),num(vs[i+1])};
            if (n.points.empty()) n.low=n.high=v;
            for(int d=0;d<2;++d) { n.low[d]=std::min(n.low[d],v[d]); n.high[d]=std::max(n.high[d],v[d]); }
            n.points.push_back(v);
        }
        for(int d=0;d<2;++d)n.center[d]=(n.low[d]+n.high[d])/2;
        auto uuid=id(at(raw,"uuid"));nodes[uuid]=n;
        for(const auto& child:arr(at(raw,"children")))read(child,uuid,n.frame);
    }
    void readParameters(const Json& values) {
        for(const auto& p:arr(values)) {
            if (!at(p,"groupUUID").is_null()) readParameters(at(p,"children"));
            else parameters[id(at(p,"uuid"))]=&p;
        }
    }
};
O match(Id target, std::string_view role={}, Id support=0) {
    O roles{{"target",A{target}}};
    if(!role.empty())roles[role]=A{support};
    return O{{"roles",roles},{"provenance","automatic_structural_plan"},
        {"requires_review",false},{"evidence",A{"mask_graph_and_sampled_aperture"}}};
}
O control(std::string label, A bindings, A x=A{-1,0,1}, A y=A{-1,0,1}) {
    return O{{"label",label},{"structural_key",label},{"x",x},{"y",y},{"bindings",bindings}};
}
template<class F> O binding(Id target, std::string_view property, const A& xs, const A& ys, F sample) {
    A rows, flags;
    for(std::size_t x=0;x<xs.size();++x) {
        A row, set;
        for(std::size_t y=0;y<ys.size();++y) { row.push_back(sample(x,y));set.push_back(true); }
        rows.push_back(row);flags.push_back(set);
    }
    return O{{"node",target},{"param_name",property},{"values",rows},{"isSet",flags},{"interpolate_mode","Linear"}};
}
A physicalAxis(const Json& p,int dim) {
    A axis;
    const auto& axes=arr(at(p,"axis_points"));
    if(axes.size()!=2)return axis;
    double lo=component(at(p,"min"),dim),hi=component(at(p,"max"),dim);
    for(const auto& v:arr(axes[dim]))axis.push_back(lo+num(v)*(hi-lo));
    if(axis.size()==1 || (!axis.empty() && std::abs(num(axis.back())-num(axis.front()))<1e-9))axis=A{-1,0,1};
    return axis;
}
bool zeroAxis(const A& axis) {
    return axis.size()>=2 && std::any_of(axis.begin(),axis.end(),[](const auto& v){return std::abs(num(v))<1e-9;});
}
// A collapsing surface is identified by the motion of its mesh, not by an eye label.
bool collapsing(const Node& n,const Json& b) {
    if(at(b,"param_name")!="deform" || n.points.empty())return false;
    double baseW=n.high[0]-n.low[0],baseH=n.high[1]-n.low[1];
    if(baseW<1e-6 || baseH<1e-6)return false;
    const auto& rows=arr(at(b,"values"));const auto& flags=arr(at(b,"isSet"));
    for(std::size_t x=0;x<rows.size();++x)for(std::size_t y=0;y<arr(rows[x]).size();++y) {
        if(x>=flags.size() || y>=arr(flags[x]).size() || flags[x].as_array()[y]!=true)continue;
        const auto& offsets=arr(rows[x].as_array()[y]);if(offsets.size()!=n.points.size())continue;
        V low{},high{};
        for(std::size_t i=0;i<offsets.size();++i)for(int d=0;d<2;++d) {
            double v=n.points[i][d]+component(offsets[i],d);
            if(!i)low[d]=high[d]=v;else {low[d]=std::min(low[d],v);high[d]=std::max(high[d],v);}
        }
        if((high[1]-low[1])/baseH<0.45 && (high[0]-low[0])/baseW>0.7)return true;
    }
    return false;
}
bool translatesOnTwoAxes(const Node& node, const Json& binding) {
    if (at(binding,"param_name") != "deform" || node.points.empty()) return false;
    const auto& rows = arr(at(binding,"values"));
    const auto& flags = arr(at(binding,"isSet"));
    if (rows.size() < 2 || flags.size() != rows.size() || arr(rows.front()).size() < 2) return false;
    const auto lastY = arr(rows.front()).size() - 1;
    const auto translation = [&](std::size_t x, std::size_t y, V& value) {
        const auto& row = arr(rows[x]);
        const auto& set = arr(flags[x]);
        if (y >= row.size() || y >= set.size() || set[y] != true) return false;
        const auto& offsets = arr(row[y]);
        if (offsets.size() != node.points.size()) return false;
        value = {component(offsets.front(),0),component(offsets.front(),1)};
        return std::ranges::all_of(offsets,[&](const auto& offset) {
            return std::hypot(component(offset,0)-value[0],component(offset,1)-value[1]) < 1e-3;
        });
    };
    V start{}, x{}, y{};
    if (!translation(0,0,start) || !translation(rows.size()-1,0,x) || !translation(0,lastY,y)) return false;
    return std::abs((x[0]-start[0])*(y[1]-start[1])-(x[1]-start[1])*(y[0]-start[0])) > 1e-3;
}
struct GazeGroup {
    std::set<Id> members;
    V pivot{};
};
std::map<Id,GazeGroup> gazeGroups(const Scene& scene, const auto& apertures) {
    std::map<Id,GazeGroup> groups;
    for (const auto& [parameter,p] : scene.parameters) {
        if (at(*p,"is_vec2") != true) continue;
        std::map<Id,GazeGroup> candidates;
        std::map<Id,double> largest;
        for (const auto& binding : arr(at(*p,"bindings"))) {
            const auto target = scene.nodes.find(id(at(binding,"node")));
            if (target == scene.nodes.end() || !target->second.frame.valid) continue;
            const auto& node = target->second;
            for (const auto& edge : arr(at(*node.raw,"masks"))) {
                if (at(edge,"mode") != "Mask") continue;
                const auto support = id(at(edge,"source"));
                if (!apertures.contains(support) || std::ranges::any_of(apertures.at(support),
                    [&](const auto& entry) { return entry.first == parameter; })) continue;
                if (!translatesOnTwoAxes(node,binding)) continue;
                auto& group = candidates[support];
                group.members.insert(target->first);
                const double area = (node.high[0]-node.low[0])*(node.high[1]-node.low[1])*
                    std::abs(node.frame.a*node.frame.d-node.frame.b*node.frame.c);
                if (area <= largest[support]) continue;
                largest[support] = area;
                group.pivot = node.frame.vector(node.center);
                group.pivot[0] += node.frame.x;
                group.pivot[1] += node.frame.y;
            }
        }
        for (auto& [support,candidate] : candidates)
            if (!groups.contains(support) || candidate.members.size() > groups.at(support).members.size())
                groups[support] = std::move(candidate);
    }
    return groups;
}
bool hasControl(const Scene& scene,const std::string& label) {
    for(const auto& [_,p]:scene.parameters)if(at(*p,"structural_key")==Json(label))return true;
    return false;
}
// A moving pendulum anchor turns aperture closing/reopening into a time response.
// All layers driven by the same gaze share a pivot and a single spring output.
void planBlinkPhysics(const Scene& scene, const auto& apertures, const auto& groups,
    const Json& selection, bool manual, A& matches, A& controls, std::set<Id>& handled) {
    std::set<Id> selected;
    for (const auto& item : arr(at(selection,"matches")))
        for (const auto& target : arr(at(at(item,"roles"),"target"))) selected.insert(id(target));
    std::set<Id> existingOutputs;
    for (const auto& [_,node] : scene.nodes)
        if (at(*node.raw,"type") == "SimplePhysics") existingOutputs.insert(id(at(*node.raw,"param")));
    for (const auto& [aperture,group] : groups) {
        if (apertures.at(aperture).size() != 1) continue;
        const auto& support = scene.nodes.at(aperture);
        if (!support.frame.valid || !scene.root.valid) continue;
        const auto [driver,source] = apertures.at(aperture).front();
        const auto& input = *scene.parameters.at(driver);
        const auto xs = physicalAxis(input,0), ys = physicalAxis(input,1);
        const auto& values = arr(at(*source,"values"));
        const auto& flags = arr(at(*source,"isSet"));
        if (xs.size() != values.size() || xs.size() != flags.size() || !zeroAxis(xs) || !zeroAxis(ys)) continue;
        const double width = support.high[0]-support.low[0], height = support.high[1]-support.low[1];
        if (width < 1e-6 || height < 1e-6) continue;
        const auto lengthVector = support.frame.vector({0.,height});
        const double length = std::max(1.,.68*std::hypot(lengthVector[0],lengthVector[1]));
        double direction = 1.;
        for (const auto& [other,otherGroup] : groups) {
            if (other == aperture) continue;
            direction = group.pivot[0] < otherGroup.pivot[0] ? -1. : 1.;
            break;
        }
        auto pendulumAnchor = [&](std::size_t x, std::size_t y) -> Json {
            if (arr(values[x]).size() != ys.size() || arr(flags[x]).size() != ys.size() || arr(flags[x])[y] != true)
                return nullptr;
            const auto& offsets = arr(arr(values[x])[y]);
            if (offsets.size() != support.points.size()) return nullptr;
            double low = support.points.front()[1]+component(offsets.front(),1), high = low;
            for (std::size_t i=1; i<offsets.size(); ++i) {
                const double y = support.points[i][1]+component(offsets[i],1);
                low = std::min(low,y);
                high = std::max(high,y);
            }
            const double closure = std::clamp(1.-(high-low)/height,0.,1.);
            // The reference drives the anchor sideways and reads the
            // resulting horizontal output as scale.
            return direction*2.73*length*closure;
        };
        A keys;
        bool valid = true;
        for (std::size_t x=0; x<xs.size() && valid; ++x) {
            A row;
            for (std::size_t y=0; y<ys.size(); ++y) {
                auto value = pendulumAnchor(x,y);
                if (value.is_null()) { valid=false; break; }
                if (num(xs[x])==0. && num(ys[y])==0.) value=0.;
                row.push_back(std::move(value));
            }
            keys.push_back(std::move(row));
        }
        if (!valid) continue;
        const auto label = "Structural p19/Blink physics/" + std::to_string(aperture);
        handled.insert(group.members.begin(),group.members.end());
        if (hasControl(scene,label)) continue;
        bool alreadyDriven = false;
        for (auto output : existingOutputs) if (scene.parameters.contains(output))
            for (const auto& b : arr(at(*scene.parameters.at(output),"bindings")))
                alreadyDriven |= group.members.contains(id(at(b,"node")));
        if (alreadyDriven) continue;
        const A axis{-1,0,1};
        A bindings;
        for (const auto target : group.members) {
            if (manual && !selected.contains(target)) continue;
            const auto& node=scene.nodes.at(target);
            const auto pivot=node.frame.inverseVector({group.pivot[0]-node.frame.x,group.pivot[1]-node.frame.y});
            bindings.push_back(binding(target,"deform",axis,axis,[&](std::size_t x,std::size_t)->Json {
                const double scale=.04*direction*num(axis[x]);
                A offsets;
                for (const auto& point : node.points)
                    offsets.push_back(A{scale*(point[0]-pivot[0]),scale*(point[1]-pivot[1])});
                return offsets;
            }));
            auto selected = match(target);
            selected["evidence"].as_array().push_back("blink_driven_pendulum_scale");
            matches.push_back(std::move(selected));
        }
        if (bindings.empty()) continue;
        auto planned = control(label,bindings,axis,axis);
        planned["blink_physics"] = O{{"driver",driver},{"anchor_x",keys},{"length",length}};
        controls.push_back(std::move(planned));
    }
}
const char* prerequisite(std::string_view p) {
    static const std::map<std::string_view,const char*> reasons{
        {"p01","source motion and source-to-target correspondence; existing motion is not a request to duplicate it"},
        {"p02","source/destination input axes and coupling policy"},
        {"p03","cross-input constraint or example poses defining a residual"},
        {"p04","attachment intent and surface correspondence"},
        {"p05","composition frame and local transform policy"},
        {"p06","supported centerline, pivot and bend/twist range"},
        {"p07","fixed/free boundaries and front/back surface assignment"},
        {"p08","explicit clip relationship; geometric containment alone is ambiguous"},
        {"p09","explicit exclusion relationship or residual-penetration evidence"},
        {"p10","explicit mask edges and their modes"},
        {"p11","PSD clipping group and lower-alpha composition scope"},
        {"p12","front/back surfaces and pose-dependent order policy"},
        {"p13","separate light/shadow layer, supporting surface and angle driver"},
        {"p14","lighting layer, supporting surface and light direction policy"},
        {"p15","material state or blend policy"},
        {"p16","confirmed seam and boundary correspondence; contact alone is not a seam"},
        {"p17","aperture boundary and closure frame; an existing blink is not duplicated"},
        {"p18","masked content and sampled aperture collapse with a nonzero residual"},
        {"p19","nested mask content in a confirmed aperture"},
        {"p20","confirmed aperture and an independent width control"},
        {"p21","alternate artwork and exclusive state policy"},
        {"p22","joint pivot, local frame and rotation range"},
        {"p23","branch roots and joint frames"},
        {"p24","supported volume and expansion/period policy"},
        {"p25","fixed/free volume regions and material policy"},
        {"p26","confirmed rigid suspended region and attachment pivot"},
        {"p27","knot, loops, free ends and crossing connectivity"},
        {"p28","confirmed fixed anchors, local frame and endpoint motions"},
        {"p29","independent mode fields and their input drivers"},
        {"p30","physics node and moving support frame"},
        {"p31","physics node and coefficient response policy"},
        {"p32","local correction region and displacement intent"},
        {"p33","desired propagation property and scope boundary"},
        {"p34","protected/free region assignment and motion policy"},
        {"p35","independent group and translation policy"}};
    return reasons.at(p);
}
}

Json ngPlanPattern(std::string_view pattern,const Json& document,const Json& selection,Checkpoint checkpoint) {
    Scene scene;scene.read(at(document,"nodes"));scene.readParameters(at(document,"param"));
    A matches,controls,missing;
    O recipe{{"_planned",true}};
    const bool manual=at(selection,"mode")=="manual" || at(selection,"restricted_selection")==true;
    // Static explicit mask edges can be retained without inventing new animation.
    // Preserve mixed Mask/DodgeMask networks rather than converting every edge to Mask.
    if(pattern=="p08" || pattern=="p09" || pattern=="p10") {
        for(const auto& m:arr(at(selection,"matches"))) {
            bool compatible=true;
            for(const auto& t:arr(at(at(m,"roles"),"target")))for(const auto& s:arr(at(at(m,"roles"),"mask"))) {
                bool found=false;
                for(const auto& edge:arr(at(*scene.nodes.at(id(t)).raw,"masks")))
                    found |= at(edge,"source")==s && at(edge,"mode")== (pattern=="p09"?"DodgeMask":"Mask");
                compatible &= found || manual;
            }
            if(compatible)matches.push_back(m);
        }
    }
    if(pattern=="p18" || pattern=="p19" || pattern=="p20") {
        std::map<Id,std::vector<std::pair<Id,const Json*>>> apertures;
        for(const auto& [pid,p]:scene.parameters)for(const auto& b:arr(at(*p,"bindings"))) {
            auto target=id(at(b,"node"));
            if(scene.nodes.contains(target) && collapsing(scene.nodes.at(target),b))apertures[target].push_back({pid,&b});
        }
        const auto eyeGroups = pattern == "p19" ? gazeGroups(scene,apertures) : std::map<Id,GazeGroup>{};
        std::set<Id> springMembers;
        if (pattern=="p19") planBlinkPhysics(scene,apertures,eyeGroups,selection,manual,matches,controls,springMembers);
        for(const auto& [uuid,n]:scene.nodes) {
            if(checkpoint)checkpoint();
            if (springMembers.contains(uuid)) continue;
            if(at(*n.raw,"type")!="Part" || n.points.empty() || !n.frame.valid)continue;
            // The manual selection is an exact replacement, including an empty one.
            if(manual) {
                bool selected=false;
                for(const auto& m:arr(at(selection,"matches")))for(const auto& t:arr(at(at(m,"roles"),"target")))selected |= id(t)==uuid;
                if(!selected)continue;
            }
            std::vector<Id> sources;
            for(const auto& edge:arr(at(*n.raw,"masks")))
                if(at(edge,"mode")=="Mask" && scene.nodes.contains(id(at(edge,"source"))))sources.push_back(id(at(edge,"source")));
            if(manual && pattern=="p18") {
                sources.clear();
                for(const auto& m:arr(at(selection,"matches"))) {
                    bool selected=false;
                    for(const auto& t:arr(at(at(m,"roles"),"target")))selected |= id(t)==uuid;
                    if(selected)for(const auto& s:arr(at(at(m,"roles"),"support")))sources.push_back(id(s));
                }
            }
            if(pattern=="p20" && apertures.contains(uuid))sources={uuid};
            Id aperture=0;bool nested=false;
            for(auto source:sources) {
                if(apertures.contains(source))aperture=source;
                else for(const auto& edge:arr(at(*scene.nodes.at(source).raw,"masks")))
                    if(at(edge,"mode")=="Mask" && apertures.contains(id(at(edge,"source")))) {
                        aperture=id(at(edge,"source"));nested=true;
                    }
            }
            if(!aperture && manual && (pattern=="p19" || pattern=="p20")) {aperture=uuid;nested=true;}
            if(!aperture)continue;
            if(pattern=="p19" && !nested) {
                const auto& support=scene.nodes.at(aperture);
                double w=n.high[0]-n.low[0],h=n.high[1]-n.low[1];
                double sw=support.high[0]-support.low[0],sh=support.high[1]-support.low[1];
                double area=w*h*std::abs(n.frame.a*n.frame.d-n.frame.b*n.frame.c);
                double supportArea=sw*sh*std::abs(support.frame.a*support.frame.d-support.frame.b*support.frame.c);
                // Compact internal material is a structural subtype; this does not
                // claim semantic eye recognition or a time-domain physics simulation.
                if(std::min(w,h)<1e-6 || std::max(w,h)/std::min(w,h)>1.8 || area>supportArea*0.65)continue;
            }
            const auto label="Structural "+std::string(pattern)+"/"+std::to_string(uuid);
            if(hasControl(scene,label))continue;
            A bindings;
            if(pattern=="p18") {
                const auto& support=scene.nodes.at(aperture);
                if(!support.frame.valid || apertures.at(aperture).size()!=1)continue;
                auto [pid,source]=apertures.at(aperture).front();
                const auto& p=*scene.parameters.at(pid);
                auto xs=physicalAxis(p,0),ys=physicalAxis(p,1);
                if(!zeroAxis(xs) || !zeroAxis(ys))continue;
                if(std::ranges::any_of(arr(at(p,"bindings")), [&](const auto& existing) {
                    return id(at(existing,"node"))==uuid && at(existing,"param_name")=="deform";
                }))continue;
                const auto& rows=arr(at(*source,"values"));const auto& flags=arr(at(*source,"isSet"));
                if(rows.empty())continue;
                bool valid=true,nonzero=false;
                auto sample=[&](std::size_t x,std::size_t y)->Json {
                    std::size_t ix=rows.size()==1?0:x;
                    if(ix>=rows.size() || ix>=flags.size()){valid=false;return A{};}
                    const auto& row=arr(rows[ix]);std::size_t iy=row.size()==1?0:y;
                    if(iy>=row.size() || iy>=arr(flags[ix]).size() || flags[ix].as_array()[iy]!=true){valid=false;return A{};}
                    const auto& offsets=arr(row[iy]);
                    if(offsets.size()!=support.points.size()){valid=false;return A{};}
                    V low{},high{};
                    for(std::size_t i=0;i<offsets.size();++i)for(int d=0;d<2;++d) {
                        double v=support.points[i][d]+component(offsets[i],d);
                        if(!i)low[d]=high[d]=v;else {low[d]=std::min(low[d],v);high[d]=std::max(high[d],v);}
                    }
                    V delta{(low[0]+high[0])/2-support.center[0],(low[1]+high[1])/2-support.center[1]};
                    delta=n.frame.inverseVector(support.frame.vector(delta));
                    // Preserve the material shape. Add only centroid motion not already
                    // provided by the same driver on this target.
                    for(const auto& existing:arr(at(p,"bindings")))if(id(at(existing,"node"))==uuid) {
                        const auto& er=arr(at(existing,"values"));
                        if(ix>=er.size() || iy>=arr(er[ix]).size()) {valid=false;continue;}
                        const auto& cell=er[ix].as_array()[iy];
                        if(at(existing,"param_name")=="deform" && arr(cell).size()==n.points.size()) {
                            V mean{};for(const auto& v:arr(cell))for(int d=0;d<2;++d)mean[d]+=component(v,d)/n.points.size();
                            for(int d=0;d<2;++d)delta[d]-=mean[d];
                        } else if(at(existing,"param_name")=="transform.t.x")delta[0]-=num(cell);
                        else if(at(existing,"param_name")=="transform.t.y")delta[1]-=num(cell);
                    }
                    if(std::abs(num(xs[x]))<1e-9 && std::abs(num(ys[y]))<1e-9) {
                        if(std::hypot(delta[0],delta[1])>1e-5)valid=false;
                        delta={0,0};
                    }
                    nonzero |= std::hypot(delta[0],delta[1])>1e-5;
                    A result;
                    for (std::size_t i=0; i<n.points.size(); ++i)
                        result.push_back(A{delta[0],delta[1]});
                    return result;
                };
                bindings.push_back(binding(uuid,"deform",xs,ys,sample));
                if(!valid || !nonzero)continue;
                auto c=control(label,bindings,xs,ys);c["driver_parameter"]=pid;controls.push_back(c);
                matches.push_back(match(uuid,"support",aperture));
            } else {
                A axis{-1,0,1};
                const double extent=std::min(n.high[0]-n.low[0],n.high[1]-n.low[1]);
                if(extent<1e-6)continue;
                bindings.push_back(binding(uuid,"deform",axis,axis,[&](std::size_t x,std::size_t y)->Json {
                    A offsets;
                    for(auto v:n.points)offsets.push_back(pattern=="p19"?
                        A{0.015*extent*num(axis[x]),0.015*extent*num(axis[y])}:
                        A{0.05*(v[0]-n.center[0])*num(axis[x]),0});
                    return offsets;
                }));
                controls.push_back(control(label,bindings));matches.push_back(match(uuid));
            }
        }
    }
    if(pattern=="p13" || pattern=="p14") {
        for(const auto& [uuid,n]:scene.nodes) {
            if(checkpoint)checkpoint();
            if(at(*n.raw,"type")!="Part" || n.points.empty() || !n.frame.valid)continue;
            const auto& blend=at(*n.raw,"blend_mode");
            if(!manual && blend!="Multiply" && blend!="Screen" && blend!="LinearDodge" && blend!="AddGlow" && blend!="ColorDodge")continue;
            std::set<Id> surfaces;
            if(manual) {
                bool selected=false;
                for(const auto& m:arr(at(selection,"matches"))) {
                    bool target=false;
                    for(const auto& t:arr(at(at(m,"roles"),"target")))target |= id(t)==uuid;
                    selected |= target;
                    if(target)for(const auto& s:arr(at(at(m,"roles"),"surface")))surfaces.insert(id(s));
                }
                if(!selected)continue;
            }
            if(surfaces.empty())for(const auto& edge:arr(at(*n.raw,"masks")))
                if(at(edge,"mode")=="Mask")surfaces.insert(id(at(edge,"source")));
            if(manual && id(at(at(selection,"intent"),"surface_node")))
                surfaces={id(at(at(selection,"intent"),"surface_node"))};
            if(surfaces.size()!=1 || !scene.nodes.contains(*surfaces.begin()))continue;
            Id surface=*surfaces.begin(),driver=0;
            double best=0;
            // Select a measured spatial driver of the supporting surface. A blink
            // and an existing opacity control are not treated as an angle signal.
            for(const auto& [pid,p]:scene.parameters) {
                bool alreadyDriven=false;
                double score=0;
                for(const auto& b:arr(at(*p,"bindings"))) {
                    if(id(at(b,"node"))==uuid && at(b,"param_name")=="opacity")alreadyDriven=true;
                    if(id(at(b,"node"))!=surface || at(b,"param_name")!="deform" || collapsing(scene.nodes.at(surface),b))continue;
                    for(const auto& row:arr(at(b,"values")))for(const auto& cell:arr(row)) {
                        double sum=0;
                        for(const auto& v:arr(cell))sum+=std::hypot(component(v,0),component(v,1));
                        if(!arr(cell).empty())score=std::max(score,sum/arr(cell).size());
                    }
                }
                if(!alreadyDriven && score>best) {best=score;driver=pid;}
            }
            if(manual && scene.parameters.contains(id(at(at(selection,"intent"),"driver_parameter")))) {
                driver=id(at(at(selection,"intent"),"driver_parameter"));best=1;
            }
            const auto label="Structural "+std::string(pattern)+"/"+std::to_string(uuid);
            if(hasControl(scene,label))continue;
            if(best<1e-5)driver=0;
            auto xs=driver?physicalAxis(*scene.parameters.at(driver),0):A{-1,0,1};
            auto ys=driver?physicalAxis(*scene.parameters.at(driver),1):A{-1,0,1};
            if(!zeroAxis(xs) || !zeroAxis(ys))continue;
            double range=0;for(const auto& v:xs)range=std::max(range,std::abs(num(v)));
            if(range<1e-6)continue;
            A bindings;
            if(pattern=="p13")bindings.push_back(binding(uuid,"opacity",xs,ys,[&](std::size_t x,std::size_t)->Json {
                // Opacity bindings multiply static layer opacity. Neutral must be one.
                return 1.0-0.15*std::abs(num(xs[x]))/range;
            }));
            else bindings.push_back(binding(uuid,"deform",xs,ys,[&](std::size_t x,std::size_t)->Json {
                A offsets;const double delta=0.01*(n.high[0]-n.low[0])*num(xs[x])/range;
                for(std::size_t i=0;i<n.points.size();++i)offsets.push_back(A{delta,0});return offsets;
            }));
            auto c=control(label,bindings,xs,ys);
            if(driver)c["driver_parameter"]=driver;else c["driver_surface"]=surface;
            controls.push_back(c);
            auto m=pattern=="p13"?match(uuid):match(uuid,"surface",surface);
            m["evidence"]=A{"lighting_blend", "explicit_mask_surface",
                driver?"measured_surface_spatial_driver":"new_shared_surface_lighting_input"};
            matches.push_back(m);
        }
    }
    // A confirmed role assignment is also a usable input for unrigged PSD meshes.
    // Generate local frames, amplitudes and protected weights instead of requiring
    // callers to hand-author all samples after selecting the Part/Node roles.
    const std::set<std::string_view> geometric{"p03","p06","p07","p17","p22","p23","p24","p25","p26","p27","p29","p32","p34","p35"};
    if(geometric.contains(pattern))for(const auto& m:arr(at(selection,"matches"))) {
        if(!manual && at(m,"provenance")!="structure")continue;
        for(const auto& t:arr(at(at(m,"roles"),"target"))) {
            Id uuid=id(t);const auto& n=scene.nodes.at(uuid);
            if(at(*n.raw,"type")!="Part" || n.points.size()<3 || !n.frame.valid)continue;
            const auto label="Structural "+std::string(pattern)+"/"+std::to_string(uuid);
            if(hasControl(scene,label))continue;
            V center{};
            for(auto p:n.points)for(int d=0;d<2;++d)center[d]+=p[d]/n.points.size();
            double xx=0,xy=0,yy=0;
            for(auto p:n.points) {double x=p[0]-center[0],y=p[1]-center[1];xx+=x*x;xy+=x*y;yy+=y*y;}
            const double angle=.5*std::atan2(2*xy,xx-yy);
            V direction{std::cos(angle),std::sin(angle)};
            double low=1e100,high=-1e100;
            for(auto p:n.points) {double u=(p[0]-center[0])*direction[0]+(p[1]-center[1])*direction[1];low=std::min(low,u);high=std::max(high,u);}
            const double extent=high-low;
            if(extent<1e-6)continue;
            V pivot{center[0]+low*direction[0],center[1]+low*direction[1]};
            const auto& supports=arr(at(at(m,"roles"),"support"));
            if(!supports.empty()) {
                const auto& support=scene.nodes.at(id(supports.front()));
                if(!support.frame.valid)continue;
                const auto origin=at(at(*n.raw,"mesh"),"origin");
                const auto supportOrigin=at(at(*support.raw,"mesh"),"origin");
                V sp{support.center[0]-component(supportOrigin,0),support.center[1]-component(supportOrigin,1)};
                sp=support.frame.vector(sp);sp[0]+=support.frame.x-n.frame.x;sp[1]+=support.frame.y-n.frame.y;
                sp=n.frame.inverseVector(sp);sp[0]+=component(origin,0);sp[1]+=component(origin,1);
                pivot=*std::min_element(n.points.begin(),n.points.end(),[&](V a,V b) {
                    return std::hypot(a[0]-sp[0],a[1]-sp[1])<std::hypot(b[0]-sp[0],b[1]-sp[1]);
                });
                if((center[0]-pivot[0])*direction[0]+(center[1]-pivot[1])*direction[1]<0)
                    for(auto& d:direction)d=-d;
            }
            const bool rotate=pattern=="p06" || pattern=="p22" || pattern=="p23" || pattern=="p26" || pattern=="p27";
            const bool protect=pattern=="p07" || pattern=="p25" || pattern=="p34";
            A xs=pattern=="p17"?A{0,0.5,1}:A{-1,0,1},ys{-1,0,1};
            auto field=binding(uuid,"deform",xs,ys,[&](std::size_t x,std::size_t y)->Json {
                A offsets;
                for(auto p:n.points) {
                    double dx=0,dy=0;
                    const double progress=std::clamp(((p[0]-pivot[0])*direction[0]+(p[1]-pivot[1])*direction[1])/extent,0.,1.);
                    const double weight=protect?progress*progress*(3-2*progress):1;
                    if(rotate) {
                        double r=.08*num(xs[x])*(pattern=="p06"?progress:1.);
                        double px=p[0]-pivot[0],py=p[1]-pivot[1];
                        dx=std::cos(r)*px-std::sin(r)*py-px;dy=std::sin(r)*px+std::cos(r)*py-py;
                    } else if(pattern=="p17") {
                        double u=(p[0]-center[0])*direction[0]+(p[1]-center[1])*direction[1];
                        double v=-(p[0]-center[0])*direction[1]+(p[1]-center[1])*direction[0];
                        double closure=-v*num(xs[x])*std::max(0.,1-4*u*u/(extent*extent));
                        dx=-direction[1]*closure;dy=direction[0]*closure;
                    } else if(pattern=="p24" || pattern=="p25") {
                        dx=.05*num(xs[x])*(p[0]-center[0]);dy=.05*num(xs[x])*(p[1]-center[1]);
                    } else if(pattern=="p03") {dx=.02*extent*num(xs[x])*num(ys[y]);}
                    else if(pattern=="p29") {
                        // Independent translation and angular modes, with no invented
                        // time driver or claim of restoring original physics constants.
                        dx=.02*extent*num(xs[x])-.05*(p[1]-pivot[1])*num(ys[y]);
                        dy=.05*(p[0]-pivot[0])*num(ys[y]);
                    }
                    else {dx=.02*extent*num(xs[x]);dy=.02*extent*num(ys[y]);}
                    offsets.push_back(A{weight*dx,weight*dy});
                }
                return offsets;
            });
            bool varies=false;
            for(const auto& row:arr(at(field,"values")))for(const auto& cell:arr(row))for(const auto& v:arr(cell))
                varies |= std::hypot(component(v,0),component(v,1))>1e-8;
            if(!varies)continue;
            controls.push_back(control(label,A{field},xs,ys));
            auto selected=m;selected.as_object().at("roles").as_object()["target"]=A{uuid};matches.push_back(selected);
        }
    }
    if(pattern=="p16") {
        A links;std::set<std::pair<Id,Id>> pairs;
        for(const auto& m:arr(at(selection,"matches"))) {
            if(!manual && at(m,"provenance")!="structure")continue;
            for(const auto& t:arr(at(at(m,"roles"),"target")))for(const auto& s:arr(at(at(m,"roles"),"seam"))) {
                Id a=id(t),b=id(s);if(!pairs.insert({std::min(a,b),std::max(a,b)}).second)continue;
                const auto& first=scene.nodes.at(a);const auto& second=scene.nodes.at(b);
                if(first.points.empty() || second.points.empty() || !first.frame.valid || !second.frame.valid)continue;
                auto boundary=[](const Node& node) {
                    std::map<std::pair<std::size_t,std::size_t>,int> edges;
                    const auto& indices=arr(at(at(*node.raw,"mesh"),"indices"));
                    for(std::size_t i=0;i+2<indices.size();i+=3)for(int k=0;k<3;++k) {
                        auto a=static_cast<std::size_t>(num(indices[i+k])),b=static_cast<std::size_t>(num(indices[i+(k+1)%3]));
                        if(a<node.points.size() && b<node.points.size())++edges[{std::min(a,b),std::max(a,b)}];
                    }
                    std::set<std::size_t> result;
                    for(const auto& [edge,count]:edges)if(count==1){result.insert(edge.first);result.insert(edge.second);}
                    return result;
                };
                auto world=[](const Node& node,std::size_t i) {
                    auto v=node.points[i];const auto& origin=at(at(*node.raw,"mesh"),"origin");
                    for(int d=0;d<2;++d)v[d]-=component(origin,d);v=node.frame.vector(v);
                    v[0]+=node.frame.x;v[1]+=node.frame.y;return v;
                };
                const auto firstBoundary=boundary(first),secondBoundary=boundary(second);
                auto diagonal=[&](const Node& node) {
                    auto delta=node.frame.vector({node.high[0]-node.low[0],node.high[1]-node.low[1]});return std::hypot(delta[0],delta[1]);
                };
                const double tolerance=.015*std::min(diagonal(first),diagonal(second));
                A indices;for(std::size_t i=0;i<first.points.size();++i)indices.push_back(-1);
                std::set<std::size_t> used;std::size_t count=0;
                for(auto i:firstBoundary) {
                    auto p=world(first,i);double best=tolerance;std::size_t partner=second.points.size();
                    for(auto j:secondBoundary)if(!used.contains(j)) {
                        auto q=world(second,j);double distance=std::hypot(p[0]-q[0],p[1]-q[1]);
                        if(distance<best){best=distance;partner=j;}
                    }
                    if(partner!=second.points.size()){indices[i]=partner;used.insert(partner);++count;}
                }
                if(count<2)continue;
                links.push_back(O{{"source",a},{"target",b},{"indices",indices},{"weight",.5}});
                matches.push_back(match(a,"seam",b));
            }
        }
        if(!links.empty())recipe["links"]=links;
    }
    if(manual && (pattern=="p11" || pattern=="p15" || pattern=="p33")) {
        const auto& intent=at(selection,"intent");
        if(pattern=="p33") {
            // Choosing a scope does not say whether propagation should be enabled.
            if(at(intent,"property").is_string() && at(intent,"value").is_bool()) {
                recipe["property"]=at(intent,"property");recipe["value"]=at(intent,"value");
                matches=arr(at(selection,"matches"));
            }
        } else if(pattern=="p11" || at(intent,"blend_mode").is_string()) {
            recipe["blend_mode"]=pattern=="p11"?Json("ClipToLower"):at(intent,"blend_mode");
            matches=arr(at(selection,"matches"));
        }
    }
    if(manual && (pattern=="p05" || pattern=="p12" || pattern=="p21" || pattern=="p30" || pattern=="p31")) {
        const auto& intent=at(selection,"intent");
        for(const auto& m:arr(at(selection,"matches")))for(const auto& t:arr(at(at(m,"roles"),"target"))) {
            Id uuid=id(t);const auto& n=scene.nodes.at(uuid);
            const auto label="Structural "+std::string(pattern)+"/"+std::to_string(uuid);
            if(hasControl(scene,label))continue;
            std::string property;
            double neutral=0,gain=0;
            A xs{-1,0,1},ys{-1,0,1};
            if(pattern=="p05") {property="transform.r.z";gain=.08;}
            if(pattern=="p12") {
                if(!at(intent,"gain_x").is_number())continue;
                property="zSort";gain=num(at(intent,"gain_x"));
            }
            if(pattern=="p21") {property="opacity";neutral=1;gain=-1;xs=A{0,1};}
            if(pattern=="p30") {
                if(!at(intent,"property").is_string() || !at(intent,"gain_x").is_number())continue;
                property=std::string(at(intent,"property").as_string());gain=num(at(intent,"gain_x"));
            }
            if(pattern=="p31") {
                if(at(*n.raw,"type")!="SimplePhysics" || !at(intent,"property").is_string() || !at(intent,"gain_x").is_number())continue;
                property=std::string(at(intent,"property").as_string());gain=num(at(intent,"gain_x"));
                // Engine physics properties are multiplicative offsets except for
                // explicit additive properties, which require a supplied neutral.
                neutral=num(at(intent,"neutral"),1);
            }
            auto b=binding(uuid,property,xs,ys,[&](std::size_t x,std::size_t)->Json{return neutral+gain*num(xs[x]);});
            controls.push_back(control(label,A{b},xs,ys));
            auto selected=m;selected.as_object().at("roles").as_object()["target"]=A{uuid};matches.push_back(selected);
        }
    }
    if(manual && pattern=="p02") {
        const auto& intent=at(selection,"intent");
        if(at(intent,"source_parameter").is_number() && at(intent,"destination_parameter").is_number() &&
            at(intent,"input_axis").is_string() && at(intent,"output_axis").is_string()) {
            O c{{"label","Structural p02"},{"x",A{-1,0,1}},{"y",A{-1,0,1}},
                {"source_parameter",at(intent,"source_parameter")},{"destination_parameter",at(intent,"destination_parameter")},
                {"input_axis",at(intent,"input_axis")},{"output_axis",at(intent,"output_axis")},
                {"gain",num(at(intent,"gain"),1)},{"bias",num(at(intent,"bias"))}};
            controls.push_back(c);matches=arr(at(selection,"matches"));
        }
    }
    if(pattern=="p04" || (manual && pattern=="p01"))for(const auto& m:arr(at(selection,"matches"))) {
        if(!manual && at(m,"provenance")!="structure")continue;
        const auto& surfaces=arr(at(at(m,"roles"),"surface"));
        Id surface=pattern=="p01"?id(at(at(selection,"intent"),"source_node")):
            surfaces.size()==1?id(surfaces.front()):0;
        if(!scene.nodes.contains(surface))continue;
        const auto& support=scene.nodes.at(surface);
        if(!support.frame.valid || support.points.empty())continue;
        for(const auto& t:arr(at(at(m,"roles"),"target"))) {
            Id uuid=id(t);const auto& n=scene.nodes.at(uuid);
            if(uuid==surface || !n.frame.valid || n.points.empty())continue;
            struct Correspondence {std::array<std::size_t,3> vertices;std::array<double,3> weights;};
            std::vector<Correspondence> mapping;
            const auto& indices=arr(at(at(*support.raw,"mesh"),"indices"));
            for(auto v:n.points) {
                const auto& origin=at(at(*n.raw,"mesh"),"origin");
                for(int d=0;d<2;++d)v[d]-=component(origin,d);
                v=n.frame.vector(v);v[0]+=n.frame.x-support.frame.x;v[1]+=n.frame.y-support.frame.y;
                v=support.frame.inverseVector(v);
                const auto& otherOrigin=at(at(*support.raw,"mesh"),"origin");
                for(int d=0;d<2;++d)v[d]+=component(otherOrigin,d);
                bool found=false;
                for(std::size_t i=0;i+2<indices.size();i+=3) {
                    std::array<std::size_t,3> ids{static_cast<std::size_t>(num(indices[i])),static_cast<std::size_t>(num(indices[i+1])),static_cast<std::size_t>(num(indices[i+2]))};
                    if(std::any_of(ids.begin(),ids.end(),[&](auto id){return id>=support.points.size();}))continue;
                    auto a=support.points[ids[0]],b=support.points[ids[1]],c=support.points[ids[2]];
                    double denominator=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
                    if(std::abs(denominator)<1e-10)continue;
                    double u=((b[1]-c[1])*(v[0]-c[0])+(c[0]-b[0])*(v[1]-c[1]))/denominator;
                    double w=((c[1]-a[1])*(v[0]-c[0])+(a[0]-c[0])*(v[1]-c[1]))/denominator;
                    if(u>=-1e-7 && w>=-1e-7 && u+w<=1+1e-7) {
                        mapping.push_back({ids,{u,w,1-u-w}});found=true;break;
                    }
                }
                if(!found)break;
            }
            if(mapping.size()!=n.points.size())continue;
            for(const auto& [pid,p]:scene.parameters) {
                Id requested=id(at(at(selection,"intent"),"source_parameter"));
                if(requested && requested!=pid)continue;
                bool already=false;
                for(const auto& b:arr(at(*p,"bindings")))already |= id(at(b,"node"))==uuid && at(b,"param_name")=="deform";
                if(already)continue;
                const auto label="Structural "+std::string(pattern)+"/"+std::to_string(uuid)+"/"+std::to_string(pid);
                if(hasControl(scene,label))continue;
                for(const auto& b:arr(at(*p,"bindings"))) {
                    if(id(at(b,"node"))!=surface || at(b,"param_name")!="deform")continue;
                    auto xs=physicalAxis(*p,0),ys=physicalAxis(*p,1);
                    if(!zeroAxis(xs) || !zeroAxis(ys))continue;
                    const auto& rows=arr(at(b,"values"));const auto& flags=arr(at(b,"isSet"));
                    bool valid=true,varies=false;
                    auto field=binding(uuid,"deform",xs,ys,[&](std::size_t x,std::size_t y)->Json {
                        auto ix=rows.size()==1?0:x;
                        if(ix>=rows.size() || ix>=flags.size()){valid=false;return A{};}
                        auto iy=arr(rows[ix]).size()==1?0:y;
                        if(iy>=arr(rows[ix]).size() || iy>=arr(flags[ix]).size() || flags[ix].as_array()[iy]!=true){valid=false;return A{};}
                        const auto& offsets=arr(rows[ix].as_array()[iy]);
                        if(offsets.size()!=support.points.size()){valid=false;return A{};}
                        A result;
                        for(const auto& correspondence:mapping) {
                            V delta{};
                            for(int k=0;k<3;++k)for(int d=0;d<2;++d)
                                delta[d]+=correspondence.weights[k]*component(offsets[correspondence.vertices[k]],d);
                            delta=n.frame.inverseVector(support.frame.vector(delta));
                            if(std::abs(num(xs[x]))<1e-9 && std::abs(num(ys[y]))<1e-9 && std::hypot(delta[0],delta[1])>1e-6)valid=false;
                            varies |= std::hypot(delta[0],delta[1])>1e-6;result.push_back(A{delta[0],delta[1]});
                        }
                        return result;
                    });
                    if(!valid || !varies)continue;
                    auto c=control(label,A{field},xs,ys);c["driver_parameter"]=pid;controls.push_back(c);
                    auto selected=m;selected.as_object().at("roles").as_object()["target"]=A{uuid};
                    if(std::find(matches.begin(),matches.end(),selected)==matches.end())matches.push_back(selected);
                }
            }
        }
    }
    if(manual && pattern=="p28")for(const auto& m:arr(at(selection,"matches"))) {
        const auto& supports=arr(at(at(m,"roles"),"support"));
        if(supports.size()<2)continue;
        for(const auto& t:arr(at(at(m,"roles"),"target"))) {
            Id uuid=id(t);const auto& n=scene.nodes.at(uuid);
            if(n.points.empty() || !n.frame.valid)continue;
            std::vector<V> anchors;
            bool framesValid=true;
            for(const auto& s:supports) {
                const auto& support=scene.nodes.at(id(s));
                if(!support.frame.valid){framesValid=false;break;}
                V p=support.center;
                const auto& origin=at(at(*support.raw,"mesh"),"origin");
                for(int d=0;d<2;++d)p[d]-=component(origin,d);
                p=support.frame.vector(p);p[0]+=support.frame.x-n.frame.x;p[1]+=support.frame.y-n.frame.y;
                p=n.frame.inverseVector(p);
                const auto& targetOrigin=at(at(*n.raw,"mesh"),"origin");
                for(int d=0;d<2;++d)p[d]+=component(targetOrigin,d);
                anchors.push_back(*std::min_element(n.points.begin(),n.points.end(),[&](V a,V b) {
                    return std::hypot(a[0]-p[0],a[1]-p[1])<std::hypot(b[0]-p[0],b[1]-p[1]);
                }));
            }
            if(!framesValid)continue;
            bool distinct=true;
            for(std::size_t i=0;i<anchors.size();++i)for(std::size_t j=0;j<i;++j)
                distinct &= std::hypot(anchors[i][0]-anchors[j][0],anchors[i][1]-anchors[j][1])>1e-6;
            if(!distinct)continue;
            for(const auto& [pid,p]:scene.parameters) {
                const auto label="Structural p28/"+std::to_string(uuid)+"/"+std::to_string(pid);
                if(hasControl(scene,label))continue;
                bool targetDriven=false,sourceDriven=false;
                for(const auto& b:arr(at(*p,"bindings"))) {
                    if(id(at(b,"node"))==uuid)targetDriven=true;
                    for(const auto& s:supports)sourceDriven |= at(b,"node")==s && at(b,"param_name")=="deform";
                }
                if(targetDriven || !sourceDriven)continue;
                auto xs=physicalAxis(*p,0),ys=physicalAxis(*p,1);
                if(!zeroAxis(xs) || !zeroAxis(ys))continue;
                bool valid=true,varies=false;
                auto field=binding(uuid,"deform",xs,ys,[&](std::size_t x,std::size_t y)->Json {
                    std::vector<V> motions(supports.size());
                    for(std::size_t k=0;k<supports.size();++k)for(const auto& b:arr(at(*p,"bindings"))) {
                        if(at(b,"node")!=supports[k] || at(b,"param_name")!="deform")continue;
                        const auto& rows=arr(at(b,"values"));const auto& flags=arr(at(b,"isSet"));
                        auto ix=rows.size()==1?0:x;
                        if(ix>=rows.size() || ix>=flags.size()){valid=false;continue;}
                        auto iy=arr(rows[ix]).size()==1?0:y;
                        if(iy>=arr(rows[ix]).size() || iy>=arr(flags[ix]).size() || flags[ix].as_array()[iy]!=true){valid=false;continue;}
                        const auto& offsets=arr(rows[ix].as_array()[iy]);
                        const auto& support=scene.nodes.at(id(supports[k]));
                        if(offsets.size()!=support.points.size() || offsets.empty()){valid=false;continue;}
                        V mean{};for(const auto& v:offsets)for(int d=0;d<2;++d)mean[d]+=component(v,d)/offsets.size();
                        motions[k]=n.frame.inverseVector(support.frame.vector(mean));
                    }
                    A offsets;
                    for(auto v:n.points) {
                        double sum=0;V delta{};
                        for(std::size_t k=0;k<anchors.size();++k) {
                            double distance=std::hypot(v[0]-anchors[k][0],v[1]-anchors[k][1]);
                            double weight=1/std::max(1e-12,distance*distance);sum+=weight;
                            for(int d=0;d<2;++d)delta[d]+=weight*motions[k][d];
                        }
                        for(auto& d:delta)d/=sum;
                        if(std::abs(num(xs[x]))<1e-9 && std::abs(num(ys[y]))<1e-9 && std::hypot(delta[0],delta[1])>1e-6)valid=false;
                        varies |= std::hypot(delta[0],delta[1])>1e-6;offsets.push_back(A{delta[0],delta[1]});
                    }
                    return offsets;
                });
                if(!valid || !varies)continue;
                auto c=control(label,A{field},xs,ys);c["driver_parameter"]=pid;controls.push_back(c);
                auto selected=m;selected.as_object().at("roles").as_object()["target"]=A{uuid};
                if(std::find(matches.begin(),matches.end(),selected)==matches.end())matches.push_back(selected);
            }
        }
    }
    if(!controls.empty())recipe["controls"]=controls;
    const bool ready=!matches.empty();
    return O{{"schema","autorig.pattern-plan/1"},{"status",ready?"ready":"needs_input"},
        {"policy","structural-defaults-v1"},{"matches",matches},{"recipe",recipe},
        {"reason",ready?"":prerequisite(pattern)},
        {"source","current_imported_document"},{"reference_rig_used",false},
        {"assumptions",A{"aperture follower preserves material shape", "independent width range is +/-5 percent",
            "lighting profile: 15 percent opacity attenuation and 1 percent layer-width displacement at driver extremes; not recovered author curves",
            "nested material displacement range is +/-1.5 percent of its short side; no time driver is created"}}};
}
}
