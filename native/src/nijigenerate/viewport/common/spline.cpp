// Copyright 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "spline.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <common/nullable_string.hpp>
#include <param/binding_impl.hpp>
#include <common/object_key_policy.hpp>
#include <bit>
#include <vector>
#include <typeinfo>
#include <string>
#include <stdexcept>
#include <new>
#include <source_location>

namespace nijigenerate::viewport::common::spline {
namespace {
using namespace detail;
using nicxlive::core::common::NullableString;
template<class Operation> auto allocateTemporary(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
SplinePoint& splinePointAt(Array<SplinePoint>& values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/common/spline.d"), line);
    return values[index];
}
std::shared_ptr<SplinePoint> splinePointPointer(Array<SplinePoint>& values,
    std::size_t index, std::size_t line) {
    (void)splinePointAt(values, index, line);
    return values.retainPointer(index);
}
void prepareSplineClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CatmullSpline),
            compat::RetainedString::fromStaticLiteral(
                nicxlive::core::d_runtime::SourceObjectTypeNames<CatmullSpline>::className));
        return true;
    }();
    (void)registered;
}
// Float specializations of Phobos trigonometry.d, algebraic.d and exponential.d.
// Copyright The D Language Foundation 2000-2011; CEPHES Copyright (C) 2001
// Stephen L. Moshier, incorporated by permission; the author reserves the
// right to distribute this material elsewhere under different permissions.
// Walter Bright, Don Clugston, Iain Buclaw and David Nadlinger. BSL-1.0;
// full license: nicxlive/core/licenses/phobos-stdio.txt.
constexpr float sourcePi = 0x1.921fb6p+1f;
constexpr float sourcePiHalf = 0x1.921fb6p+0f;
constexpr float sourcePiQuarter = 0x1.921fb6p-1f;
float sourceAtan(float value) {
    constexpr std::array<float, 4> coefficients{
        -3.33329491539E-1f, 1.99777106478E-1f, -1.38776856032E-1f, 8.05374449538E-2f};
    constexpr float tanPiEighth = 0.414213562373095048801688724209698078569672f;
    constexpr float tanThreePiEighths = 2.414213562373095048801688724209698078569672f;
    if (value == 0.0f) return value;
    if (std::isinf(value)) return std::copysign(sourcePiHalf, value);
    const bool negative = std::signbit(value);
    if (negative) value = -value;
    float offset;
    if (value > tanThreePiEighths) {
        offset = sourcePiHalf;
        value = -(1.0f / value);
    } else if (value > tanPiEighth) {
        offset = sourcePiQuarter;
        value = (value - 1.0f) / (value + 1.0f);
    } else {
        offset = 0.0f;
    }
    const float squared = value * value;
    float polynomial = coefficients[3];
    for (std::size_t i = 3; i > 0; --i) {
        polynomial *= squared;
        polynomial += coefficients[i - 1];
    }
    float contribution = polynomial * squared;
    contribution *= value;
    contribution += value;
    offset += contribution;
    return negative ? -offset : offset;
}
float sourceAtan2(float y, float x) {
    if (std::isnan(x) || std::isnan(y)) return detail::nan;
    if (y == 0.0f) {
        if (x >= 0.0f && !std::signbit(x)) return std::copysign(0.0f, y);
        return std::copysign(sourcePi, y);
    }
    if (x == 0.0f) return std::copysign(sourcePiHalf, y);
    if (std::isinf(x)) {
        if (std::signbit(x))
            return std::copysign(std::isinf(y) ? 3.0f * sourcePiQuarter : sourcePi, y);
        return std::copysign(std::isinf(y) ? sourcePiQuarter : 0.0f, y);
    }
    if (std::isinf(y)) return std::copysign(sourcePiHalf, y);
    float result = sourceAtan(y / x);
    if (std::signbit(x)) result = std::signbit(y) ? result - sourcePi : result + sourcePi;
    if (result == 0.0f) return std::copysign(result, y);
    return result;
}
Vec2 transformed(const Mat4& matrix, Vec2 point) {
    const auto value = core::math::ngTransformHomogeneousPoint(matrix, {point.x, point.y, 0});
    return {value.x, value.y};
}
std::uint32_t toUnsigned(float value) {
    // Keep the authored floating-to-uint conversion as a native conversion.
    // Do not turn an unrepresentable input into a successful index zero.
    // Its selected-target invalid-input profile is not established here.
    return static_cast<std::uint32_t>(value);
}
float nearestLine(Vec2 a, Vec2 b, Vec2 c) {
    return ((c.x-a.x)*(b.x-a.x)+(c.y-a.y)*(b.y-a.y)) /
        ((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
}
BezierSegment catmullToBezier(SplinePoint* p0, SplinePoint* p1,
    SplinePoint* p2, SplinePoint* p3, float tension = 1) {
    BezierSegment result;
    result.p[0] = p1->position;
    result.p[1] = add(p1->position, divide(sub(p2->position, p0->position), 6*tension));
    result.p[2] = sub(p2->position, divide(sub(p3->position, p1->position), 6*tension));
    result.p[3] = p2->position;
    return result;
}
float sourceCube(float value) {
    // Selected Phobos pow(float, integral 3): accumulate in real, then
    // narrow at the function return. Its exponent-two path stays float.
    const SourceReal input = value;
    SourceReal result = 1;
    result *= input;
    const SourceReal squared = input * input;
    result *= squared;
    return static_cast<float>(result);
}
Vec2 evalEdge(Vec2 p0, Vec2 p1, Vec2 p2, float t) {
    const float t2 = t*t, t3 = sourceCube(t);
    const float h00 = 2*t3-3*t2+1, h10 = t3-2*t2+t;
    const float h01 = -2*t3+3*t2, h11 = t3-t2;
    (void)h10;
    const auto slope = divide(sub(p2,p0),2);
    return add(add(mul(slope,h11),mul(p1,h01)),mul(p0,h00));
}
}

void BezierSegment::split(float t, BezierSegment& left, BezierSegment& right) {
    // D out parameters are initialized before the body, including aliasing calls.
    left = BezierSegment{};
    right = BezierSegment{};
    const float s = 1-t;
    const auto mc = add(mul(p[1],s),mul(p[2],t));
    const auto a1 = add(mul(p[0],s),mul(p[1],t));
    const auto a2 = add(mul(a1,s),mul(mc,t));
    const auto b2 = add(mul(p[2],s),mul(p[3],t));
    const auto b1 = add(mul(b2,t),mul(mc,s));
    const auto middle = add(mul(a2,s),mul(b1,t));
    left.p = {p[0],a1,a2,middle};
    right.p = {middle,b1,b2,p[3]};
}
Vec2 BezierSegment::eval(float t) { BezierSegment left,right; split(t,left,right); return left.p[3]; }

void CatmullSpline::remapTarget(MeshRef reference, Mat4 transform, const Positions* vertices) {
    const auto keepAlive = weak_from_this().lock();
    if (target) {
        refMesh.resize(0);
        if (vertices) {
            const auto count = vertices->size();
            for (std::size_t i = 0; i < count; ++i) {
                const nicxlive::core::math::Vec2ViewConst vertex(*vertices, i);
                refMesh.push_back(transformed(transform, vertex.toVector()));
            }
        } else {
            const auto input = compat::ngRequireObject(reference)->vertices;
            for (auto* vertex : input)
                refMesh.push_back(transformed(transform, compat::ngRequireObject(vertex)->position));
        }
        mapReference();
    }
}
void CatmullSpline::remapTarget(NodeRef node, Mat4 transform, const Positions*) {
    const auto keepAlive = weak_from_this().lock();
    if (target) {
        refMesh.resize(0);
        const float x = compat::ngRequireObject(node)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"));
        const float y = compat::ngRequireObject(node)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"));
        refMesh.push_back(transformed(transform,{x,y}));
        mapReference();
        const auto getParameter = [](NodeRef n, std::shared_ptr<nicxlive::core::param::Parameter> parameter,
            NullableString name, nicxlive::core::param::Vec2u index) {
            auto binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(
                compat::ngRequireObject(parameter)->getBinding(n,name));
            if (!binding) return 0.0f;
            return binding->getValue(index);
        };
        const auto parameter = project::incArmedParameter();
        const auto index = parameter ? parameter->findClosestKeypoint() : nicxlive::core::param::Vec2u{0,0};
        origX = getParameter(node,parameter,NullableString::fromStaticLiteral("transform.t.x"),index);
        // Preserve the independent source's repeated X key for origY.
        origY = getParameter(node,parameter,NullableString::fromStaticLiteral("transform.t.x"),index);
        origRotZ = getParameter(node,parameter,NullableString::fromStaticLiteral("transform.r.z"),index);
    }
}
void CatmullSpline::mapReference() {
    const auto keepAlive = weak_from_this().lock();
    if (points.size()<2) { refOffsets.resize(0); initTangents.resize(0); return; }
    refOffsets.resize(0); initTangents.resize(0);
    const float epsilon = 0.0001f;
    const auto count = refMesh.size();
    for (std::size_t i = 0; i < count; ++i) {
        nicxlive::core::math::Vec2View vertex(refMesh, i);
        const float offset = findClosestPointOffset(vertex.toVector());
        const auto point = [&] {
            const auto receiver = target;
            return compat::ngRequireObject(receiver)->eval(offset);
        }();
        const auto next = [&] {
            const auto receiver = target;
            return compat::ngRequireObject(receiver)->eval(offset+epsilon);
        }();
        const auto tangent = normalized(sub(next,point));
        if (offset<=0 || offset>=points.size()-1) {
            refOffsets.push_back({0,0,detail::nan}); initTangents.push_back(tangent); continue;
        }
        vertex.set(sub(vertex.toVector(),point));
        const float alongX = vertex.component(0)*tangent.x;
        const float alongY = vertex.component(1)*tangent.y;
        const float along = alongX+alongY;
        const float acrossY = vertex.component(1)*tangent.x;
        const float acrossX = vertex.component(0)*tangent.y;
        refOffsets.push_back({along,acrossY-acrossX,offset});
        initTangents.push_back(tangent);
    }
}
Mat4 CatmullSpline::exportTarget(MeshRef& mesh, std::size_t index, Vec2 vertex, Vec2,
    Vec2, Mat4 inverse, Vec2 deformation) {
    const auto keepAlive = weak_from_this().lock();
    const auto receiver = compat::ngRequireObject(mesh);
    if (index >= receiver->vertices.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, receiver->vertices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/spline.d"), 183);
    auto* destination = compat::ngRequireObject(receiver->vertices[index]);
    const auto destinationLifetime = core::math::mesh::ngTryRetainMeshVertex(destination);
    destination->position = sub(transformed(inverse,vertex),deformation);
    return Mat4::identity();
}
Mat4 CatmullSpline::exportTarget(NodeRef& node, std::size_t, Vec2 vertex, Vec2 tangent, Vec2 initial) {
    const auto keepAlive = weak_from_this().lock();
    const float currentAngle = sourceAtan2(tangent.y,tangent.x);
    const float originalAngle = sourceAtan2(initial.y,initial.x);
    const float angle = currentAngle-originalAngle+origRotZ;
    const auto changeParameter = [](NodeRef n, std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        NullableString name, nicxlive::core::param::Vec2u index, float value) {
        if (value==0) return value;
        auto binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(
            compat::ngRequireObject(parameter)->getBinding(n,name));
        if (!binding) {
            binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(parameter->createBinding(n,name));
            parameter->addBinding(binding);
        }
        const float previous = compat::ngRequireObject(binding)->getValue(index);
        compat::ngRequireObject(binding)->setValue(index,value);
        return previous;
    };
    const auto parameter = project::incArmedParameter();
    const auto index = parameter ? parameter->findClosestKeypoint() : nicxlive::core::param::Vec2u{0,0};
    if (parameter) {
        changeParameter(node,parameter,NullableString::fromStaticLiteral("transform.t.x"),index,vertex.x);
        changeParameter(node,parameter,NullableString::fromStaticLiteral("transform.t.y"),index,vertex.y);
        changeParameter(node,parameter,NullableString::fromStaticLiteral("transform.r.z"),index,angle);
    } else {
        compat::ngRequireObject(node)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"));
        compat::ngRequireObject(node)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"));
        compat::ngRequireObject(node)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.r.z"));
        compat::ngRequireObject(node)->setValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"),vertex.x);
        compat::ngRequireObject(node)->setValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"),vertex.y);
        compat::ngRequireObject(node)->setValueSource(
            compat::RetainedString::fromStaticLiteral("transform.r.z"),angle);
    }
    return Mat4::identity();
}
void CatmullSpline::update() { const auto keepAlive=weak_from_this().lock(); interpolate(); }
void CatmullSpline::interpolate() {
    const auto keepAlive=weak_from_this().lock();
    interpolated.resize(0); drawLines.resize(0);
    if (points.empty()) return;
    Vec2 last{detail::nan,detail::nan};
    const std::size_t count=resolution*(points.size()-1)+1;
    for (std::size_t position=0;position<count;++position) {
        const auto point=eval(static_cast<float>(position)/static_cast<float>(resolution));
        interpolated.push_back(point);
        if(position>0) {
            const auto segment = allocateTemporary([&] {
                return std::vector<Vec3>{{last.x,last.y,0},{point.x,point.y,0}};
            });
            drawLines.append(segment);
        }
        last=point;
    }
    drawPoints.resize(0);
    const auto snapshot=points;
    for(auto point:snapshot) drawPoints.push_back({point.position.x,point.position.y,0});
}
Vec2 CatmullSpline::eval(float offset) {
    const auto keepAlive=weak_from_this().lock();
    if(offset<=0) return splinePointAt(points,0,297).position;
    if(offset>=points.size()-1) return splinePointAt(points,points.size()-1,298).position;
    const auto index=toUnsigned(offset);
    const float t=offset-static_cast<float>(index),s=1-t;
    const auto p1=splinePointPointer(points,index,304);
    const auto p2=splinePointPointer(points,index+1u,305);
    if(points.size()==2) return add(mul(p1->position,s),mul(p2->position,t));
    if(index==0) return evalEdge(p1->position,p2->position,splinePointAt(points,index+2u,325).position,t);
    if(index==points.size()-2) return evalEdge(p2->position,p1->position,splinePointAt(points,index-1u,327).position,s);
    const auto p0=splinePointPointer(points,index-1u,330);
    const auto p3=splinePointPointer(points,index+2u,331);
    return catmullToBezier(p0.get(),p1.get(),p2.get(),p3.get()).eval(t);
}
float CatmullSpline::findClosestPointOffset(Vec2 point) {
    const auto keepAlive=weak_from_this().lock(); Vec2 tangent{detail::nan,detail::nan};
    return findClosestPointOffset(point,tangent);
}
float CatmullSpline::findClosestPointOffset(Vec2 point,Vec2& tangent) {
    const auto keepAlive=weak_from_this().lock(); tangent={0,0};
    if(points.size()<2) return 0;
    std::uint32_t best=0;float bestDistance=std::numeric_limits<float>::infinity();
    const std::size_t count=resolution*(points.size()-1)+1;
    for(std::size_t position=0;position<count;++position) {
        const float value=distance(interpolated[position],point);
        if(value<bestDistance) {bestDistance=value;best=static_cast<std::uint32_t>(position);}
    }
    const float candidateLeft=static_cast<float>(best-1u)/static_cast<float>(resolution);
    float left=0<candidateLeft?candidateLeft:0;
    const float limit=static_cast<float>(points.size()-1);
    const float candidateRight=static_cast<float>(best+1u)/static_cast<float>(resolution);
    float right=candidateRight<limit?candidateRight:limit;
    auto leftPoint=eval(left),rightPoint=eval(right);
    float leftDistance=std::abs(distance(point,leftPoint)),rightDistance=std::abs(distance(point,rightPoint));
    const float epsilon=0.0001f;
    while(right-left>epsilon*2) {
        const float middle=(left+right)/2;
        const auto middleLeft=eval(middle-epsilon),middleRight=eval(middle+epsilon);
        const float middleLeftDistance=std::abs(distance(point,middleLeft));
        const float middleRightDistance=std::abs(distance(point,middleRight));
        if(middleLeftDistance<middleRightDistance) {
            right=middle;rightPoint=eval(right);rightDistance=std::abs(distance(point,rightPoint));
        } else {left=middle;leftPoint=eval(left);leftDistance=std::abs(distance(point,leftPoint));}
    }
    (void)leftDistance;(void)rightDistance;
    tangent=sub(rightPoint,leftPoint);tangent=divide(tangent,std::abs(distance(tangent,{0,0})));
    if(left==0) return 0;
    if(right==points.size()-1) return static_cast<float>(points.size()-1);
    const float t=nearestLine(leftPoint,rightPoint,point);
    return left*(1-t)+right*t;
}
void CatmullSpline::splitAt(float offset) {
    const auto keepAlive=weak_from_this().lock();const auto position=eval(offset);
    const auto index=1u+toUnsigned(offset);
    points.insertValue(index, {position, 1, 1});
    interpolate();if(auto receiver=target) receiver->splitAt(offset);
}
void CatmullSpline::prependPoint(Vec2 point) {
    const auto keepAlive=weak_from_this().lock();points.insertValue(0, {point, 1, 1});
    interpolate();if(auto receiver=target) receiver->prependPoint(point);
}
void CatmullSpline::appendPoint(Vec2 point) {
    const auto keepAlive=weak_from_this().lock();points.push_back({point,1,1});
    interpolate();if(auto receiver=target) receiver->appendPoint(point);
}
std::int32_t CatmullSpline::findPoint(Vec2 point,float zoom) {
    const auto keepAlive=weak_from_this().lock();
    std::uint32_t best=0;float bestDistance=std::numeric_limits<float>::infinity();
    const auto snapshot=points;
    for(std::size_t i=0;i<snapshot.size();++i) {
        const float value=distance(snapshot[i].position,point);
        if(value<bestDistance) {bestDistance=value;best=static_cast<std::uint32_t>(i);}
    }
    if(bestDistance>selectRadius/zoom) return -1;
    return std::bit_cast<std::int32_t>(best);
}
std::int32_t CatmullSpline::addPoint(Vec2 point,float zoom) {
    const auto keepAlive=weak_from_this().lock();
    const auto lastIndex=[this] {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(points.size())-1u);};
    if(points.size()<2) {appendPoint(point);return lastIndex();}
    const float offset=findClosestPointOffset(point);
    if(offset<=0) {prependPoint(point);return 0;}
    if(offset>=points.size()-1) {appendPoint(point);return lastIndex();}
    const auto onCurve=eval(offset);
    if(std::abs(distance(point,onCurve))<selectRadius/zoom) {
        splitAt(offset);return std::bit_cast<std::int32_t>(1u+static_cast<std::uint32_t>(compat::ngFloatToInt32(offset)));
    }
    return -1;
}
void CatmullSpline::removePoint(std::uint32_t index) {
    const auto keepAlive=weak_from_this().lock();
    // The selected Phobos stable remove reaches popFrontExactly first.
    // Preserve that producer's failure and source location rather than the
    // native container helper's implementation-file diagnostic.
#ifndef NDEBUG
    if(index>=points.size()) throw compat::ngStaticError<compat::AssertError>(
        "range is smaller than amount of items to pop", "std/range/primitives.d", 2137);
#else
    const auto sourceFile=compat::RetainedString::fromStaticLiteral("std/range/primitives.d");
    if(index>points.size()) throw compat::ngStaticError<compat::ArraySliceError>(
        index, points.size(), points.size(), sourceFile, 2140);
    if(index==points.size()) throw compat::ngStaticError<compat::ArraySliceError>(
        1, 0, 0, sourceFile, 2140);
#endif
    points = points.removed(index);
    if(auto receiver=target) receiver->removePoint(index);
    interpolate();
}
void CatmullSpline::draw(Mat4 transform,Vec4 color,std::uint32_t lockedPoint) {
    const auto keepAlive=weak_from_this().lock();using namespace core::dbg;
    if(!drawLines.empty()) {inDbgSetBuffer(drawLines);inDbgDrawLines(color,transform);}
    if(!drawPoints.empty()) {
        inDbgSetBuffer(drawPoints);inDbgPointsSize(10);
        inDbgDrawPoints({0,0,0,1},transform);inDbgPointsSize(6);inDbgDrawPoints(color,transform);
    }
    if(lockedPoint<drawPoints.size()) {
        const auto value = drawPoints[lockedPoint].toVector();
        auto point = allocateTemporary([&] { return compat::Slice<Vec3>{value}; });
        inDbgSetBuffer(point);
        inDbgPointsSize(6);inDbgDrawPoints({1,0,0,1},transform);
    }
}
void CatmullSpline::resetFloating() {
    const auto keepAlive=weak_from_this().lock();const auto snapshot=points;
    for(std::size_t i=0;i<snapshot.size();++i) {
        const auto point=snapshot[i];
        splinePointAt(points,i,485).position=point.position;
    }
    update();
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0.
// Retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString CatmullSpline::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSplineClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t CatmullSpline::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int CatmullSpline::opCmp(std::shared_ptr<CatmullSpline>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSplineClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = allocateTemporary([&] {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    });
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool CatmullSpline::opEquals(std::shared_ptr<CatmullSpline> other) {
    return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
}
bool ngCatmullSplineObjectEquals(std::shared_ptr<CatmullSpline> left, std::shared_ptr<CatmullSpline> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareSplineClassInfo);
}
} // namespace nijigenerate::viewport::common::spline
