#pragma once

#include "native_stage.hpp"
#include "../registered.hpp"
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <param/binding_impl.hpp>
#include <map>
#include <numeric>

namespace nijigenerate::autorig::deterministic::native_detail {
namespace head_draw_order {
using Point3=contracts::Point3;
using Point2=contracts::Point2;
using evidence::detail::get;
using evidence::detail::stringGet;
using Key=nicxlive::core::param::Vec2u;
using ValueBinding=nicxlive::core::param::ValueParameterBinding;
using Cell=std::pair<int,int>;

struct Projection {
    compat::Slice<double> matrix;
    Point3 view;
    explicit Projection(const Json& key) : matrix(contracts::ngRigNumbers(indexNative(key,"projection"))),
        view{matrix[2]*matrix[5]-matrix[4]*matrix[3],
            matrix[4]*matrix[1]-matrix[0]*matrix[5], matrix[0]*matrix[3]-matrix[2]*matrix[1]} {}
    Point3 operator()(Point3 p) const {
        return {matrix[0]*p[0]+matrix[2]*p[1]+matrix[4]*p[2]+matrix[6],
            matrix[1]*p[0]+matrix[3]*p[1]+matrix[5]*p[2]+matrix[7],
            view[0]*p[0]+view[1]*p[1]+view[2]*p[2]};
    }
};

inline std::vector<Point3> samples(const Json& material,const Json& target,double scale,std::size_t limit) {
    const auto support=material.sourceObjectRef().if_contains("draw_order_cloud");
    if (!support) return {};
    const auto cloud=contracts::ngRigPoints(*support);
    const auto frame=contracts::ngRigNumbers(indexNative(target,"parent_to_root"));
    const double det=frame[0]*frame[4]-frame[1]*frame[3];
    if (std::abs(det)<1e-9) return {};
    std::vector<Point3> result;
    compat::Slice<Point2> local;
    const auto stride=1+(cloud.size()?cloud.size()-1:0)/limit;
    for (std::size_t i=0;i<cloud.size();i+=stride) {
        const auto p=cloud[i];
        const double x=p[0]-frame[2],y=p[1]-frame[5];
        local.append(Point2{(frame[4]*x-frame[1]*y)/det,(frame[0]*y-frame[3]*x)/det});
        result.push_back({p[0],p[1],0.});
    }
    const auto depth=registered::ngRigSampleRegisteredDepth(contracts::ngRigNumbers(indexNative(target,"xs")),
        contracts::ngRigNumbers(indexNative(target,"ys")),contracts::ngRigNumbers(indexNative(target,"depth")),local);
    for (std::size_t i=0;i<result.size();++i) result[i][2]=depth[i]*scale;
    return result;
}

// Preserve composite boundaries. An outer composite is the render unit
// whenever the face lies in another scope.
inline std::vector<NodeRef> drawPath(NodeRef part) {
    std::vector<NodeRef> path{part};
    for (auto p=part->parentPtr();p;p=p->parentPtr())
        if (p->typeId()=="Composite" || p->typeId()=="DynamicComposite") path.push_back(p);
    std::reverse(path.begin(),path.end());
    return path;
}

template<class Puppet, class Parameter>
Json apply(const Json& state,const core::settings::SettingsJsonArray& headKeys,
    const Puppet& puppet,const Parameter& parameter) {
    using Array=core::settings::SettingsJsonArray;
    Array evidence;
    const auto verification=get(state,"depth_validation",Json{});
    const auto* scaleField=verification.is_object()?verification.sourceObjectRef().if_contains("compiled_scale"):nullptr;
    if (!scaleField) return evidence;
    const double scale=contracts::ngRigNumber(*scaleField);
    std::map<std::uint32_t,Json> materials;
    for (const auto& material : indexNative(state,"materials").sourceArrayValue())
        materials.emplace(uuid(indexNative(material,"uuid")),material);

    struct Skin {
        std::vector<NodeRef> path;
        std::vector<Point3> points;
    };
    const auto targets=indexNative(state,"targets").sourceArrayValue();
    std::vector<Skin> skin;
    double left=INFINITY,right=-INFINITY,top=INFINITY,bottom=-INFINITY;
    for (const auto& target : targets) {
        if (stringGet(target,"role","")!="face") continue;
        const auto id=uuid(indexNative(target,"part"));
        const auto& material=materials.at(id);
        if (material.sourceObjectRef().if_contains("receiver")) continue;
        auto points=samples(material,target,scale,2048);
        if (points.empty()) continue;
        auto part=puppet->findNodeById(id);
        if (!part) continue;
        for (const auto& p : points) {
            left=std::min(left,p[0]); right=std::max(right,p[0]);
            top=std::min(top,p[1]); bottom=std::max(bottom,p[1]);
        }
        skin.push_back({drawPath(part),std::move(points)});
    }
    const double width=right-left,height=bottom-top,middle=std::midpoint(left,right);
    if (!(width>0. && height>0.)) return evidence;

    using BindingName=nicxlive::core::common::NullableString;
    const auto sortName=BindingName::fromStaticLiteral("zSort");
    struct Candidate {
        std::vector<NodeRef> path;
        std::vector<Point3> points;
        double lateral;
        std::vector<NodeRef> clips;
    };
    const auto receiverRoot=[&](std::uint32_t id) {
        for (std::size_t depth=0;depth<materials.size();++depth) {
            const auto found=materials.find(id);
            if (found==materials.end()) break;
            const auto* receiver=found->second.sourceObjectRef().if_contains("receiver");
            if (!receiver) return id;
            id=uuid(*receiver);
        }
        return std::uint32_t{};
    };
    std::map<std::uint32_t,std::vector<NodeRef>> clips;
    for (const auto& [id,material] : materials) {
        if (!material.sourceObjectRef().if_contains("receiver")) continue;
        const auto root=receiverRoot(id);
        if (root) if (auto child=puppet->findNodeById(id)) clips[root].push_back(child);
    }
    std::vector<Candidate> candidates;
    for (const auto& target : targets) {
        if (stringGet(target,"owner","")!="head") continue;
        const auto role=stringGet(target,"role","");
        if (role!="hair_side" && role!="headwear" && role!="hair_back") continue;
        const auto id=uuid(indexNative(target,"part"));
        const auto& material=materials.at(id);
        if (material.sourceObjectRef().if_contains("receiver")) continue;
        auto points=samples(material,target,scale,256);
        if (points.empty()) continue;
        double x=0.,y=0.;
        for (const auto& p : points) { x+=p[0]; y+=p[1]; }
        x=x/points.size()-middle; y=(y/points.size()-top)/height;
        if (std::abs(x)<.24*width) continue;
        if (role!="hair_side") {
            const auto sameSide=std::ranges::count_if(points,[&](const auto& p) {
                return (p[0]-middle)*x>.1*width*std::abs(x);
            });
            if (y<-.45 || y>.92 || std::abs(x)>.92*width ||
                static_cast<double>(sameSide)<.9*points.size()) continue;
            if (role=="hair_back") {
                // Ear pads can share the rear-hair grid. Use their own compact
                // alpha footprint to distinguish them from the rear hair mass.
                const auto xs=std::ranges::minmax_element(points,{},[](const auto& p) { return p[0]; });
                const auto ys=std::ranges::minmax_element(points,{},[](const auto& p) { return p[1]; });
                const double w=(*xs.max)[0]-(*xs.min)[0],h=(*ys.max)[1]-(*ys.min)[1];
                if (y<.30 || y>.91 || std::abs(x)<.34*width || w<.05*width || w>.60*width ||
                    h<.07*height || h>.58*height) continue;
            }
        }
        if (auto part=puppet->findNodeById(id)) candidates.push_back({drawPath(part),std::move(points),x,clips[id]});
    }
    if (candidates.empty()) return evidence;

    const auto leafAllowed=[&](const auto& self,const NodeRef& node,std::uint32_t source) -> bool {
        if (node->typeId()=="Part" && receiverRoot(node->getUUID())!=source) return false;
        for (auto child : node->children) if (!self(self,child,source)) return false;
        return true;
    };
    struct Comparison {
        NodeRef moving;
        NodeRef face;
        const Candidate* candidate;
        std::size_t skinIndex;
        double frontmost;
    };
    std::vector<Comparison> pairs;
    for (const auto& candidate : candidates) for (std::size_t i=0;i<skin.size();++i) {
        const auto& face=skin[i];
        const auto mismatch=std::mismatch(candidate.path.begin(),candidate.path.end(),face.path.begin(),face.path.end());
        if (mismatch.first==candidate.path.end() || mismatch.second==face.path.end()) continue;
        const auto& moving=*mismatch.first;
        const auto& backing=*mismatch.second;
        if (parameter->getBinding(moving,sortName)) continue;
        const bool part=moving==candidate.path.back();
        if (!part && !leafAllowed(leafAllowed,moving,candidate.path.back()->getUUID())) continue;
        double frontmost=moving->zSortNoOffset();
        bool compatible=true;
        for (const auto& clip : candidate.clips) {
            const auto path=drawPath(clip);
            if (part) {
                if (!std::ranges::equal(candidate.path.begin(),candidate.path.end()-1,path.begin(),path.end()-1) ||
                    parameter->getBinding(clip,sortName)) compatible=false;
                frontmost=std::min(frontmost,static_cast<double>(clip->zSortNoOffset()));
            } else if (std::ranges::find(path,moving)==path.end()) compatible=false;
        }
        if (!compatible || frontmost>backing->zSortNoOffset()) continue;
        pairs.push_back({moving,backing,&candidate,i,frontmost});
    }
    if (pairs.empty()) return evidence;

    const auto nx=parameter->axisPoints[0].size(), ny=parameter->axisPoints[1].size();
    const auto neutral=parameter->findClosestKeypoint(parameter->defaults);
    struct Plan {
        NodeRef node;
        std::vector<float> offsets;
    };
    std::map<std::uint32_t,Plan> plans;
    const double cellSize=.035*width;
    const auto cell=[&](Point3 p)->Cell {
        return {static_cast<int>(std::floor(p[0]/cellSize)),static_cast<int>(std::floor(p[1]/cellSize))};
    };
    std::vector<bool> visited(nx*ny,false);
    for (const auto& row : headKeys) {
        const auto key=contracts::ngRigNumbers(indexNative(row,"key"));
        const auto x=static_cast<std::size_t>(key[0]), y=static_cast<std::size_t>(key[1]);
        const auto index=x*ny+y;
        if (x==neutral.x || visited[index]) continue;
        visited[index]=true;
        const Projection project(row);
        std::vector<std::map<Cell,double>> faceDepths(skin.size());
        for (std::size_t i=0;i<skin.size();++i) for (const auto& p : skin[i].points) {
            const auto q=project(p);
            auto& depth=faceDepths[i];
            auto [it,inserted]=depth.try_emplace(cell(q),q[2]);
            if (!inserted) it->second=std::max(it->second,q[2]);
        }
        struct Shift { NodeRef node; double behind,frontmost; const Candidate* candidate; };
        std::map<std::uint32_t,Shift> shifts;
        for (const auto& pair : pairs) {
            if (project.view[0]*pair.candidate->lateral>-.012*width) continue;
            std::size_t overlap=0,behind=0;
            for (const auto& p : pair.candidate->points) {
                const auto q=project(p);
                const auto hit=faceDepths[pair.skinIndex].find(cell(q));
                if (hit==faceDepths[pair.skinIndex].end()) continue;
                ++overlap;
                if (q[2]+.018*width<hit->second) ++behind;
            }
            if (behind<3 || behind*4<overlap*3) continue;
            const auto id=pair.moving->getUUID();
            const double order=pair.face->zSortNoOffset();
            auto [it,inserted]=shifts.try_emplace(id,Shift{pair.moving,order,pair.frontmost,pair.candidate});
            if (!inserted) it->second.behind=std::max(it->second.behind,order);
        }
        std::vector<Shift> ordered;
        for (const auto& [id,shift] : shifts) ordered.push_back(shift);
        std::ranges::stable_sort(ordered,{},&Shift::frontmost);
        std::map<std::uint32_t,double> lastByScope;
        for (const auto& shift : ordered) {
            const auto path=drawPath(shift.node);
            const auto scope=path.size()>1 ? path[path.size()-2]->getUUID() : 0;
            auto [last,inserted]=lastByScope.try_emplace(scope,-INFINITY);
            const double destination=std::max(shift.behind+.25,last->second+.001);
            last->second=destination;
            const float offset=static_cast<float>(destination-shift.frontmost);
            const auto record=[&](const NodeRef& node) {
                auto [it,inserted]=plans.try_emplace(node->getUUID(),Plan{node,std::vector<float>(nx*ny)});
                it->second.offsets[index]=offset;
            };
            record(shift.node);
            // Keep clipping receivers and their layers together.
            if (shift.node==shift.candidate->path.back())
                for (const auto& clip : shift.candidate->clips) record(clip);
        }
    }
    auto group=createNativeObject<actions::GroupAction>();
    for (const auto& [id,plan] : plans) {
        auto binding=std::dynamic_pointer_cast<ValueBinding>(parameter->createBinding(plan.node,sortName));
        if (!binding) continue;
        // Draw order switches at an occlusion boundary. Interpolating the
        // offset would leave it in front until nearly the next keyed pose.
        binding->setInterpolateMode(nicxlive::core::common::InterpolateMode::Nearest);
        parameter->addBinding(binding);
        group->addAction(actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(
            parameter,binding));
        Array offsets;
        for (std::size_t x=0;x<nx;++x) for (std::size_t y=0;y<ny;++y) {
            const float value=plan.offsets[x*ny+y];
            binding->setValue(Key{static_cast<unsigned>(x),static_cast<unsigned>(y)},value);
            offsets.append(value);
        }
        binding->reInterpolate();
        evidence.append(evidence::detail::objectEvidence({
            {evidence::detail::stringLiteralEvidence("node"),id},
            {evidence::detail::stringLiteralEvidence("offsets"),offsets}}));
    }
    if (!group->empty()) core::actionstack::incActionPush(group);
    return evidence;
}
}
}
