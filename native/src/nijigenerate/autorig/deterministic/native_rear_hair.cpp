#include "native_stage.hpp"
#include "detail/native_stage.hpp"
#include "detail/head_template_data.hpp"
#include "detail/head_surface_ridge.hpp"
#include "detail/evidence.hpp"
#include "controls_projection.hpp"
#include "observation.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <vector>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
using contracts::Point2;
using contracts::Point3;
constexpr unsigned sections = 48;

struct Silhouette {
    double top, bottom;
    std::vector<Point2> edges;

    Point2 at(double y) const {
        const double row = std::clamp((y-top)/(bottom-top),0.,1.)*sections;
        const auto i = std::min(static_cast<unsigned>(row),sections-1);
        return {std::lerp(edges[i][0],edges[i+1][0],row-i),
                std::lerp(edges[i][1],edges[i+1][1],row-i)};
    }
};

Silhouette measure(const compat::Slice<Point2>& points) {
    constexpr double inf = std::numeric_limits<double>::infinity();
    Silhouette result{inf,-inf,{}};
    for (const auto p : points) {
        result.top = std::min(result.top,p[1]);
        result.bottom = std::max(result.bottom,p[1]);
    }
    if (!(result.bottom>result.top)) return result;
    result.edges.resize(sections+1,Point2{inf,-inf});
    for (const auto p : points) {
        const auto i = static_cast<unsigned>(std::lround(sections*(p[1]-result.top)/(result.bottom-result.top)));
        result.edges[i][0] = std::min(result.edges[i][0],p[0]);
        result.edges[i][1] = std::max(result.edges[i][1],p[0]);
    }
    unsigned previous=0;
    for (unsigned next=1; next<=sections; ++next) {
        if (result.edges[next][0]>result.edges[next][1]) continue;
        const auto a=result.edges[previous], b=result.edges[next];
        for (unsigned row=previous+1;row<next;++row) {
            const double t=double(row-previous)/(next-previous);
            result.edges[row]={std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)};
        }
        previous=next;
    }
    return result;
}

std::optional<Point2> horizontalExtent(const std::vector<Point2>& hull, double y) {
    std::optional<Point2> extent;
    for (std::size_t i=0; i<hull.size(); ++i) {
        const auto a=hull[i], b=hull[(i+1)%hull.size()];
        if ((a[1]>y)==(b[1]>y)) continue;
        const auto x=std::lerp(a[0],b[0],(y-a[1])/(b[1]-a[1]));
        if (!extent) extent=Point2{x,x};
        else {
            (*extent)[0]=std::min((*extent)[0],x);
            (*extent)[1]=std::max((*extent)[1],x);
        }
    }
    return extent;
}

// Projected convex hull of the skull sections. Hair hanging below the jaw is
// transported separately so the hull cannot round off the authored hem.
std::vector<Point2> projectedHull(std::vector<Point2> points) {
    std::ranges::sort(points,[](Point2 a,Point2 b) {
        return a[0]<b[0] || (a[0]==b[0] && a[1]<b[1]);
    });
    std::vector<Point2> hull;
    const auto append=[&](Point2 p,std::size_t begin) {
        while (hull.size()>=begin+2) {
            const auto a=hull[hull.size()-2], b=hull.back();
            if ((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])>0.) break;
            hull.pop_back();
        }
        hull.push_back(p);
    };
    for (const auto p : points) append(p,0);
    const auto lower=hull.size()-1;
    for (auto i=points.rbegin()+1;i!=points.rend();++i) append(*i,lower);
    hull.pop_back();
    return hull;
}
}

