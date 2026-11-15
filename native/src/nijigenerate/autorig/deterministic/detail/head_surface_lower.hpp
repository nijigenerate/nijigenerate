#pragma once

#include "head_surface_fit.hpp"
#include <queue>

namespace nijigenerate::autorig::deterministic::head_surface {

inline double distance(Point3 a, Point3 b) {
    return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);
}

inline bool contains(Point3 p, Point3 a, Point3 b, Point3 c) {
    double uu=0., uv=0., vv=0., wu=0., wv=0.;
    for (std::size_t i=0;i<3;++i) {
        const double u=b[i]-a[i], v=c[i]-a[i], w=p[i]-a[i];
        uu+=u*u; uv+=u*v; vv+=v*v; wu+=w*u; wv+=w*v;
    }
    const double det=uu*vv-uv*uv;
    if (det<1e-12) return false;
    const double s=(wu*vv-wv*uv)/det, t=(wv*uu-wu*uv)/det;
    if (s< -1e-6 || t< -1e-6 || s+t>1.+1e-6) return false;
    return distance(p,{a[0]+s*(b[0]-a[0])+t*(c[0]-a[0]),
        a[1]+s*(b[1]-a[1])+t*(c[1]-a[1]),a[2]+s*(b[2]-a[2])+t*(c[2]-a[2])})<1e-4;
}

// Find a path on the original triangles of the blue surface. Prefer its
// lower projected outline when multiple routes lead to the same fixed chin.
inline std::vector<Point3> blueChinPath(std::span<const Point3> mesh, std::span<const Point2> view,
    Point3 end, Point3 ear) {
    constexpr auto chin=reference::faceLast*reference::columns+(reference::faceColumns-1)/2;
    constexpr auto missing=std::numeric_limits<std::size_t>::max();
    std::vector<std::vector<std::size_t>> links(mesh.size());
    std::vector<std::array<std::size_t,2>> edges;
    std::vector<bool> starts(mesh.size());
    visitTriangles([&](const Triangle& tri) {
        if (tri.skin) return;
        const auto& v=tri.vertices;
        if (contains(end,mesh[v[0]],mesh[v[1]],mesh[v[2]]))
            for (const auto i : v) starts[i]=true;
        for (std::size_t i=0;i<3;++i) {
            const auto a=v[i], b=v[(i+1)%3];
            links[a].push_back(b);
            links[b].push_back(a);
            edges.push_back(view[a][0]<=view[b][0]?std::array{a,b}:std::array{b,a});
        }
    });
    if (std::ranges::none_of(starts,[](bool found){return found;})) return {};
    std::vector<double> gap;
    for (const auto p : view) {
        double bottom=p[1];
        for (const auto edge : edges) {
            const auto a=view[edge[0]], b=view[edge[1]];
            if (p[0]<a[0] || p[0]>b[0]) continue;
            bottom=std::max(bottom,b[0]-a[0]<1e-8 ? std::max(a[1],b[1]) :
                std::lerp(a[1],b[1],(p[0]-a[0])/(b[0]-a[0])));
        }
        gap.push_back(bottom-p[1]);
    }
    using Step=std::pair<double,std::size_t>;
    std::priority_queue<Step,std::vector<Step>,std::greater<Step>> queue;
    const double infinity=std::numeric_limits<double>::infinity();
    std::vector<double> cost(mesh.size(),infinity);
    std::vector<std::size_t> next(mesh.size(),missing);
    const double span=std::max(1.,std::abs(mesh[chin][1]-ear[1]));
    double best=infinity;
    std::size_t first=missing;
    cost[chin]=0.;
    queue.emplace(0.,chin);
    while (!queue.empty()) {
        const auto [value,i]=queue.top();
        queue.pop();
        if (value!=cost[i]) continue;
        if (value>=best) break;
        if (starts[i] && value+distance(end,mesh[i])<best) {
            best=value+distance(end,mesh[i]);
            first=i;
        }
        for (const auto j : links[i]) {
            if (mesh[j][1]<ear[1]) continue;
            const double candidate=value+distance(mesh[i],mesh[j])*(1.+1.6*(gap[i]+gap[j])/span);
            if (candidate>=cost[j]) continue;
            cost[j]=candidate;
            next[j]=i;
            queue.emplace(candidate,j);
        }
    }
    if (first==missing) return {};
    std::vector<Point3> result{end};
    for (auto i=first;i!=chin;i=next[i]) result.push_back(mesh[i]);
    result.push_back(mesh[chin]);
    return result;
}

struct ProjectedSkin {
    std::vector<std::array<Point2,2>> edges;
    int side;
    explicit ProjectedSkin(std::span<const Point2> mesh, int direction) : side(direction) {
        if (mesh.size()!=rowCount*reference::columns) return;
        visitTriangles([&](const Triangle& tri) {
            if (!tri.skin) return;
            for (std::size_t i=0;i<3;++i) {
                const auto a=mesh[tri.vertices[i]], b=mesh[tri.vertices[(i+1)%3]];
                edges.push_back(a[1]<=b[1]?std::array{a,b}:std::array{b,a});
            }
        });
    }

    // Signed clearance to the near edge in the original projection.
    double clearance(Point2 p) const {
        double edge=-std::numeric_limits<double>::infinity();
        for (const auto& [a,b] : edges) {
            if (p[1]<a[1] || p[1]>b[1]) continue;
            edge=std::max(edge,b[1]-a[1]<1e-9 ? std::max(side*a[0],side*b[0]) :
                side*std::lerp(a[0],b[0],(p[1]-a[1])/(b[1]-a[1])));
        }
        return side*p[0]-edge;
    }
};
}
