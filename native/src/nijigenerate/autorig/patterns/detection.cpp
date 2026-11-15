#include "detection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace nijigenerate::autorig::patterns {
namespace {
using O = boost::json::object;
using A = boost::json::array;
using Id = std::uint32_t;
using V = std::array<double,2>;
const Json null;
const Json& at(const Json& v, std::string_view k) {
    if (v.is_object()) if (auto p = v.as_object().if_contains(k)) return *p;
    return null;
}
const A& arr(const Json& v) { static const A empty; return v.is_array() ? v.as_array() : empty; }
double num(const Json& v, double fallback = 0) {
    if (!v.is_number()) return fallback;
    double n = v.to_number<double>();
    return std::isfinite(n) ? n : fallback;
}
Id uuid(const Json& v) {
    double n = num(v);
    return n > 0 && n < 4294967295. && std::floor(n) == n ? static_cast<Id>(n) : 0;
}
std::string str(const Json& v) { return v.is_string() ? std::string(v.as_string()) : ""; }
double component(const Json& v, std::size_t i, double fallback = 0) {
    return arr(v).size() > i ? num(arr(v)[i], fallback) : fallback;
}
V add(V a,V b) { return {a[0]+b[0],a[1]+b[1]}; }
V sub(V a,V b) { return {a[0]-b[0],a[1]-b[1]}; }
double dot(V a,V b) { return a[0]*b[0]+a[1]*b[1]; }
double norm(V a) { return std::hypot(a[0],a[1]); }
struct Affine {
    double a=1,b=0,c=0,d=1,x=0,y=0;
    V apply(V p) const { return {a*p[0]+b*p[1]+x,c*p[0]+d*p[1]+y}; }
};
Affine compose(Affine p,Affine q) {
    return {p.a*q.a+p.b*q.c,p.a*q.b+p.b*q.d,p.c*q.a+p.d*q.c,p.c*q.b+p.d*q.d,
        p.a*q.x+p.b*q.y+p.x,p.c*q.x+p.d*q.y+p.y};
}
struct Box {
    double l=0,t=0,r=0,b=0; bool valid=false;
    void extend(V p) {
        if (!valid) { l=r=p[0];t=b=p[1];valid=true; }
        else { l=std::min(l,p[0]);r=std::max(r,p[0]);t=std::min(t,p[1]);b=std::max(b,p[1]); }
    }
    double area() const { return valid ? (r-l)*(b-t) : 0; }
    double diagonal() const { return valid ? std::hypot(r-l,b-t) : 0; }
    double aspect() const { return std::max(r-l,b-t)/std::max(1e-9,std::min(r-l,b-t)); }
    V center() const { return {(l+r)/2,(t+b)/2}; }
};
double overlap(Box a,Box b) {
    if (!a.valid || !b.valid) return 0;
    return std::max(0.,std::min(a.r,b.r)-std::max(a.l,b.l))*std::max(0.,std::min(a.b,b.b)-std::max(a.t,b.t));
}
struct Motion {
    Id parameter=0; std::string property;
    bool varying=false, cross=false, radial=false, rotating=false, anisotropic=false, localized=false;
    bool opening=false, translating=false, small=false, binary=false;
    double magnitude=0, coverage=0;
};
struct Node {
    Id id=0,parent=0; const Json* raw=nullptr;
    std::string type;
    std::vector<Id> children,masks,dodges,welds,physics,parameters;
    std::vector<V> local,world;
    std::vector<V> boundary;
    std::vector<Motion> motions;
    Affine matrix; Box box,localBox;
    bool projected=false; int components=0,holes=0;
};
struct Graph {
    std::map<Id,Node> nodes;
    std::map<Id,const Json*> parameters;
    std::map<Id,std::set<Id>> parameterTargets,links,drivers;
    Checkpoint checkpoint;
    void check() const { if(checkpoint) checkpoint(); }
    void readNode(const Json& raw,Id parent,Affine matrix,bool projected=false,unsigned depth=0) {
        check();
        if(depth>96) throw std::invalid_argument("Structural observation hierarchy exceeds 96 levels");
        Id id=uuid(at(raw,"uuid"));
        if(!id || nodes.contains(id)) throw std::invalid_argument("Invalid/duplicate observed Node UUID");
        Node n; n.id=id;n.parent=parent;n.raw=&raw;n.type=str(at(raw,"type"));
        const auto& tr=at(raw,"transform");
        double angle=component(at(tr,"rot"),2),sx=component(at(tr,"scale"),0,1),sy=component(at(tr,"scale"),1,1);
        Affine local{std::cos(angle)*sx,-std::sin(angle)*sy,std::sin(angle)*sx,std::cos(angle)*sy,
            component(at(tr,"trans"),0),component(at(tr,"trans"),1)};
        n.matrix=compose(matrix,local);
        n.projected=projected || std::abs(component(at(tr,"rot"),0))>1e-6 || std::abs(component(at(tr,"rot"),1))>1e-6;
        const auto& verts=arr(at(at(raw,"mesh"),"verts"));
        for(std::size_t i=0;i+1<verts.size();i+=2) {
            V p{num(verts[i]),num(verts[i+1])}; n.local.push_back(p); n.localBox.extend(p);
            p=n.matrix.apply(p);n.world.push_back(p);n.box.extend(p);
        }
        for(const auto& m:arr(at(raw,"masks"))) {
            Id source=uuid(at(m,"source")); if(source) n.masks.push_back(source);
            if(str(at(m,"mode"))=="DodgeMask") n.dodges.push_back(source);
        }
        for(const auto& w:arr(at(raw,"weldedLinks"))) if(auto target=uuid(at(w,"targetUUID"))) n.welds.push_back(target);
        for(const auto& c:arr(at(raw,"children"))) n.children.push_back(uuid(at(c,"uuid")));
        nodes.emplace(id,std::move(n));
        for(const auto& c:arr(at(raw,"children"))) readNode(c,id,nodes.at(id).matrix,nodes.at(id).projected,depth+1);
    }
    void readParameters(const Json& list) {
        for(const auto& p:arr(list)) {
            if(!at(p,"groupUUID").is_null()) readParameters(at(p,"children"));
            else if(auto id=uuid(at(p,"uuid"))) parameters[id]=&p;
        }
    }
    bool part(Id id) const { return nodes.contains(id) && nodes.at(id).type=="Part"; }
    bool ancestor(Id a,Id b) const {
        while(b && nodes.contains(b)) { b=nodes.at(b).parent; if(a==b) return true; }
        return false;
    }
    std::set<Id> targets(Id p,std::set<Id>& visiting) const {
        if(!visiting.insert(p).second) return {};
        std::set<Id> result;
        if(auto it=parameterTargets.find(p);it!=parameterTargets.end()) result=it->second;
        if(auto it=links.find(p);it!=links.end()) for(auto child:it->second) {
            auto nested=targets(child,visiting); result.insert(nested.begin(),nested.end());
        }
        return result;
    }
};
void topology(Node& n) {
    const auto& indices=arr(at(at(*n.raw,"mesh"),"indices"));
    if(n.local.empty() || indices.empty()) return;
    std::vector<std::size_t> root(n.local.size());
    for(std::size_t i=0;i<root.size();++i)root[i]=i;
    auto find=[&](std::size_t x) {while(root[x]!=x){root[x]=root[root[x]];x=root[x];}return x;};
    std::set<std::size_t> used; std::map<std::pair<std::size_t,std::size_t>,int> edges;
    std::size_t faces=0;
    for(std::size_t i=0;i+2<indices.size();i+=3) {
        std::array<std::size_t,3> tri{};bool valid=true;
        for(int k=0;k<3;++k){double v=num(indices[i+k],-1); if(v<0 || v>=root.size() || std::floor(v)!=v) valid=false; else tri[k]=v;}
        if(!valid || tri[0]==tri[1] || tri[1]==tri[2] || tri[0]==tri[2])continue;
        ++faces;
        for(int k=0;k<3;++k) {auto a=tri[k],b=tri[(k+1)%3];used.insert(a);root[find(a)]=find(b);++edges[{std::min(a,b),std::max(a,b)}];}
    }
    std::set<std::size_t> components;for(auto v:used)components.insert(find(v));
    n.components=static_cast<int>(components.size());
    n.holes=std::max(0,n.components-static_cast<int>(used.size())+static_cast<int>(edges.size())-static_cast<int>(faces));
    std::set<std::size_t> boundary;
    for(const auto& [edge,count]:edges)if(count==1){boundary.insert(edge.first);boundary.insert(edge.second);}
    for(auto v:boundary)n.boundary.push_back(n.world[v]);
}
bool boundaryContact(const Node& a,const Node& b) {
    if(a.boundary.size()<2 || b.boundary.size()<2)return false;
    const double tolerance=std::min(a.box.diagonal(),b.box.diagonal())*0.015;
    if(tolerance<1e-8)return false;
    if(a.box.r+tolerance<b.box.l || b.box.r+tolerance<a.box.l ||
        a.box.b+tolerance<b.box.t || b.box.b+tolerance<a.box.t)return false;
    std::set<std::size_t> paired;std::vector<V> points;
    for(auto p:a.boundary) {
        double best=tolerance;std::size_t index=b.boundary.size();
        for(std::size_t i=0;i<b.boundary.size();++i){auto distance=norm(sub(p,b.boundary[i]));if(distance<=best){best=distance;index=i;}}
        if(index<b.boundary.size() && paired.insert(index).second)points.push_back(p);
    }
    if(points.size()<2)return false;
    Box extent;for(auto p:points)extent.extend(p);
    return extent.diagonal()>tolerance*2;
}
bool setCell(const Json& binding,std::size_t x,std::size_t y) {
    const auto& flags=arr(at(binding,"isSet"));
    return x<flags.size() && y<arr(flags[x]).size() && arr(flags[x])[y].is_bool() && arr(flags[x])[y].as_bool();
}
std::size_t neutralIndex(const Json& p,int dimension) {
    const auto& axes=arr(at(p,"axis_points"));if(axes.size()!=2)return 0;
    auto lo=component(at(p,"min"),dimension),hi=component(at(p,"max"),dimension,1);
    auto value=component(at(p,"defaults"),dimension);
    const double normalized=hi!=lo?(value-lo)/(hi-lo):0;
    double best=1e100;std::size_t found=0,i=0;
    for(const auto& v:arr(axes[dimension])){auto d=std::abs(num(v)-normalized);if(d<best){best=d;found=i;}++i;}
    return found;
}
Motion measure(const Node& n,Id parameter,const Json& p,const Json& binding,const Checkpoint& checkpoint) {
    Motion m; m.parameter=parameter;m.property=str(at(binding,"param_name"));
    const auto& values=arr(at(binding,"values"));
    if(values.empty())return m;
    auto nx=neutralIndex(p,0),ny=neutralIndex(p,1);
    if(nx>=values.size() || ny>=arr(values[nx]).size() || !setCell(binding,nx,ny)) {
        bool found=false;
        for(std::size_t x=0;x<values.size() && !found;++x)for(std::size_t y=0;y<arr(values[x]).size();++y)
            if(setCell(binding,x,y)){nx=x;ny=y;found=true;break;}
        if(!found)return m;
    }
    const auto& neutral=arr(values[nx])[ny];
    const double size=std::max(1e-6,n.localBox.diagonal());
    V center{};for(auto v:n.local)center=add(center,v);
    if(!n.local.empty()){center[0]/=n.local.size();center[1]/=n.local.size();}
    std::size_t observed=0;bool allBinary=true;std::set<int> states;
    for(std::size_t x=0;x<values.size();++x) {
        if(checkpoint)checkpoint();
        const auto& row=arr(values[x]);
        for(std::size_t y=0;y<row.size();++y) {
            if(!setCell(binding,x,y))continue;
            const auto& cell=row[y];++observed;
            if(m.property!="deform") {
                if(!cell.is_number() || !neutral.is_number())continue;
                double v=num(cell),delta=std::abs(v-num(neutral));m.magnitude=std::max(m.magnitude,delta);
                m.varying |= delta>1e-6;
                allBinary &= std::abs(v)<1e-5 || std::abs(v-1)<1e-5;states.insert(v>0.5);
                if(x<values.size() && ny<row.size() && y<arr(values[nx]).size() &&
                    setCell(binding,x,ny) && setCell(binding,nx,y))
                    m.cross |= std::abs(v-num(row[ny])-num(arr(values[nx])[y])+num(neutral))>1e-5;
                continue;
            }
            if(arr(cell).size()!=n.local.size() || arr(neutral).size()!=n.local.size() || n.local.empty())continue;
            std::vector<V> delta;V mean{};double max=0;
            for(std::size_t i=0;i<n.local.size();++i){
                V d{component(arr(cell)[i],0)-component(arr(neutral)[i],0),component(arr(cell)[i],1)-component(arr(neutral)[i],1)};
                delta.push_back(d);mean=add(mean,d);max=std::max(max,norm(d));
            }
            if(max<=size*1e-6)continue;
            mean[0]/=delta.size();mean[1]/=delta.size();m.varying=true;m.magnitude=std::max(m.magnitude,max/size);
            double radial=0,tangent=0,radius=0,error=0,rx=0,ry=0,dx=0,dy=0;
            std::size_t moving=0;Box deformed;
            for(std::size_t i=0;i<delta.size();++i) {
                auto r=sub(n.local[i],center),d=sub(delta[i],mean);
                radius+=dot(r,r);radial+=dot(r,d);tangent+=r[0]*d[1]-r[1]*d[0];error+=dot(d,d);
                rx+=r[0]*r[0];ry+=r[1]*r[1];dx+=r[0]*d[0];dy+=r[1]*d[1];
                moving+=norm(delta[i])>max*0.05;
                deformed.extend(add(n.local[i],delta[i]));
            }
            m.coverage=std::max(m.coverage,static_cast<double>(moving)/delta.size());
            m.localized |= moving>0 && moving<delta.size()*0.85;
            m.translating |= error/delta.size()<max*max*0.01;
            if(radius>1e-9) {
                double a=radial/radius,b=tangent/radius;
                m.radial |= std::abs(a)>0.015 && a*a*radius>error*0.8;
                m.rotating |= std::abs(b)>0.015 && (a*a+b*b)*radius>error*0.8;
                if(rx>1e-9 && ry>1e-9) m.anisotropic |= std::abs(dx/rx-dy/ry)>0.03;
            }
            if(n.localBox.area()>1e-9) m.opening |= deformed.area()/n.localBox.area()<0.35 && m.anisotropic;
            if(x!=nx && y!=ny && ny<row.size() && y<arr(values[nx]).size() &&
                setCell(binding,x,ny) && setCell(binding,nx,y) && arr(row[ny]).size()==delta.size() &&
                arr(arr(values[nx])[y]).size()==delta.size()) {
                for(std::size_t i=0;i<delta.size();++i) for(int k=0;k<2;++k)
                    m.cross |= std::abs(component(arr(cell)[i],k)-component(arr(row[ny])[i],k)-
                        component(arr(arr(values[nx])[y])[i],k)+component(arr(neutral)[i],k))>size*0.001;
            }
        }
    }
    m.small=m.varying && m.property=="deform" && m.magnitude<0.08;
    m.binary=observed>1 && allBinary && states.size()==2;
    return m;
}
bool moving(const Node& n,std::string_view property) {
    return std::any_of(n.motions.begin(),n.motions.end(),[&](const Motion& m){return m.varying && m.property==property;});
}
template<class Predicate> bool motion(const Node& n,Predicate predicate) {
    return std::any_of(n.motions.begin(),n.motions.end(),predicate);
}
A ids(const std::vector<Id>& values) {A result;std::set<Id>seen;for(auto id:values)if(id && seen.insert(id).second)result.push_back(id);return result;}
O roles(Id target,std::string_view role={},std::vector<Id> values={}) {
    O r{{"target",A{target}}};if(!role.empty())r[role]=ids(values);return r;
}
}

