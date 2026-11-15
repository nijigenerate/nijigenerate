#pragma once

#include "head_surface_fit.hpp"
#include "../contracts_json.hpp"

namespace nijigenerate::autorig::deterministic::hair_surface {
using contracts::Point2;
using contracts::Point3;
using Json = core::settings::SettingsJsonValue;

struct Profile {
    double center = 0.;
    double radius = 0.;
    double rim = 0.;
    double relief = 0.;
};

inline constexpr std::size_t verticalBands = 10;
inline constexpr std::array lateral{0., .20, .40, .60, .78, .92, 1.08, 1.24, 1.40};

struct Interval {
    std::size_t index;
    double fraction;
};

inline Interval interval(double x, std::size_t count) {
    x = std::clamp(x, 0., static_cast<double>(count-1));
    const auto index = std::min(static_cast<std::size_t>(x), count-2);
    return {index, x-index};
}

inline Interval lateralInterval(double u) {
    u = std::clamp(u, lateral.front(), lateral.back());
    const auto end = std::ranges::upper_bound(lateral, u);
    const auto index = std::min(static_cast<std::size_t>(end-lateral.begin()-1), lateral.size()-2);
    return {index, (u-lateral[index])/(lateral[index+1]-lateral[index])};
}

inline double fade(double t) { return t*t*(3.-2.*t); }

inline double frontArc(double u) {
    constexpr double shoulder = .92;
    constexpr double reach = .22;
    if (u <= shoulder) return std::sqrt(1.-u*u);
    const double z = std::sqrt(1.-shoulder*shoulder);
    // Continue past the lateral silhouette with a finite, matching slope.
    return z-shoulder/z*reach*(-std::expm1(-(u-shoulder)/reach));
}

struct Shell {
    head_surface::Frame frame;
    double top = 0.;
    double bottom = 0.;
    std::vector<Profile> rows;
    std::array<std::array<double,lateral.size()>,verticalBands+1> clearance{};
    std::array<double,4> bounds{};
    std::size_t columns = 0;
    std::size_t height = 0;
    std::vector<double> limited;

    Profile profile(double y) const {
        const auto [i,t] = interval((y-top)/(bottom-top)*(rows.size()-1), rows.size());
        const auto& a=rows[i];
        const auto& b=rows[i+1];
        return {std::lerp(a.center,b.center,t),std::lerp(a.radius,b.radius,t),
            std::lerp(a.rim,b.rim,t), std::lerp(a.relief,b.relief,t)};
    }

    double depth(Point2 root) const {
        if (!limited.empty()) {
            const auto [x,u]=interval((root[0]-bounds[0])/(bounds[2]-bounds[0])*(columns-1),columns);
            const auto [y,v]=interval((root[1]-bounds[1])/(bounds[3]-bounds[1])*(height-1),height);
            const auto index=y*columns+x;
            return std::lerp(std::lerp(limited[index],limited[index+1],u),
                std::lerp(limited[index+columns],limited[index+columns+1],u),v);
        }
        const auto p=frame.local(root);
        const auto row=profile(p[1]);
        const double u=std::abs(p[0]-row.center)/row.radius;
        const auto [v,t]=interval((p[1]-top)/(bottom-top)*verticalBands,verticalBands+1);
        const auto [h,s]=lateralInterval(u);
        const double a=std::lerp(clearance[v][h],clearance[v][h+1],fade(s));
        const double b=std::lerp(clearance[v+1][h],clearance[v+1][h+1],fade(s));
        return row.rim+row.relief*frontArc(u)+std::lerp(a,b,fade(t));
    }
};

struct Section {
    double y;
    double left;
    double right;
    std::vector<Point2> front;
};

inline Section sectionAt(double y, std::span<const Point3> mesh) {
    Section section{y,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),{}};
    head_surface::visitTriangles([&](const head_surface::Triangle& tri) {
        std::array<Point2,2> pair;
        std::size_t count=0;
        for (std::size_t i=0;i<3;++i) {
            const auto a=mesh[tri.vertices[i]], b=mesh[tri.vertices[(i+1)%3]];
            if ((a[1]>y)==(b[1]>y)) continue;
            const auto p=head_surface::mix(a,b,(y-a[1])/(b[1]-a[1]));
            pair[count++]={p[0],p[2]};
        }
        if (count!=2) return;
        if (pair[0][0]>pair[1][0]) std::swap(pair[0],pair[1]);
        section.left=std::min(section.left,pair[0][0]);
        section.right=std::max(section.right,pair[1][0]);
        section.front.insert(section.front.end(),pair.begin(),pair.end());
    });
    return section;
}

