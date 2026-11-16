#pragma once

#include "head_surface_reference.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

namespace nijigenerate::autorig::deterministic::head_surface {
using contracts::Point2;
using contracts::Point3;
namespace reference = head_surface_reference;

inline Point3 mix(Point3 a, Point3 b, double t) {
    return {std::lerp(a[0], b[0], t), std::lerp(a[1], b[1], t), std::lerp(a[2], b[2], t)};
}

struct Frame {
    Point2 tangent{1., 0.};
    Point2 normal{0., 1.};

    Point2 local(Point2 world) const {
        return {tangent[0]*world[0]+tangent[1]*world[1],
            normal[0]*world[0]+normal[1]*world[1]};
    }

    Point3 model(Point3 local) const {
        return {tangent[0]*local[0]+normal[0]*local[1],
            tangent[1]*local[0]+normal[1]*local[1], local[2]};
    }
};

inline Frame alignedFrame(Point2 a, Point2 b) {
    if (a[0] > b[0]) std::swap(a, b);
    const auto length = std::hypot(b[0]-a[0], b[1]-a[1]);
    if (length < 1e-6) return {};
    const double x = (b[0]-a[0])/length, y = (b[1]-a[1])/length;
    return {{x,y},{-y,x}};
}

struct Row {
    double y;
    Point2 bounds;
};

inline Point2 sample(std::span<const Row> rows, double y) {
    if (y <= rows.front().y) return rows.front().bounds;
    if (y >= rows.back().y) return rows.back().bounds;
    auto end = std::ranges::upper_bound(rows, y, {}, &Row::y);
    const auto& a = *(end-1);
    return {std::lerp(a.bounds[0], end->bounds[0], (y-a.y)/(end->y-a.y)),
        std::lerp(a.bounds[1], end->bounds[1], (y-a.y)/(end->y-a.y))};
}

inline std::vector<Row> outlineRows(const compat::Slice<Point2>& outline, const Frame& frame, double pixel) {
    std::map<long, Row> bins;
    const auto pitch=std::max(2.5*pixel,1e-7);
    for (const auto world : outline) {
        const auto p = frame.local(world);
        auto [it, inserted] = bins.try_emplace(std::lround(p[1]/pitch), Row{p[1],{p[0],p[0]}});
        if (!inserted) {
            it->second.bounds[0] = std::min(it->second.bounds[0], p[0]);
            it->second.bounds[1] = std::max(it->second.bounds[1], p[0]);
        }
    }
    std::vector<Row> rows;
    for (const auto& [key, row] : bins) {
        if (row.bounds[1]-row.bounds[0] > 1e-7) rows.push_back(row);
    }
    return rows;
}

inline constexpr std::size_t rowCount = reference::packed.size()/(3*reference::columns);
struct Triangle {
    std::array<std::size_t,3> vertices;
    bool skin;
};

template<class Visit> void visitTriangles(Visit&& visit) {
    for (std::size_t row=0; row+1<rowCount; ++row) {
        for (std::size_t col=0; col<reference::columns; ++col) {
            const auto a=row*reference::columns+col;
            const auto b=row*reference::columns+(col+1)%reference::columns;
            const auto c=a+reference::columns, d=b+reference::columns;
            const bool skin=row>=reference::faceFirst && row<reference::faceLast &&
                col+1<reference::faceColumns;
            visit(Triangle{{a,b,d},skin});
            visit(Triangle{{a,d,c},skin});
        }
    }
}

// Sample the fixed facial surface along the front view's depth axis. Grid
// padding does not belong to the visible face; extend the closest rim depth
// there instead of sampling the back of the head.
inline double frontDepthAt(std::span<const Point3> mesh, Point2 p) {
    double front = -std::numeric_limits<double>::infinity();
    visitTriangles([&](const Triangle& triangle) {
        if (!triangle.skin) return;
        const auto [ia, ib, ic] = triangle.vertices;
        const auto a = mesh[ia], b = mesh[ib], c = mesh[ic];
        const double ux=b[0]-a[0], uy=b[1]-a[1], vx=c[0]-a[0], vy=c[1]-a[1];
        const double det=ux*vy-uy*vx;
        if (std::abs(det)<1e-10) return;
        const double x=p[0]-a[0], y=p[1]-a[1];
        const double u=(x*vy-y*vx)/det, v=(ux*y-uy*x)/det;
        if (u>=-1e-8 && v>=-1e-8 && u+v<=1.+1e-8)
            front=std::max(front,a[2]+u*(b[2]-a[2])+v*(c[2]-a[2]));
    });
    if (std::isfinite(front)) return front;

    double closest = std::numeric_limits<double>::infinity();
    const auto edge = [&](std::size_t ia, std::size_t ib) {
        const auto a=mesh[ia], b=mesh[ib];
        const double dx=b[0]-a[0], dy=b[1]-a[1];
        const double length=dx*dx+dy*dy;
        const double t=length>1e-10 ?
            std::clamp(((p[0]-a[0])*dx+(p[1]-a[1])*dy)/length,0.,1.) : 0.;
        const double x=p[0]-a[0]-t*dx, y=p[1]-a[1]-t*dy;
        const double distance=x*x+y*y;
        if (distance>=closest) return;
        closest=distance;
        front=std::lerp(a[2],b[2],t);
    };
    constexpr auto first=reference::faceFirst*reference::columns;
    constexpr auto last=reference::faceLast*reference::columns;
    constexpr auto right=reference::faceColumns-1;
    for (std::size_t col=0; col<right; ++col) {
        edge(first+col,first+col+1);
        edge(last+col,last+col+1);
    }
    for (std::size_t row=first; row<last; row+=reference::columns) {
        edge(row,row+reference::columns);
        edge(row+right,row+reference::columns+right);
    }
    return front;
}

inline Point3 standardPoint(std::size_t row, std::size_t col) {
    const auto i = (row*reference::columns+col)*3;
    return {reference::packed[i]/16000., reference::packed[i+1]/16000., reference::packed[i+2]/16000.};
}

inline double ease(double t) {
    t = std::clamp(t, 0., 1.);
    return t*t*t*(10.+t*(-15.+6.*t));
}

struct Fit {
    std::vector<Point3> mesh;
    std::array<std::vector<Point3>,2> earLine;
    std::array<std::vector<Point3>,2> lowerEarLine;
    std::array<Point3,2> ear{}, jaw{}, upper{};
    Point3 chin{}, nape{};