Json ngDetectStructuralPatterns(const Json& document,Checkpoint checkpoint) {
    Graph g;g.checkpoint=checkpoint;g.readNode(at(document,"nodes"),0,{});g.readParameters(at(document,"param"));
    for(auto& [id,n]:g.nodes)topology(n);
    for(const auto& [pid,p]:g.parameters) for(const auto& b:arr(at(*p,"bindings"))) {
        g.check();auto target=uuid(at(b,"node"));auto property=str(at(b,"param_name"));
        if((property=="X" || property=="Y") && g.parameters.contains(target)){g.links[pid].insert(target);continue;}
        if(!g.nodes.contains(target))continue;
        auto& n=g.nodes.at(target);n.parameters.push_back(pid);g.parameterTargets[pid].insert(target);
        n.motions.push_back(measure(n,pid,*p,b,checkpoint));
    }
    for(auto& [id,n]:g.nodes) if(n.type=="SimplePhysics") {
        Id p=uuid(at(*n.raw,"param"));if(!p)p=uuid(at(*n.raw,"outputUUID"));
        std::set<Id> visiting;
        for(auto target:g.targets(p,visiting)){g.nodes.at(target).physics.push_back(id);g.drivers[target].insert(id);}
    }
    O result;for(const auto& spec:ngPatternSpecs())result[spec.id]=O{{"matches",A{}},{"candidates",A{}}};
    std::set<std::pair<std::string,Id>> emitted;
    auto emit=[&](std::string_view pattern,const Node& n,O role,std::string_view evidence,bool certain,
                  O metrics=O{},A missing=A{}) {
        if(!emitted.emplace(std::string(pattern),n.id).second)return;
        bool complete=true;for(const auto& expected:ngPatternSpec(pattern).roles)
            complete &= role.contains(expected) && !arr(role.at(expected)).empty();
        if(!complete){certain=false;missing.push_back("structural_role");}
        O match{{"roles",role},{"provenance","structural_detector"},{"evidence",A{evidence}},
            {"metrics",metrics},{"confidence",certain?0.95:0.55},{"requires_review",!certain},
            {"missing",missing},{"parts",A{}},{"nodes",A{}}};
        std::set<Id> unique;
        for(const auto& r:role)for(const auto& value:arr(r.value())){
            auto id=uuid(value);if(unique.insert(id).second)match[g.part(id)?"parts":"nodes"].as_array().push_back(id);
        }
        result.at(pattern).as_object().at(certain?"matches":"candidates").as_array().push_back(match);
    };
    for(const auto& [id,n]:g.nodes) {
        g.check();const bool part=g.part(id),hasMesh=!n.local.empty(),elongated=hasMesh && n.localBox.aspect()>2.5;
        auto parent=n.parent;const Node* support=parent?&g.nodes.at(parent):nullptr;
        std::vector<Id> surfaces,masks,welds;
        for(auto target:n.masks)if(g.part(target) && target!=id) {surfaces.push_back(target);masks.push_back(target);}
        for(auto target:n.welds)if(g.part(target) && target!=id)welds.push_back(target);
        bool transform=motion(n,[](const Motion& m){return m.varying && m.property.starts_with("transform.");});
        bool deform=moving(n,"deform"),opening=motion(n,[](const Motion& m){return m.opening;});
        bool radial=motion(n,[](const Motion& m){return m.radial;}),rotation=motion(n,[](const Motion& m){return m.rotating;});
        bool localized=motion(n,[](const Motion& m){return m.localized;});
        bool nested=false,apertureSupport=false;
        for(auto surface:surfaces){const auto& receiver=g.nodes.at(surface);nested |= receiver.masks.size()>0;
            apertureSupport |= motion(receiver,[](const Motion& m){return m.opening;});}
        std::vector<Id> containing,contacts;
        if(part && n.box.area()>1e-8 && !n.projected) for(const auto& [otherId,other]:g.nodes) {
            g.check();
            if(otherId==id || !g.part(otherId) || other.projected || other.box.area()<1e-8)continue;
            const double coverage=overlap(n.box,other.box)/n.box.area();
            if(coverage>0.9 && other.box.area()>n.box.area()*1.3 &&
                (parent==other.parent || g.ancestor(otherId,id)))containing.push_back(otherId);
            if(coverage<0.8 && boundaryContact(n,other))contacts.push_back(otherId);
        }
        std::sort(containing.begin(),containing.end(),[&](Id a,Id b){return g.nodes.at(a).box.area()<g.nodes.at(b).box.area();});
        auto candidateSurface=surfaces.empty()?containing:surfaces;
        if(candidateSurface.size()>1 && surfaces.empty())candidateSurface.resize(1);
        const auto geometry=O{{"aspect",hasMesh?n.localBox.aspect():0},{"components",n.components},{"holes",n.holes},
            {"parameters",ids(n.parameters)},{"physics",ids(n.physics)}};
        if(deform || transform)emit("p01",n,roles(id),"authored_spatial_binding",true,geometry);
        for(const auto& [source,destinations]:g.links) for(auto destination:destinations) {
            std::set<Id> visiting;auto targets=g.targets(destination,visiting);
            if(targets.contains(id))emit("p02",n,roles(id),"parameter_axis_link",true,
                O{{"source_parameter",source},{"destination_parameter",destination}});
        }
        if(motion(n,[](const Motion& m){return m.cross;}))emit("p03",n,roles(id),"nonseparable_sample_grid",true,geometry);
        if(part && !candidateSurface.empty())emit("p04",n,roles(id,"surface",candidateSurface),
            "contained_or_masked_surface",false,geometry,A{"attachment_intent","surface_correspondence"});
        if(parent && deform && transform)emit("p05",n,roles(id,"support",{parent}),"local_transform_and_deformation",true,geometry);
        if(part && elongated)emit("p06",n,roles(id),rotation?"elongated_rotating_field":"elongated_mesh",false,geometry,A{"volume_axis"});
        if(part && parent && (surfaces.size()>1 || (localized && !contacts.empty())))
            emit("p07",n,roles(id,"support",surfaces.empty()?std::vector<Id>{parent}:surfaces),
                "multiple_surfaces_or_fixed_free_region",false,geometry,A{"front_back_side_assignment"});
        if(part && !masks.empty())emit("p08",n,roles(id,"mask",masks),"explicit_mask_edges",true,geometry);
        else if(part && !containing.empty())emit("p08",n,roles(id,"mask",{containing.front()}),
            "contained_layer",false,geometry,A{"occlusion_intent"});
        if(part && !n.dodges.empty()) {
            std::vector<Id> valid;for(auto v:n.dodges)if(g.part(v) && v!=id)valid.push_back(v);
            if(!valid.empty())emit("p09",n,roles(id,"mask",valid),"explicit_dodge_mask",true,geometry);
        } else if(part && !masks.empty()) {
            bool excess=false;for(auto m:masks)excess |= overlap(n.box,g.nodes.at(m).box)<n.box.area()*0.9;
            if(excess)emit("p09",n,roles(id,"mask",masks),"source_extends_outside_mask_bounds",false,geometry,A{"alpha_coverage"});
        }
        bool shared=false;for(auto m:masks)for(const auto& [otherId,other]:g.nodes)
            if(otherId!=id && std::find(other.masks.begin(),other.masks.end(),m)!=other.masks.end())shared=true;
        if(!masks.empty() && (masks.size()>1 || nested || shared))emit("p10",n,roles(id,"mask",masks),
            "multiple_shared_or_chained_masks",true,geometry);
        auto blend=str(at(*n.raw,"blend_mode"));
        if(part && (blend=="ClipToLower" || blend=="SliceFromLower"))emit("p11",n,roles(id),"lower_alpha_blend_mode",true);
        if(moving(n,"zSort"))emit("p12",n,roles(id),"varying_zsort_binding",true,geometry);
        if(moving(n,"opacity"))emit("p13",n,roles(id),"varying_opacity_binding",true,geometry);
        bool lighting=blend=="Multiply" || blend=="Screen" || blend=="AddGlow" || blend=="LinearDodge";
        if(part && !candidateSurface.empty() && lighting && (transform || deform || moving(n,"opacity")))
            emit("p14",n,roles(id,"surface",candidateSurface),"animated_lighting_blend_on_surface",false,geometry,A{"lighting_semantics"});
        if(part && ((!blend.empty() && blend!="Normal") || motion(n,[](const Motion& m){return m.varying &&
            (m.property.starts_with("tint.") || m.property.starts_with("screenTint.") || m.property=="emissionStrength");})))
            emit("p15",n,roles(id),"material_property_or_blend",true,geometry);
        if(part && !welds.empty())emit("p16",n,roles(id,"seam",welds),"explicit_welding_edges",true,geometry);
        else if(part && !contacts.empty())emit("p16",n,roles(id,"seam",contacts),"nearby_mesh_boundary_vertices",false,geometry,A{"boundary_correspondence","visible_alpha_boundary"});
        if(part && opening)emit("p17",n,roles(id),"anisotropic_aperture_collapse",true,geometry);
        else if(part && n.localBox.aspect()>1.5 && !containing.empty())emit("p17",n,roles(id),
            "contained_elongated_region",false,geometry,A{"aperture_boundary","eye_or_mouth_context"});
        if(part && !surfaces.empty() && apertureSupport && (transform || deform))emit("p18",n,roles(id,"support",surfaces),
            "moving_content_of_animated_aperture",false,geometry,A{"shared_opening_input"});
        else if(part && !containing.empty())emit("p18",n,roles(id,"support",{containing.front()}),
            "contained_content",false,geometry,A{"aperture_support"});
        if(part && (nested || !containing.empty()) && (motion(n,[](const Motion& m){return m.small;}) || !n.physics.empty()))
            emit("p19",n,roles(id),"nested_small_motion_or_physics",false,geometry,A{"material_role"});
        if(part && motion(n,[](const Motion& m){return m.anisotropic;}) && (opening || !containing.empty()))
            emit("p20",n,roles(id),"contained_anisotropic_shape_controls",false,geometry,A{"opening_semantics"});
        if(part && motion(n,[](const Motion& m){return m.property=="opacity" && m.binary;}))
            emit("p21",n,roles(id),"binary_visibility_states",false,geometry,A{"expression_semantics"});
        if(parent && (moving(n,"transform.r.z") || rotation))emit("p22",n,roles(id),"local_articulation",true,geometry);
        else if(parent && elongated && support && support->parent)emit("p22",n,roles(id),
            "elongated_hierarchical_chain",false,geometry,A{"joint_locations"});
        if(parent && n.children.size()>=2) {
            std::size_t branches=0;for(auto c:n.children){const auto& child=g.nodes.at(c);
                branches+=!child.local.empty() && child.localBox.aspect()>2;}
            if(branches>=2)emit("p23",n,roles(id,"support",{parent}),"multiple_elongated_children",false,geometry,A{"branch_roots"});
        }
        if(part && radial)emit("p24",n,roles(id),"radial_expansion_field",true,geometry);
        else if(moving(n,"transform.s.x") && moving(n,"transform.s.y"))emit("p24",n,roles(id),
            "animated_scale_axes",false,geometry,A{"coupled_expansion_input"});
        if(part && parent && localized && (radial || !n.physics.empty()))emit("p25",n,roles(id,"support",{parent}),
            "supported_partial_motion",false,geometry,A{"soft_volume_semantics"});
        if(part && parent && !elongated && (!n.physics.empty() || (support && !support->local.empty() && support->localBox.aspect()>3)))
            emit("p26",n,roles(id,"support",{parent}),"compact_region_on_driver_or_thin_support",false,geometry,A{"rigidity","pivot"});
        if(parent && (n.holes>0 || n.children.size()>=3)) {
            std::size_t tails=0;for(auto c:n.children){const auto& child=g.nodes.at(c);tails+=!child.local.empty() && child.localBox.aspect()>2.5;}
            if(n.holes>0 || tails>=2)emit("p27",n,roles(id,"support",{parent}),"loop_or_knot_with_tails",false,geometry,A{"crossing_connectivity"});
        }
        if(part && elongated && (welds.size()>=2 || contacts.size()>=2))emit("p28",n,
            roles(id,"support",welds.size()>=2?welds:contacts),"elongated_region_with_multiple_contacts",false,geometry,A{"fixed_anchor_endpoints"});
        std::set<Id> inheritedDrivers(n.physics.begin(),n.physics.end());
        for(Id p=parent;p;p=g.nodes.at(p).parent)for(auto driver:g.nodes.at(p).physics)inheritedDrivers.insert(driver);
        if(inheritedDrivers.size()>=2)emit("p29",n,roles(id),"multiple_direct_or_ancestor_physics_outputs",true,
            O{{"physics",ids(std::vector<Id>(inheritedDrivers.begin(),inheritedDrivers.end()))}});
        if(n.type=="SimplePhysics" && parent) {
            bool inherited=false;for(Id p=parent;p;p=g.nodes.at(p).parent)inherited|=motion(g.nodes.at(p),[](const Motion& m){
                return m.varying && m.property.starts_with("transform.");});
            if(transform || inherited)emit("p30",n,roles(id,"support",{parent}),"animated_physics_reference_frame",true,geometry);
            if(motion(n,[](const Motion& m){return m.varying && (m.property.starts_with("outputScale.") ||
                m.property=="angleDamping" || m.property=="lengthDamping" || m.property=="frequency" || m.property=="length" || m.property=="gravity");}))
                emit("p31",n,roles(id),"animated_physics_coefficients",true,geometry);
        }
        if(part && (localized || motion(n,[](const Motion& m){return m.small && !m.translating;})))
            emit("p32",n,roles(id),"localized_or_small_shape_field",true,geometry);
        if(n.type=="MeshGroup" || n.type=="Composite" || n.type=="DynamicComposite")
            emit("p33",n,roles(id),"deformation_scope_node",true,O{{"type",n.type},
                {"dynamic_deformation",at(*n.raw,"dynamic_deformation")},{"propagate_meshgroup",at(*n.raw,"propagate_meshgroup")}});
        if(part && localized)emit("p34",n,roles(id),"fixed_and_moving_vertices",false,geometry,A{"mixed_material_semantics"});
        else if(part && n.components>1)emit("p34",n,roles(id),"disconnected_mesh_regions",false,geometry,A{"region_semantics"});
        if(moving(n,"transform.t.x") || moving(n,"transform.t.y") ||
            motion(n,[](const Motion& m){return m.translating;}))emit("p35",n,roles(id),"independent_translation_control",true,geometry);
    }
    return result;
}
}