inline double frontDepth(const Section& section, double x) {
    double depth=-std::numeric_limits<double>::infinity();
    for (std::size_t i=0;i<section.front.size();i+=2) {
        const auto a=section.front[i],b=section.front[i+1];
        if (x<a[0] || x>b[0]) continue;
        const double span=b[0]-a[0];
        depth=std::max(depth,span<1e-9 ? std::max(a[1],b[1]) :
            std::lerp(a[1],b[1],(x-a[0])/span));
    }
    return depth;
}

inline std::optional<Shell> fit(std::span<const Point3> rootMesh, head_surface::Frame frame, double width) {
    if (rootMesh.size()!=head_surface::rowCount*head_surface_reference::columns || width<=1e-6) return {};
    std::vector<Point3> mesh;
    mesh.reserve(rootMesh.size());
    for (const auto p : rootMesh) {
        const auto q=frame.local({p[0],p[1]});
        mesh.push_back({q[0],q[1],p[2]});
    }
    const auto bounds=std::ranges::minmax_element(mesh,{},[](Point3 p){return p[1];});
    Shell shell;
    shell.frame=frame;
    shell.top=(*bounds.min)[1];
    shell.bottom=(*bounds.max)[1];
    const double height=shell.bottom-shell.top;
    if (height<=1e-6) return {};
    const auto segments=static_cast<std::size_t>(std::clamp(std::ceil(height/(.0172*width)),24.,128.));
    std::vector<Section> sections;
    std::vector<std::pair<double,Profile>> fitted;
    sections.reserve(segments+1);
    for (std::size_t i=0;i<=segments;++i) {
        const double y=std::lerp(shell.top,shell.bottom,static_cast<double>(i)/segments);
        const auto& section=sections.emplace_back(sectionAt(y,mesh));
        const double radius=(section.right-section.left)*.5;
        if (radius<.035*width) continue;
        const double center=std::midpoint(section.left,section.right);
        double a=0., z=0., aa=0., az=0.;
        unsigned samples=0;
        for (int j=-10;j<=10;++j) {
            const double u=j*.095;
            const double depth=frontDepth(section,center+u*radius);
            if (!std::isfinite(depth)) continue;
            const double arc=frontArc(std::abs(u)/1.06);
            a+=arc; z+=depth; aa+=arc*arc; az+=arc*depth;
            ++samples;
        }
        const double det=samples*aa-a*a;
        if (samples<8 || det<1e-8) continue;
        const double relief=std::max(0.,(samples*az-a*z)/det);
        fitted.push_back({y,{center,radius*1.06,(z-a*relief)/samples,relief}});
    }
    if (fitted.size()<3) return {};

    // Each height has one smooth lateral arc. Average nearby arcs before
    // applying a broad clearance field over the front of the fitted head.
    const double sigma=.065*width;
    for (const auto& section : sections) {
        Profile row;
        double total=0.;
        for (const auto& [y,p] : fitted) {
            const double d=(section.y-y)/sigma;
            const double weight=std::exp(-.5*d*d);
            row.center+=p.center*weight;
            row.radius+=p.radius*weight;
            row.rim+=p.rim*weight;
            row.relief+=p.relief*weight;
            total+=weight;
        }
        if (total<=1e-15) return {};
        shell.rows.push_back({row.center/total,row.radius/total,row.rim/total,row.relief/total});
    }
    std::array<std::array<double,lateral.size()-1>,verticalBands> deficits{};
    for (const auto& section : sections) {
        if (section.front.empty()) continue;
        const auto profile=shell.profile(section.y);
        const auto row=interval((section.y-shell.top)/height*verticalBands,verticalBands+1).index;
        for (int i=0;i<=80;++i) {
            const double x=std::lerp(section.left,section.right,i/80.);
            const double target=frontDepth(section,x);
            const double u=std::abs(x-profile.center)/profile.radius;
            const auto col=lateralInterval(u).index;
            deficits[row][col]=std::max(deficits[row][col],target-profile.rim-profile.relief*frontArc(u));
        }
    }
    for (std::size_t j=0;j<=verticalBands;++j) {
        for (std::size_t i=0;i<lateral.size();++i) {
            double margin=0.;
            for (auto y=j>0?j-1:j;y<std::min(j+1,verticalBands);++y)
                for (auto x=i>0?i-1:i;x<std::min(i+1,lateral.size()-1);++x)
                    margin=std::max(margin,deficits[y][x]);
            shell.clearance[j][i]=margin+.004*width;
        }
    }
    return shell;
}