Json applyRearCap(Json state, Json program, Task task) {
    core::settings::SettingsJsonArray silhouettes;
    const auto targets=indexNative(state,"targets").sourceArrayValue();
    const auto keys=indexNative(state,"native_head_keys").sourceArrayValue();
    const auto skin=indexNative(indexNative(program,"hierarchy"),"face_origin");
    if (skin.is_null()) return silhouettes;
    const auto fitValue=program.sourceObjectRef().if_contains("head_surface_fit");
    if (!fitValue || !fitValue->is_array()) return silhouettes;
    const auto fit=ngRigNumbers(*fitValue);
    if (fit.size()!=5) return silhouettes;
    const auto anatomyValue=program.sourceObjectRef().if_contains("head_anatomical_fit");
    if (!anatomyValue || !anatomyValue->is_array()) return silhouettes;
    const auto coordinates=ngRigNumbers(*anatomyValue);
    if (coordinates.size()!=6) return silhouettes;
    const head_template::Fit anatomy{coordinates[0],coordinates[1],coordinates[2],
        coordinates[3],coordinates[4],coordinates[5]};
    const double faceWidth=fit[2]-fit[0], faceHeight=fit[3]-fit[1];
    const double faceCenter=std::midpoint(fit[0],fit[2]);
    std::vector<Point3> fittedScalp;
    if (const auto surface=program.sourceObjectRef().if_contains("head_fitted_surface")) {
        for (const auto& point : indexNative(*surface,"points").sourceArrayValue()) {
            const auto p=ngRigNumbers(point);
            if (p.size()!=3) {
                fittedScalp.clear();
                break;
            }
            fittedScalp.push_back({p[0],p[1],p[2]});
        }
    }
    NativeIntegerMap<std::uint32_t,Json> materials;
    for (const auto& material : indexNative(state,"materials").sourceArrayValue())
        materials[uuid(indexNative(material,"uuid"))]=material;

    for (const auto& target : targets) {
        if (stringGet(target,"reference_component","")!="hair_back") continue;
        contracts::ngRigCheckpoint(task);
        const auto id=uuid(indexNative(target,"part"));
        const auto bounds=ngRigNumbers(indexNative(materials[id],"bounds"));
        if (bounds[0]>faceCenter-.30*faceWidth || bounds[2]<faceCenter+.30*faceWidth ||
            bounds[1]>fit[1]+.16*faceHeight || bounds[3]<fit[1]+.65*faceHeight) continue;
        const auto rear=measure(ngRigPoints(indexNative(
            observation::ngRigAlphaContour(materials[id],task),"points")));
        if (rear.edges.empty()) continue;
        bool canBind=false;
        task->runOnMainThread([&] {
            auto puppet=project::incActivePuppet();
            auto part=puppet->findNodeById(id);
            canBind=part && !parameterByName(puppet,"Face::Yaw-Pitch")->getBinding(part,"deform");
        });
        if (!canBind) continue;
        const auto gridX=ngRigNumbers(indexNative(target,"xs"));
        const auto gridY=ngRigNumbers(indexNative(target,"ys"));
        const auto gridFrame=ngRigNumbers(indexNative(target,"parent_to_root"));
        const auto vertices=ngRigPoints(indexNative(target,"world"));
        const auto inverse=ngRigNumbers(indexNative(target,"root_to_local_direction"));
        // Fit the crown to the upper painted contour. Treat lower locks as
        // the existing artwork, with no contour target imposed on them.
        constexpr double templateTop=head_template::crown.front()[1];
        constexpr double templateBottom=head_template::crown.back()[1];
        const double capHeight=(templateBottom-templateTop)*faceHeight;
        const double capBottom=rear.top+capHeight;
        if (capBottom>=rear.bottom) continue;
        std::vector<Point3> volume;
        const auto appendRow = [&](std::span<const Point3> row,double y,bool fitted) {
            const auto edge=rear.at(y);
            const auto range=std::ranges::minmax_element(row,{},[](Point3 p) { return p[0]; });
            const double width=(*range.max)[0]-(*range.min)[0];
            for (const auto p : row) {
                const double u=width>1e-6 ? (p[0]-(*range.min)[0])/width : .5;
                volume.push_back({std::lerp(edge[0],edge[1],u),y,
                    fitted ? p[2]-.035*faceWidth : anatomy.depth(p[2])});
            }
        };
        constexpr auto columns=head_surface_reference::columns;
        if (fittedScalp.size()==head_surface::rowCount*columns) {
            const double top=fittedScalp.front()[1];
            for (std::size_t i=0;i<fittedScalp.size();i+=columns) {
                const double y=rear.top+fittedScalp[i][1]-top;
                if (y>capBottom) break;
                appendRow({fittedScalp.data()+i,columns},y,true);
            }
        } else {
            const auto& crown=head_template::crown;
            for (std::size_t begin=0;begin<crown.size();) {
                std::size_t end=begin+1;
                while (end<crown.size() && crown[end][1]==crown[begin][1]) ++end;
                appendRow({crown.data()+begin,end-begin},
                    rear.top+(crown[begin][1]-templateTop)*faceHeight,false);
                begin=end;
            }
        }
        if (volume.size()<3) continue;

        for (const auto& key : keys) {
            if (indexNative(key,"grid")!=indexNative(target,"grid")) continue;
            const auto pose=ngRigPoint(indexNative(key,"value"));
            const auto matrix=ngRigNumbers(indexNative(key,"projection"));
            const auto project=[&](Point3 p) {
                return head_surface::project(p,{matrix.begin(),matrix.size()});
            };
            std::vector<Point2> projected;
            projected.reserve(volume.size());
            for (const auto p : volume) projected.push_back(project(p));
            const auto hull=projectedHull(std::move(projected));
            compat::Slice<Point2> outline;
            for (const auto p : hull) outline.append(p);
            silhouettes.append(objectEvidence({{stringLiteralEvidence("part"),id},
                {stringLiteralEvidence("key"),indexNative(key,"key")},
                {stringLiteralEvidence("outline"),contracts::ngRigPointsJson(outline)}}));
            compat::Slice<Point2> offsets;
            const auto packed=ngRigNumbers(indexNative(key,"offsets"));
            for (std::size_t i=0;i<packed.size();i+=2) offsets.append(Point2{packed[i],packed[i+1]});
            const auto& f=gridFrame;
            const double det=f[0]*f[4]-f[1]*f[3];
            const auto gridAt=[&](Point2 p) {
                const auto offset=controls_detail::gridSample(offsets,gridX,gridY,
                    controls_detail::gridLocal(p,f,det));
                return Point2{p[0]+f[0]*offset[0]+f[1]*offset[1],p[1]+f[3]*offset[0]+f[4]*offset[1]};
            };
            const double strength=controls_detail::smoothStep(std::max(std::abs(pose[0]),std::abs(pose[1])));
            compat::Slice<float> values;
            for (const auto p : vertices) {
                const auto before=gridAt(p);
                Point2 delta{0.,0.};
                const double height=std::clamp((p[1]-rear.top)/capHeight,0.,1.);
                const double cap=1.-controls_detail::smoothStep((height-.54)/.46);
                if (strength>0. && cap>0.) {
                    const auto edge=rear.at(p[1]);
                    const auto left=gridAt({edge[0],p[1]});
                    const auto right=gridAt({edge[1],p[1]});
                    const double width=edge[1]-edge[0];
                    const double u=width>1e-6 ? std::clamp((p[0]-edge[0])/width,0.,1.) : .5;
                    const auto extent=horizontalExtent(hull,before[1]);
                    if (extent) {
                        const double leftWeight=1.-controls_detail::smoothStep(u/.55);
                        const double rightWeight=controls_detail::smoothStep((u-.45)/.55);
                        delta[0]=strength*cap*(leftWeight*((*extent)[0]-left[0])+
                            rightWeight*((*extent)[1]-right[0]));
                    }
                }
                if (delta[0]!=0. && indexNative(target,"grid_dynamic").as_bool()) {
                    auto q=p;
                    for (unsigned iter=0;iter<16;++iter) {
                        const auto actual=gridAt(q);
                        for (std::size_t axis=0;axis<2;++axis) q[axis]+=before[axis]+delta[axis]-actual[axis];
                    }
                    delta={q[0]-p[0],q[1]-p[1]};
                }
                values.append(static_cast<float>(inverse[0]*delta[0]+inverse[1]*delta[1]));
                values.append(static_cast<float>(inverse[2]*delta[0]+inverse[3]*delta[1]));
            }
            const auto indices=ngRigNumbers(indexNative(key,"key"));
            task->runOnMainThread([&] {
                auto puppet=project::incActivePuppet();
                auto part=puppet->findNodeById(id);
                auto parameter=parameterByName(puppet,"Face::Yaw-Pitch");
                auto context=editorContext({part});
                context->parameters({parameter});
                context->keyPoint({static_cast<unsigned>(indices[0]),static_cast<unsigned>(indices[1])});
                context->hasExplicitKeyPoint(true);
                commandSource([&] {
                    return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                        compat::RetainedString::fromStaticLiteral("deform"),values,true,false);
                },[&] { return context; });
            });
        }
    }
    return silhouettes;
}
}