    void translate(Point3 offset) {
        const auto move = [&](Point3& p) {
            for (std::size_t i=0; i<3; ++i) p[i]+=offset[i];
        };
        for (auto& p : mesh) move(p);
        for (std::size_t i=0; i<2; ++i) {
            for (auto& p : earLine[i]) move(p);
            for (auto& p : lowerEarLine[i]) move(p);
            move(ear[i]);
            move(jaw[i]);
            move(upper[i]);
        }
        move(chin);
        move(nape);
    }
};

inline std::vector<Point3> depthSection(std::span<const Point3> mesh, double y, double z, int side,
    int direction=-1) {
    using Key = std::array<long long,2>;
    const auto key = [](Point3 p) {
        return Key{std::llround(1e5*p[0]), std::llround(1e5*p[1])};
    };
    struct Sample {
        Point3 point;
        std::vector<std::size_t> connections;
    };
    std::map<Key,std::size_t> index;
    std::vector<Sample> graph;
    const auto node = [&](Point3 p) {
        auto [it, inserted] = index.try_emplace(key(p), graph.size());
        if (inserted) graph.push_back({p,{}});
        return it->second;
    };
    std::optional<std::size_t> first, previous;
    Point3 contact{};
    double lateral = -std::numeric_limits<double>::infinity();
    visitTriangles([&](const Triangle& triangle) {
        std::array<Point3,2> hits;
        std::size_t count = 0;
        for (std::size_t i=0; i<3; ++i) {
            const auto a = mesh[triangle.vertices[i]], b = mesh[triangle.vertices[(i+1)%3]];
            if ((a[2]>z)==(b[2]>z)) continue;
            hits[count++] = mix(a,b,(z-a[2])/(b[2]-a[2]));
        }
        if (count!=2) return;
        if (hits[0][1]>hits[1][1]) std::swap(hits[0],hits[1]);
        const auto a=node(hits[0]), b=node(hits[1]);
        if (a==b) return;
        graph[a].connections.push_back(b);
        graph[b].connections.push_back(a);
        if (!(hits[0][1]<=y && y<hits[1][1])) return;
        const auto p = mix(hits[0],hits[1],(y-hits[0][1])/(hits[1][1]-hits[0][1]));
        if (side*p[0] > lateral) {
            lateral = side*p[0];
            contact = {p[0],y,z};
            first = direction<0 ? a : b;
            previous = direction<0 ? b : a;
        }
    });
    if (!first) return {};
    std::vector<Point3> result{contact};
    std::vector<bool> visited(graph.size());
    while (first && !visited[*first]) {
        const auto current = *first;
        visited[current] = true;
        auto p = graph[current].point;
        p[2] = z;
        if (std::hypot(p[0]-result.back()[0], p[1]-result.back()[1]) > 1e-5) result.push_back(p);
        first.reset();
        for (const auto next : graph[current].connections) {
            if (next==previous || visited[next] || direction*(graph[next].point[1]-p[1]) < -1e-6) continue;
            if (!first || direction*graph[next].point[1] > direction*graph[*first].point[1]) first = next;
        }
        previous = current;
    }
    return result;
}

inline std::optional<Fit> fit(std::span<const Row> painted, const Frame& frame,
                              double eyeY, double chinDepth, Point3 neck) {
    if (painted.size()<3) return {};
    constexpr double earRow=std::midpoint(reference::leftEar[1],reference::rightEar[1]);
    const double top=painted.front().y, chin=painted.back().y, height=chin-top;
    double width=0.;
    for (const auto& r : painted) width=std::max(width,r.bounds[1]-r.bounds[0]);
    if (width<=1e-6 || height<=1e-6) return {};
    eyeY=std::clamp(eyeY,top+.32*height,top+.68*height);
    neck[1]=std::clamp(neck[1],std::max(eyeY+.15*height,top+.74*height),top+.94*height);
    const double earY=std::lerp(eyeY,neck[1],(earRow-.60)/(reference::nape[1]-.60));
    std::array<Row,reference::faceLast-reference::faceFirst+1> source{};
    for (std::size_t r=0; r<source.size(); ++r) {
        const auto left=standardPoint(reference::faceFirst+r,0);
        const auto right=standardPoint(reference::faceFirst+r,reference::faceColumns-1);
        source[r]={left[1],{left[0],right[0]}};
    }
    const std::array sourceY{0.,earRow,reference::nape[1],1.};
    const std::array targetY{top,earY,neck[1],chin};
    std::array<double,3> slopes{};
    for (std::size_t i=0;i<3;++i) slopes[i]=(targetY[i+1]-targetY[i])/(sourceY[i+1]-sourceY[i]);
    const std::array derivatives{slopes[0],2.*slopes[0]*slopes[1]/(slopes[0]+slopes[1]),
        2.*slopes[1]*slopes[2]/(slopes[1]+slopes[2]),slopes[2]};
    const auto mapY = [&](double y) {
        if (y<=sourceY.front()) return top+y*height;
        if (y>=sourceY.back()) return chin+(y-1.)*height;
        const auto i=static_cast<std::size_t>(std::ranges::upper_bound(sourceY,y)-sourceY.begin()-1);
        const double length=sourceY[i+1]-sourceY[i];
        const auto t=(y-sourceY[i])/length;
        return std::lerp(targetY[i],targetY[i+1],t*t*(3.-2.*t))+
            length*t*(1.-t)*((1.-t)*derivatives[i]-t*derivatives[i+1]);
    };
    const auto map = [&](Point3 p) {
        const double y=mapY(p[1]);
        const auto from=sample(source,p[1]), to=sample(painted,y);
        const double sourceCenter=std::midpoint(from[0],from[1]);
        const double center=std::midpoint(to[0],to[1]);
        const double u=std::clamp((p[0]-from[0])/std::max(from[1]-from[0],1e-6),0.,1.);
        const double beyond=p[0]-std::lerp(from[0],from[1],u);
        double x=std::lerp(to[0],to[1],u)+beyond*width*(1.-ease((p[1]-.07)/(earRow-.19)));
        if (p[1]<0.) x=std::lerp(x,center+(p[0]-sourceCenter)*width,ease(-p[1]/.14));
        if (p[1]>1.) x=std::lerp(x,neck[0]+p[0]*width,ease((p[1]-1.)/.12));
        const auto napeWeight=1.-ease(std::hypot((p[1]-reference::nape[1])/.36,
            (p[2]-reference::nape[2])/.31));
        const double z=chinDepth+width*p[2]+napeWeight*(neck[2]-chinDepth-width*reference::nape[2]);
        return Point3{x,y,z};
    };
    Fit result;
    result.mesh.reserve(rowCount*reference::columns);
    for (std::size_t r=0;r<rowCount;++r)
        for (std::size_t c=0;c<reference::columns;++c) result.mesh.push_back(map(standardPoint(r,c)));
    const std::array referenceE{reference::leftEar,reference::rightEar};
    const std::array referenceM{reference::leftJaw,reference::rightJaw};
    for (std::size_t i=0;i<2;++i) {
        const auto modelEar=map(referenceE[i]);
        auto line=depthSection(result.mesh,modelEar[1],modelEar[2],i==0 ? -1 : 1);
        auto lower=depthSection(result.mesh,modelEar[1],modelEar[2],i==0 ? -1 : 1,1);
        if (line.size()<3) return {};
        result.ear[i]=frame.model(line.front());
        for (auto& point : line) point=frame.model(point);
        for (auto& point : lower) point=frame.model(point);
        result.earLine[i]=std::move(line);
        result.lowerEarLine[i]=std::move(lower);
        result.jaw[i]=frame.model(map(referenceM[i]));
        const auto q=standardPoint(reference::faceFirst,i==0 ? 0 : reference::faceColumns-1);
        result.upper[i]=frame.model(map(q));
    }
    result.chin=frame.model(map(standardPoint(reference::faceLast,(reference::faceColumns-1)/2)));
    result.nape=frame.model(map(reference::nape));
    for (auto& point : result.mesh) point=frame.model(point);
    return result;
}
}
