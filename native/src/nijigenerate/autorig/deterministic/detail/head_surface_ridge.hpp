#pragma once

#include "head_surface_lower.hpp"
#include "head_contour_curve.hpp"

namespace nijigenerate::autorig::deterministic::head_surface {

inline Point2 project(Point3 p, std::span<const double> matrix) {
    return {matrix[0]*p[0]+matrix[2]*p[1]+matrix[4]*p[2]+matrix[6],
        matrix[1]*p[0]+matrix[3]*p[1]+matrix[5]*p[2]+matrix[7]};
}

struct PoseFrame {
    Point2 upper{};
    int side=1;

    Point2 section(Point2 p) const {
        return {side*(p[0]-upper[0]),p[1]-upper[1]};
    }
    Point2 world(Point2 p) const {
        return {upper[0]+side*p[0],upper[1]+p[1]};
    }
};

struct Ridge {
    ContourCurve<Point2> curve;
    std::array<double,4> stations{};
    std::array<Point2,4> anchors{};
    Point2 originalJaw{};
    std::optional<Point2> depthEnd;

    Point2 at(std::size_t interval, double t) const {
        return curve.at(std::lerp(stations[interval],stations[interval+1],std::clamp(t,0.,1.)));
    }
};

inline void smoothLowerGuide(std::vector<Point2>& points) {
    std::vector<double> stations(points.size());
    for (std::size_t i=1;i<points.size();++i)
        stations[i]=stations[i-1]+std::hypot(points[i][0]-points[i-1][0],points[i][1]-points[i-1][1]);
    const double length=stations.back();
    if (length<1e-8) return;
    const auto sample=[&](double s) {
        if (s<=0.) return points.front();
        if (s>=length) return points.back();
        const auto i=std::ranges::upper_bound(stations,s)-stations.begin();
        const double t=(s-stations[i-1])/(stations[i]-stations[i-1]);
        return Point2{std::lerp(points[i-1][0],points[i][0],t),
                      std::lerp(points[i-1][1],points[i][1],t)};
    };
    auto smooth=points;
    for (std::size_t i=1;i+1<points.size();++i) {
        const auto s=stations[i];
        // The shrinking window pins the ear and chin while rounding short
        // bends left by the mesh edge walk.
        const double radius=std::min({.11*length,s,length-s});
        Point2 average{0.,0.};
        for (int j=-3;j<=3;++j) {
            const auto p=sample(s+radius*j/3.);
            const double weight=(4-std::abs(j))/16.;
            average[0]+=weight*p[0];
            average[1]+=weight*p[1];
        }
        smooth[i]=average;
    }
    points=std::move(smooth);
}

inline std::optional<Ridge> projectedRidge(std::span<const Point3> mesh,
    std::array<Point3,4> joints, std::span<const Point3> lowerSection,
    std::span<const Point2> rawProjection, std::span<const double> projection, int side,
    Point2 posedUpper, Point2 posedChin, double tolerance,
    std::optional<Point2> paintedEar = {}) {
    if (mesh.size()!=rowCount*reference::columns || projection.size()!=8) return {};
    const auto q=project(joints[0],projection), e=project(joints[1],projection), j=project(joints[3],projection);
    const double length=posedChin[1]-posedUpper[1];
    if (length<1e-6 || !(q[1]+1e-6<e[1] && e[1]+1e-6<j[1])) return {};
    Ridge result;
    const PoseFrame frame{posedUpper,side};
    const auto screen=[&](Point3 p) {
        const auto xy=project(p,projection);
        // Blend only the endpoint errors. A pose-wide similarity rotates the
        // ear and its depth section away from the upper-neck attachment.
        const double upper=ease((e[1]-xy[1])/(e[1]-q[1]));
        const double lower=ease((xy[1]-e[1])/(j[1]-e[1]));
        return frame.section({xy[0]+upper*(posedUpper[0]-q[0])+lower*(posedChin[0]-j[0]),
            xy[1]+upper*(posedUpper[1]-q[1])+lower*(posedChin[1]-j[1])});
    };
    const Point2 upper{0.,0.}, ear=frame.section(paintedEar.value_or(e)), chin=frame.section(posedChin);
    auto jaw=screen(joints[2]);
    result.originalJaw=frame.world(jaw);
    const double step=std::max(tolerance,1e-6);
    if (!(step<ear[1] && ear[1]+2.*step<length)) return {};
    jaw[1]=std::clamp(jaw[1],ear[1]+step,length-step);

    std::vector<Point2> red;
    std::vector<std::array<Point2,2>> redEdges;
    const auto skinPoint=[&](std::size_t row,std::size_t col) {
        // The raster's lowest and highest rows may have several pixels
        // across. Close those terminal sections on the pinned Q and J.
        if (row==reference::faceFirst) return upper;
        if (row==reference::faceLast) return chin;
        return screen(mesh[row*reference::columns+col]);
    };
    for (auto row=reference::faceFirst;row<=reference::faceLast;++row) {
        for (std::size_t col=0;col<reference::faceColumns;++col) {
            const auto p=skinPoint(row,col);
            red.push_back(p);
            if (col+1<reference::faceColumns) redEdges.push_back({p,skinPoint(row,col+1)});
            if (row<reference::faceLast) {
                redEdges.push_back({p,skinPoint(row+1,col)});
                if (col+1<reference::faceColumns)
                    redEdges.push_back({p,skinPoint(row+1,col+1)});
            }
        }
    }
    const auto outer=[&](double y) {
        double x=-std::numeric_limits<double>::infinity();
        for (const auto& edge : redEdges) {
            const auto a=edge[0],b=edge[1];
            if (y<std::min(a[1],b[1]) || y>std::max(a[1],b[1])) continue;
            x=std::max(x,std::abs(b[1]-a[1])<1e-9 ? std::max(a[0],b[0]) :
                std::lerp(a[0],b[0],(y-a[1])/(b[1]-a[1])));
        }
        return x;
    };
    jaw[0]=std::max(jaw[0],outer(jaw[1]));
    const auto convex=[&](Point2 a,Point2 b) {
        std::vector<Point2> points{a,b};
        if (outer(a[1])>a[0]+step || outer(b[1])>b[0]+step) return std::vector<Point2>{};
        for (const auto p : red) if (a[1]<p[1] && p[1]<b[1]) points.push_back(p);
        std::ranges::sort(points,[](Point2 a,Point2 b) {
            return a[1]<b[1] || (a[1]==b[1] && a[0]>b[0]);
        });
        std::vector<Point2> hull;
        for (const auto p : points) {
            if (!hull.empty() && hull.back()[1]==p[1]) continue;
            while (hull.size()>1) {
                const auto a=hull[hull.size()-2], b=hull.back();
                if ((b[0]-a[0])*(p[1]-b[1]) > (p[0]-b[0])*(b[1]-a[1])) break;
                hull.pop_back();
            }
            hull.push_back(p);
        }
        return hull;
    };
    const auto upperGuide=convex(upper,ear);
    const auto jawGuide=convex(ear,jaw);
    const auto chinGuide=convex(jaw,chin);
    if (upperGuide.empty() || jawGuide.empty() || chinGuide.empty()) return {};
    // Interpolate Q–E in the projection plane using the facial silhouette
    // as a guide. Limit tangents to avoid extra extrema between guide points.
    const auto secant=[&](std::size_t i) {
        const auto a=upperGuide[i], b=upperGuide[i+1];
        return (b[0]-a[0])/(b[1]-a[1]);
    };
    std::vector<double> tangents(upperGuide.size());
    tangents.front()=secant(0);
    tangents.back()=secant(upperGuide.size()-2);
    for (std::size_t i=1;i+1<upperGuide.size();++i) {
        const double a=secant(i-1), b=secant(i);
        tangents[i]=a*b>0. ? 2.*a*b/(a+b) : 0.;
    }
    result.curve.append(posedUpper);
    for (std::size_t i=0;i+1<upperGuide.size();++i) {
        const auto a=upperGuide[i], b=upperGuide[i+1];
        const double span=b[1]-a[1];
        constexpr int subdivisions=16;
        for (int j=0;j<subdivisions;++j) {
            const double t=double(j)/subdivisions, u=1.-t;
            const double y=std::lerp(a[1],b[1],t);
            const double x=std::lerp(a[0],b[0],t*t*(3.-2.*t))+
                span*t*u*(u*tangents[i]-t*tangents[i+1]);
            if (i || j) result.curve.append(frame.world({std::max(x,outer(y)),y}));
        }
    }
    result.curve.append(frame.world(ear));
    result.stations[1]=result.curve.length();
    std::vector<Point2> lower;
    if (lowerSection.size()>=2) {
        const auto blue=blueChinPath(mesh,rawProjection,lowerSection.back(),joints[1]);
        if (!blue.empty()) {
            // Retarget the projected depth section as a whole. Moving only
            // its first point would leave a kink just below the ear.
            const auto alignLower = [&](Point2 p) {
                if (!paintedEar) return p;
                const auto pose = std::clamp((p[1]-e[1])/(j[1]-e[1]),0.,1.);
                const auto blend = ease(pose);
                return Point2{p[0]+std::lerp((*paintedEar)[0]-e[0],posedChin[0]-j[0],blend),
                    std::lerp((*paintedEar)[1],posedChin[1],pose)};
            };
            for (const auto p : lowerSection) lower.push_back(alignLower(project(p,projection)));
            result.depthEnd=lower.back();
            // The graph path starts at the last depth-section point.
            for (std::size_t i=1;i<blue.size();++i) lower.push_back(alignLower(project(blue[i],projection)));
            lower.front()=frame.world(ear);
            lower.back()=posedChin;
            smoothLowerGuide(lower);
            const auto jawPoint=alignLower(project(joints[2],projection));
            const auto nearest=std::ranges::min_element(lower,{},[&](Point2 p) {
                return std::hypot(p[0]-jawPoint[0],p[1]-jawPoint[1]);
            });
            for (auto it=lower.begin();it!=lower.end();++it) {
                result.curve.append(*it);
                if (it==nearest) result.stations[2]=result.curve.length();
            }
            const double length=result.curve.length()-result.stations[1];
            result.stations[2]=std::clamp(result.stations[2],
                result.stations[1]+.05*length,result.stations[1]+.95*length);
        }
    }
    if (lower.empty()) {
        for (const auto p : jawGuide) result.curve.append(frame.world(p));
        result.stations[2]=result.curve.length();
        for (const auto p : chinGuide) result.curve.append(frame.world(p));
    }
    result.stations[3]=result.curve.length();
    result.curve.fit();
    result.anchors={posedUpper,frame.world(ear),result.at(1,1.),posedChin};
    return result;
}
}
