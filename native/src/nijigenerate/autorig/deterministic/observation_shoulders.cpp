#include "detail/observation.hpp"
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <array>
#include <utility>
namespace nijigenerate::autorig::deterministic::observation {
using namespace detail;
namespace {
struct SideIdHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(value) > sizeof(std::size_t) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
}
Json ngRigPrepareLayeredEars(Json state, Context context) {
    if (auto options=object(state).if_contains("options"))
        if (auto selected=object(*options).if_contains("ear_surface_pairs")) {
            core::settings::SettingsJsonArray result;
            for (auto pair:selected->as_array()) {
                pair=Json(object(pair).duplicate());
                const auto source=ngRigUnsigned(at(pair,"source")),target=ngRigUnsigned(at(pair,"target"));
                bool hasSource=false,hasTarget=false;
                for (auto material:at(state,"materials").as_array()) if(!at(material,"static").as_bool()) {
                    hasSource|=ngRigUnsigned(at(material,"uuid"))==source;
                    hasTarget|=ngRigUnsigned(at(material,"uuid"))==target;
                }
                require(source!=target&&hasSource&&hasTarget,"Manual ear attachment requires two active Parts",20);
                for (auto prior:result) require(at(prior,"source")!=at(pair,"source"),"Duplicate manual ear attachment source",21);
                object(pair)["kind"]="layered_ear";object(pair)["matching"]=true;object(pair)["manual"]=true;
                result.append(pair);
            }
            for (auto pair:result) {
                auto cursor=at(pair,"target");
                for(std::size_t step=0;step<=result.size();++step) {
                    require(cursor!=at(pair,"source"),"Cyclic manual ear attachments",28);
                    bool next=false;
                    for(auto other:result) if(at(other,"source")==cursor) {cursor=at(other,"target");next=true;break;}
                    if(!next)break;
                }
            }
            return result;
        }
    // A common contour and overlapping support identify two surfaces of one ear.
    // Sort by support extent so attachment links form a directed forest.
    compat::Slice<Json> ears;
    for (auto material : at(state, "materials").as_array())
        if (!at(material, "static").as_bool() && at(material, "role").as_string().view() == "ear")
            ears.append(material);
    const auto area = [](Json m) {
        auto b = ngRigNumbers(at(m, "bounds")); return (b[2] - b[0]) * (b[3] - b[1]);
    };
    if (ears.size()>1) std::sort(ears.begin(), ears.end(), [&](Json a, Json b) {
        return area(a) != area(b) ? area(a) > area(b) : ngRigUnsigned(at(a,"uuid")) < ngRigUnsigned(at(b,"uuid"));
    });
    compat::Slice<compat::Slice<Point2>> contours;
    for (auto ear : ears) contours.append(ngRigPoints(at(ngRigAlphaContour(ear, context), "points")));
    const auto principalAxis = [](Json material) {
        const auto points = ngRigPoints(at(material,"cloud"));
        Point2 center{0.,0.};
        for (auto p:points) { center[0]+=p[0]/points.size(); center[1]+=p[1]/points.size(); }
        double xx=0.,xy=0.,yy=0.;
        for (auto p:points) {const double x=p[0]-center[0],y=p[1]-center[1];xx+=x*x;xy+=x*y;yy+=y*y;}
        const double angle=.5*std::atan2(2*xy,xx-yy);
        return Point2{std::cos(angle),std::sin(angle)};
    };
    core::settings::SettingsJsonArray pairs;
    for (std::size_t i = 1; i < ears.size(); ++i) {
        auto a = ngRigNumbers(at(ears[i], "bounds"));
        double best = 0; Json selected;
        for (std::size_t j = 0; j < i; ++j) {
            ngRigCheckpoint(context);
            auto b = ngRigNumbers(at(ears[j], "bounds"));
            const double width = std::max(0., std::min(a[2], b[2]) - std::max(a[0], b[0]));
            const double height = std::max(0., std::min(a[3], b[3]) - std::max(a[1], b[1]));
            if (width * height < .6 * std::min(area(ears[i]), area(ears[j]))) continue;
            const double tolerance = .025 * std::min(a[2] - a[0], b[2] - b[0]);
            std::size_t matched = 0;
            double low = INFINITY, high = -INFINITY;
            for (auto p : contours[i]) {
                double distance = INFINITY;
                for (auto q : contours[j]) distance = std::min(distance, square(p[0]-q[0])+square(p[1]-q[1]));
                if (distance <= square(tolerance)) { ++matched; low = std::min(low,p[1]); high = std::max(high,p[1]); }
            }
            const double fraction = contours[i].empty() ? 0 : double(matched) / contours[i].size();
            const bool sharedContour = fraction >= .25 && high-low >= .25*std::min(a[3]-a[1],b[3]-b[1]);
            double coverage=0.,alignment=0.;
            if (!sharedContour) {
                // Interior fur and front surfaces need not share the outer
                // silhouette. Require painted containment and a common long
                // axis, so a crossing ornament is not attached by its box alone.
                const auto u=principalAxis(ears[i]),v=principalAxis(ears[j]);
                alignment=std::abs(u[0]*v[0]+u[1]*v[1]);
                if (alignment>=.8 && area(ears[i])>=.15*area(ears[j]))
                    coverage=ngRigAlphaCoverage(ears[j],ngRigPoints(at(ears[i],"cloud")),
                        compat::RetainedString::fromStaticLiteral("alpha_runs_32"),nullptr);
            }
            bool commonTip=false;
            double tipDistance=0.;
            if (coverage>=.55&&coverage<.7&&alignment>=.95) {
                const auto axis=principalAxis(ears[i]);
                const auto poles=[&](Json material) {
                    const auto points=ngRigPoints(at(material,"cloud"));
                    double lo=INFINITY,hi=-INFINITY;
                    for(auto p:points) {const double t=p[0]*axis[0]+p[1]*axis[1];lo=std::min(lo,t);hi=std::max(hi,t);}
                    std::array<Point2,2> ends{Point2{0.,0.},Point2{0.,0.}};std::array<std::size_t,2> counts{};
                    for(auto p:points) {const double t=p[0]*axis[0]+p[1]*axis[1];
                        for(std::size_t k=0;k<2;++k)if(k?t>=hi-.05*(hi-lo):t<=lo+.05*(hi-lo)) {
                            ends[k][0]+=p[0];ends[k][1]+=p[1];++counts[k];
                        }
                    }
                    for(std::size_t k=0;k<2;++k)for(auto& value:ends[k])value/=counts[k];
                    return std::make_pair(ends,hi-lo);
                };
                const auto pa=poles(ears[i]),pb=poles(ears[j]);
                tipDistance=std::min(std::hypot(pa.first[0][0]-pb.first[0][0],pa.first[0][1]-pb.first[0][1]),
                    std::hypot(pa.first[1][0]-pb.first[1][0],pa.first[1][1]-pb.first[1][1]));
                commonTip=tipDistance<=.08*std::min(pa.second,pb.second);
            }
            const bool nestedSurface=coverage>=.7||commonTip;
            const double score=sharedContour?fraction:.2*coverage;
            if ((!sharedContour&&!nestedSurface)||score<=best) continue;
            best = score;
            core::settings::SettingsJsonObject pair;
            pair["source"] = at(ears[i], "uuid"); pair["target"] = at(ears[j], "uuid");
            pair["kind"] = "layered_ear"; pair["matching"] = true;
            pair["matched_fraction"] = fraction; pair["matched_span"] = matched?high-low:0.;
            pair["surface_relation"] = sharedContour?"shared_contour":commonTip?"overlap_with_common_tip":"painted_containment";
            pair["painted_coverage"] = coverage; pair["axis_alignment"] = alignment;
            pair["tip_distance"] = tipDistance;
            pair["contour_tolerance"] = tolerance; selected = Json(pair);
        }
        if (!selected.is_null()) pairs.append(selected);
    }
    return pairs;
}

Json ngRigPrepareShoulders(Json state, Json program, Context context) {
    core::settings::SettingsJsonArray pairs;
    if (ngRigString(program,compat::RetainedString::fromStaticLiteral("kind"),
            compat::RetainedString::fromStaticLiteral("humanoid")).view() != "humanoid") return pairs;
    Json body, bodyContour; std::size_t greatest = 0;
    for (auto material : at(state, "materials").as_array())
        if (!at(material, "static").as_bool() && at(material, "role").as_string().view() == "torso") {
            auto support = object(material).if_contains("alpha_runs_32"); if (!support) continue;
            std::size_t count = 0; auto runs = support->as_array();
            for (std::size_t i = 1; i < runs.size(); i += 2) count += static_cast<std::size_t>(ngRigUnsigned(runs[i]));
            if (count > greatest) { greatest = count; body = material; }
        }
    if (!greatest) return pairs; bodyContour = ngRigAlphaContour(body, context, true);
    auto evidence = at(state, "evidence");
    auto landmark = [&](std::string_view name) {
        auto p = ngRigPoint(at(at(at(evidence, "landmarks"), name), "xy"));
        auto matrix = at(evidence, "source_to_model").as_array(); auto x = ngRigNumbers(matrix[0]), y = ngRigNumbers(matrix[1]);
        return Point2{x[0] * p[0] + x[1] * p[1] + x[2], y[0] * p[0] + y[1] * p[1] + y[2]};
    };
    auto neck = landmark("neck_base"), chest = landmark("chest"); Point2 up{neck[0] - chest[0], neck[1] - chest[1]};
    double norm = std::sqrt(square(up[0]) + square(up[1])); require(norm > 0, "Unresolved shoulder up axis",222);
    for (auto& v : up) v /= norm; Point2 tangent{-up[1], up[0]};
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, compat::RetainedStringSlice,
        SideIdHash, std::equal_to<std::uint64_t>> sides;
    for (auto carrier : at(program, "carriers").as_array())
        sides[ngRigUnsigned(at(carrier, "part"))] = at(carrier, "side").as_string();
    for (auto material : at(state, "materials").as_array()) {
        if (at(material, "static").as_bool() || at(material, "role").as_string().view() != "arm"
            || !object(material).if_contains("alpha_runs_32")) continue;
        auto sideEntry = sides.find(ngRigUnsigned(at(material, "uuid")));
        if (sideEntry == sides.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/observation.d"),228);
        auto side = sideEntry->second; if (side.view() != "L" && side.view() != "R") continue;
        const auto originName = contracts::detail::allocateContractStorage([&] {
            return "shoulder." + std::string(side.view());
        });
        const auto origin = landmark(originName);
        const auto elbowName = contracts::detail::allocateContractStorage([&] {
            return "elbow." + std::string(side.view());
        });
        const auto elbow = landmark(elbowName);
        Point2 axis{elbow[0] - origin[0], elbow[1] - origin[1]}; double length = std::sqrt(square(axis[0]) + square(axis[1]));
        require(length > 0, "Degenerate upper arm",232); for (auto& v : axis) v /= length;
        auto region = [&](Json contour) {
            compat::Slice<Point2> result; auto points = ngRigPoints(at(contour, "points")), normals = ngRigPoints(at(contour, "normals"));
            for (std::size_t i = 0; i < points.size(); ++i) {
                auto p = points[i]; Point2 delta{p[0] - origin[0], p[1] - origin[1]};
                if (std::sqrt(square(delta[0]) + square(delta[1])) < .30 * length && delta[0] * axis[0] + delta[1] * axis[1] < .12 * length
                    && normals[i][0] * up[0] + normals[i][1] * up[1] > .5) result.append(p);
            }
            return result;
        };
        auto proximal = region(ngRigAlphaContour(material, context, true)), torso = region(bodyContour);
        std::size_t matched = 0; double low = std::numeric_limits<double>::infinity(), high = -std::numeric_limits<double>::infinity();
        for (auto p : proximal) {
            double nearest = std::numeric_limits<double>::infinity();
            for (auto q : torso) nearest = std::min(nearest, square(p[0] - q[0]) + square(p[1] - q[1]));
            if (nearest < square(.07 * length)) {
                ++matched; double station = p[0] * tangent[0] + p[1] * tangent[1]; low = std::min(low, station); high = std::max(high, station);
            }
        }
        double fraction = proximal.empty() ? 0. : static_cast<double>(matched) / proximal.size(), span = matched ? high - low : 0.;
        core::settings::SettingsJsonObject pair; pair["source"] = at(body, "uuid"); pair["target"] = at(material, "uuid");
        pair["side"] = Json(core::settings::SettingsString(side));
        pair["matching"] = fraction >= .6 && span >= .18 * length; pair["matched_fraction"] = fraction; pair["matched_span"] = span;
        pair["upper_arm_length"] = length; pair["origin"] = pointJson(origin); pair["up"] = pointJson(up);
        pair["tangent"] = pointJson(tangent); pair["arm_axis"] = pointJson(axis); pairs.append(Json(pair));
    }
    return pairs;
}

Json ngRigPrepareHipAttachments(Json state, Json program, Context context) {
    core::settings::SettingsJsonArray result;
    if (at(program,"kind").as_string().view() != "humanoid") return result;
    const auto materials = at(state,"materials").as_array();
    const auto landmarks = at(at(state,"evidence"),"landmarks");
    const auto torsoId = at(at(program,"hierarchy"),"body_origin");
    Json torso;
    for (auto material : materials)
        if (at(material,"uuid")==torsoId && at(material,"role").as_string().view()=="torso")
            torso=material;
    if (torso.is_null() || !object(torso).if_contains("alpha_runs_32")) return result;
    const auto support = compat::RetainedString::fromStaticLiteral("alpha_runs_32");
    const auto point = [&](const std::string& name) {
        return ngRigPoint(at(at(landmarks,name),"xy"));
    };
    for (const std::string_view side : {"R","L"}) {
        const auto hip=point("hip."+std::string(side)), knee=point("knee."+std::string(side));
        Point2 axis{knee[0]-hip[0],knee[1]-hip[1]};
        const double length=std::hypot(axis[0],axis[1]);
        if (length<=0.) continue;
        for (auto& v : axis) v/=length;
        const Point2 across{axis[1],-axis[0]};
        const auto projection = [&](Point2 position, Point2 direction) {
            return (position[0]-hip[0])*direction[0]+(position[1]-hip[1])*direction[1];
        };
        double bestScore=0.;
        Json best;
        for (auto carrier : at(program,"carriers").as_array()) {
            if (at(carrier,"role").as_string().view()!="leg" || at(carrier,"side").as_string().view()!=side)
                continue;
            Json thigh;
            for (auto material : materials)
                if (at(material,"uuid")==at(carrier,"part")) { thigh=material; break; }
            if (thigh.is_null() || object(thigh).if_contains("receiver") ||
                !object(thigh).if_contains("alpha_runs_32")) continue;
            ngRigCheckpoint(context);
            const auto cloud=ngRigPoints(at(thigh,"cloud"));
            if (cloud.empty()) continue;
            double top=INFINITY,bottom=-INFINITY;
            for (const auto p : cloud) {
                const double y=projection(p,axis);
                top=std::min(top,y); bottom=std::max(bottom,y);
            }
            if (top>.40*length || bottom<.55*length) continue;
            compat::Slice<Point2> proximal, distal;
            for (const auto p : cloud) {
                const double y=projection(p,axis);
                if (y<=top+.42*length) proximal.append(p);
                if (y>=top+.72*length && y<=top+1.05*length) distal.append(p);
            }
            if (proximal.empty() || distal.empty()) continue;
            compat::Slice<bool> overlap;
            const double coverage=ngRigAlphaCoverage(torso,proximal,support,&overlap);
            if (coverage<.38 || ngRigAlphaCoverage(torso,distal,support)>.50) continue;
            double left=INFINITY,right=-INFINITY;
            std::array<double,4> region{INFINITY,INFINITY,-INFINITY,-INFINITY};
            for (std::size_t i=0;i<proximal.size();++i) if (overlap[i]) {
                const auto p=proximal[i];
                const auto x=projection(p,across);
                left=std::min(left,x); right=std::max(right,x);
                region[0]=std::min(region[0],p[0]); region[1]=std::min(region[1],p[1]);
                region[2]=std::max(region[2],p[0]); region[3]=std::max(region[3],p[1]);
            }
            if (right-left<.19*length) continue;
            const double score=(right-left)*coverage;
            if (score<=bestScore) continue;
            bestScore=score;
            core::settings::SettingsJsonArray seamRegion;
            const double margin=.025*length;
            for (std::size_t i=0;i<region.size();++i)
                seamRegion.append(region[i]+(i<2 ? -margin : margin));
            core::settings::SettingsJsonObject pair;
            pair["kind"]="hip_attachment";
            pair["source"]=at(thigh,"uuid");
            pair["target"]=torsoId;
            pair["side"]=core::settings::SettingsString(compat::RetainedString(side));
            pair["matching"]=true;
            pair["matched_fraction"]=coverage;
            pair["matched_span"]=right-left;
            pair["seam_region"]=Json(seamRegion);
            best=Json(pair);
        }
        if (!best.is_null()) result.append(best);
    }
    return result;
}
}
