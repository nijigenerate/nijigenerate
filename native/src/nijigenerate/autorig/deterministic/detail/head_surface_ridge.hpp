#pragma once

#include "head_surface_lower.hpp"

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

inline double curveAt(std::span<const Point2> points, double y) {
    if (y<=points.front()[1]) return points.front()[0];
    if (y>=points.back()[1]) return points.back()[0];
    const auto b=std::ranges::upper_bound(points,y,{},[](Point2 p) {return p[1];});
    const auto a=b-1;
    return std::lerp((*a)[0],(*b)[0],(y-(*a)[1])/((*b)[1]-(*a)[1]));
}

struct Ridge {
    PoseFrame frame;
    // Each curve is stored from upper to lower projected Y.
    std::array<std::vector<Point2>,3> curves;
    std::array<Point2,4> anchors{};
    Point2 originalJaw{};
    std::vector<Point2> lower;
    std::vector<double> cumulative;
    double jawPosition=0.;
    Point2 depthEnd{};

    Point2 at(std::size_t interval, double t) const {
        if (interval && !lower.empty()) {
            const double wanted=interval==1 ? t*jawPosition :
                std::lerp(jawPosition,cumulative.back(),t);
            auto end=std::ranges::upper_bound(cumulative,wanted);
            if (end==cumulative.end()) return lower.back();
            const auto i=static_cast<std::size_t>(end-cumulative.begin());
            if (i==0) return lower.front();
            const double u=(wanted-cumulative[i-1])/(cumulative[i]-cumulative[i-1]);
            return {std::lerp(lower[i-1][0],lower[i][0],u),std::lerp(lower[i-1][1],lower[i][1],u)};
        }
        const auto& curve=curves[interval];
        const double y=std::lerp(curve.front()[1],curve.back()[1],std::clamp(t,0.,1.));
        return frame.world({curveAt(curve,y),y});
    }
};

inline std::optional<Ridge> projectedRidge(std::span<const Point3> mesh,
    std::array<Point3,4> joints, std::span<const Point3> lowerSection,
    std::span<const Point2> rawProjection, std::span<const double> projection, int side,
    Point2 posedUpper, Point2 posedChin, double tolerance) {
    if (mesh.size()!=rowCount*reference::columns || projection.size()!=8) return {};
    const auto q=project(joints[0],projection), e=project(joints[1],projection), j=project(joints[3],projection);
    const double length=posedChin[1]-posedUpper[1];
    if (length<1e-6 || !(q[1]+1e-6<e[1] && e[1]+1e-6<j[1])) return {};
    Ridge result;
    result.frame={posedUpper,side};
    const auto screen=[&](Point3 p) {
        const auto xy=project(p,projection);
        // Blend only the endpoint errors. A pose-wide similarity rotates the
        // ear and its depth section away from the upper-neck attachment.
        const double upper=ease((e[1]-xy[1])/(e[1]-q[1]));
        const double lower=ease((xy[1]-e[1])/(j[1]-e[1]));
        return result.frame.section({xy[0]+upper*(posedUpper[0]-q[0])+lower*(posedChin[0]-j[0]),
            xy[1]+upper*(posedUpper[1]-q[1])+lower*(posedChin[1]-j[1])});
    };
    const Point2 upper{0.,0.}, ear=result.frame.section(e), chin=result.frame.section(posedChin);
    auto jaw=screen(joints[2]);
    result.originalJaw=result.frame.world(jaw);
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
    result.curves[1]=convex(ear,jaw);
    result.curves[2]=convex(jaw,chin);
    if (upperGuide.empty() || result.curves[1].empty() || result.curves[2].empty()) return {};
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
    auto& curve=result.curves[0];
    for (std::size_t i=0;i+1<upperGuide.size();++i) {
        const auto a=upperGuide[i], b=upperGuide[i+1];
        const double span=b[1]-a[1];
        constexpr int subdivisions=16;
        for (int j=0;j<subdivisions;++j) {
            const double t=double(j)/subdivisions, u=1.-t;
            const double y=std::lerp(a[1],b[1],t);
            const double x=std::lerp(a[0],b[0],t*t*(3.-2.*t))+
                span*t*u*(u*tangents[i]-t*tangents[i+1]);
            curve.push_back({std::max(x,outer(y)),y});
        }
    }
    curve.front()=upper;
    curve.push_back(ear);
    result.anchors={posedUpper,result.frame.world(ear),result.frame.world(jaw),posedChin};
    if (lowerSection.size()>=2) {
        const auto blue=blueChinPath(mesh,rawProjection,lowerSection.back(),joints[1]);
        if (!blue.empty()) {
            auto append=[&](Point3 p) {
                const auto q=project(p,projection);
                double distance=0.;
                if (!result.lower.empty()) {
                    distance=std::hypot(q[0]-result.lower.back()[0],q[1]-result.lower.back()[1]);
                    if (distance<1e-8) return;
                }
                result.lower.push_back(q);
                result.cumulative.push_back(distance+(result.cumulative.empty()?0.:result.cumulative.back()));
            };
            for (const auto p : lowerSection) append(p);
            result.depthEnd=result.lower.back();
            for (const auto p : blue) append(p);
            const double length=result.cumulative.back();
            if (result.lower.size()>2 && length>0.) {
                const auto jawPoint=project(joints[2],projection);
                auto nearest=std::ranges::min_element(result.lower,{},[&](Point2 p) {
                    return std::hypot(p[0]-jawPoint[0],p[1]-jawPoint[1]);
                });
                result.jawPosition=std::clamp(result.cumulative[nearest-result.lower.begin()],.05*length,.95*length);
                result.anchors[2]=result.at(1,1.);
            }
        }
    }
    return result;
}
}
