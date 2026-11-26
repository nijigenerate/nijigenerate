#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace nijigenerate::autorig::deterministic::head_surface {

// Chord length is shared by both coordinates and all sections of the ridge.
// Each knot has one tangent. Limiting the coordinate slopes keeps every
// segment within the ranges of its two endpoints.
template<class Point> class ContourCurve {
public:
    void append(Point point) {
        const double distance = points.empty() ? 0. :
            std::hypot(point[0]-points.back()[0],point[1]-points.back()[1]);
        if (!points.empty() && distance<1e-8) return;
        points.push_back(point);
        lengths.push_back(length()+distance);
    }

    double length() const { return lengths.empty() ? 0. : lengths.back(); }

    void fit() {
        slopes.resize(points.size());
        for (std::size_t axis=0; axis<2; ++axis) {
            const auto secant=[&](std::size_t i) {
                return (points[i+1][axis]-points[i][axis])/(lengths[i+1]-lengths[i]);
            };
            slopes.front()[axis]=secant(0);
            slopes.back()[axis]=secant(points.size()-2);
            for (std::size_t i=1; i+1<points.size(); ++i) {
                const auto a=secant(i-1), b=secant(i);
                const double before=lengths[i]-lengths[i-1], after=lengths[i+1]-lengths[i];
                const double w1=2.*after+before, w2=after+2.*before;
                slopes[i][axis]=a*b>0. ? (w1+w2)/(w1/a+w2/b) : 0.;
            }
        }
    }

    Point at(double distance) const {
        if (distance<=0.) return points.front();
        if (distance>=length()) return points.back();
        const auto i=static_cast<std::size_t>(
            std::upper_bound(lengths.begin(),lengths.end(),distance)-lengths.begin()-1);
        const double span=lengths[i+1]-lengths[i];
        const double t=(distance-lengths[i])/span, u=1.-t;
        Point result;
        for (std::size_t axis=0; axis<2; ++axis)
            result[axis]=std::lerp(points[i][axis],points[i+1][axis],t*t*(3.-2.*t))+
                span*t*u*(u*slopes[i][axis]-t*slopes[i+1][axis]);
        return result;
    }

private:
    std::vector<Point> points, slopes;
    std::vector<double> lengths;
};
}