// Constrain the common surface before any domain is sampled. The clearance
// field is a lower bound on depth, so the slope limit does not bury the hair
// behind the head where the artwork projects over it.
template<class CrownAttachment>
void limitProjection(Shell& shell, std::span<const Point3> mesh,
    std::array<double,4> bounds, double width, double slopeX, double slopeY,
    CrownAttachment attachCrown) {
    shell.bounds=bounds;
    const auto size=[&](double span) {
        return static_cast<std::size_t>(std::clamp(std::ceil(span/(.025*width)),2.,192.))+1;
    };
    shell.columns=size(bounds[2]-bounds[0]);
    shell.height=size(bounds[3]-bounds[1]);
    const double dx=(bounds[2]-bounds[0])/(shell.columns-1);
    const double dy=(bounds[3]-bounds[1])/(shell.height-1);
    std::vector<double> source, support;
    for (std::size_t j=0;j<shell.height;++j) {
        const double y=bounds[1]+j*dy;
        const auto section=sectionAt(y,mesh);
        for (std::size_t i=0;i<shell.columns;++i) {
            const double x=bounds[0]+i*dx;
            const auto [z,floor]=attachCrown(Point2{x,y},shell.depth({x,y}),frontDepth(section,x)+.004*width);
            source.push_back(z);
            support.push_back(floor);
        }
    }
    auto low=source, high=source;
    const auto extend=[&](std::size_t a,std::size_t b,double delta) {
        low[b]=std::min(low[b],low[a]+delta);
        high[b]=std::max(high[b],high[a]-delta);
        support[b]=std::max(support[b],support[a]-delta);
    };
    for (std::size_t y=0;y<shell.height;++y) {
        for (std::size_t x=1;x<shell.columns;++x) extend(y*shell.columns+x-1,y*shell.columns+x,slopeX*dx);
        for (std::size_t x=shell.columns-1;x>0;--x) extend(y*shell.columns+x,y*shell.columns+x-1,slopeX*dx);
    }
    for (std::size_t x=0;x<shell.columns;++x) {
        for (std::size_t y=1;y<shell.height;++y) extend((y-1)*shell.columns+x,y*shell.columns+x,slopeY*dy);
        for (std::size_t y=shell.height-1;y>0;--y) extend(y*shell.columns+x,(y-1)*shell.columns+x,slopeY*dy);
    }
    shell.limited.resize(source.size());
    for (std::size_t k=0;k<source.size();++k)
        shell.limited[k]=std::max(support[k],std::midpoint(low[k],high[k]));
}

inline Json encode(const Shell& shell) {
    using Array=core::settings::SettingsJsonArray;
    using Object=core::settings::SettingsJsonObject;
    Array rows, clearance;
    for (auto row : shell.rows)
        rows.append(Json(Array{row.center,row.radius,row.rim,row.relief}));
    for (const auto& row : shell.clearance)
        for (const auto value : row) clearance.append(value);
    Object result;
    result["y_range"]=Json(Array{shell.top,shell.bottom});
    result["tangent"]=Json(Array{shell.frame.tangent[0],shell.frame.tangent[1]});
    result["rows"]=Json(rows);
    result["clearance"]=Json(clearance);
    if (!shell.limited.empty()) {
        Array depth;
        for (const double z : shell.limited) depth.append(z);
        result["bounded_depth"]=Json(depth);
        result["bounds"]=Json(Array{shell.bounds[0],shell.bounds[1],shell.bounds[2],shell.bounds[3]});
        result["size"]=Json(Array{shell.columns,shell.height});
    }
    return Json(result);
}

inline Shell decode(const Json& data) {
    Shell shell;
    const auto y=contracts::ngRigNumbers(data.sourceIndexValue("y_range"));
    shell.top=y[0];
    shell.bottom=y[1];
    shell.frame.tangent=contracts::ngRigPoint(data.sourceIndexValue("tangent"));
    shell.frame.normal={-shell.frame.tangent[1],shell.frame.tangent[0]};
    for (const auto& entry : data.sourceIndexValue("rows").sourceArrayValue()) {
        const auto r=contracts::ngRigNumbers(entry);
        shell.rows.push_back({r[0],r[1],r[2],r[3]});
    }
    const auto values=contracts::ngRigNumbers(data.sourceIndexValue("clearance"));
    std::size_t i=0;
    for (auto& row : shell.clearance) for (auto& v : row) v=values[i++];
    if (const auto depth=data.sourceObjectRef().if_contains("bounded_depth")) {
        const auto bounds=contracts::ngRigNumbers(data.sourceIndexValue("bounds"));
        std::copy_n(bounds.begin(),4,shell.bounds.begin());
        const auto size=contracts::ngRigNumbers(data.sourceIndexValue("size"));
        shell.columns=static_cast<std::size_t>(size[0]);
        shell.height=static_cast<std::size_t>(size[1]);
        const auto z=contracts::ngRigNumbers(*depth);
        shell.limited.assign(z.begin(),z.end());
    }
    return shell;
}
}
