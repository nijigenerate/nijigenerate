#include "nijigenerate/viewport/depth/tools/operation.hpp"
#include "nijigenerate/ext/nodes/exdepthops.hpp"
#include "nijigenerate/viewport/depth/mesheditor/node.hpp"
#include "operation_sort.hpp"
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::depth::tools::operation {

namespace {
using Vec2=nicxlive::core::math::Vec2;
using nicxlive::core::d_runtime::SourceAllocator;
void prepareOperationClassInfo() {
    static const bool registered = [] {
        const auto registerType = []<class T> {
            using namespace nicxlive::core::d_runtime;
            using Names = SourceObjectTypeNames<T>;
            const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
            ngRegisterObjectTypeNames<T>(name,
                compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
            ngRegisterObjectClassName(typeid(T), name);
        };
        registerType.template operator()<DepthOperation>();
        registerType.template operator()<DepthAttachedPointOperation>();
        registerType.template operator()<DepthRingOperation>();
        registerType.template operator()<DepthPlaneOperation>();
        return true;
    }();
    (void)registered;
}
float orderedClamp(float value,float low,float high) {
    return value<low?low:value>high?high:value;
}
float hitDistance(Vec2 a,Vec2 b) {return detail::segmentLength(a.x-b.x,a.y-b.y);}
}

// Inherited object.Object defaults (Digital Mars 2000-2011, BSL-1.0).
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthOperation::toString() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    prepareOperationClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthOperation::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthOperation::opEquals(std::shared_ptr<DepthOperation> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthOperation::opCmp(std::shared_ptr<DepthOperation>) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    prepareOperationClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

DepthAttachedPointOperation::DepthAttachedPointOperation(std::size_t index,float amount) {
    initializeSource(index, amount);
}
void DepthAttachedPointOperation::initializeSource(std::size_t index, float amount) {
    this->index=index;
    this->amount=amount;
}

std::shared_ptr<DepthOperation> DepthAttachedPointOperation::clone() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    return DepthAttachedPointOperation::createSource([&] { return std::tuple{index, amount}; });
}

DepthRingOperation::DepthRingOperation(nicxlive::core::math::Vec2 p0,
    nicxlive::core::math::Vec2 p1,DepthBrushSettings settings) {
    initializeSource(p0, p1, settings);
}
void DepthRingOperation::initializeSource(nicxlive::core::math::Vec2 p0,
    nicxlive::core::math::Vec2 p1, DepthBrushSettings settings) {
    this->p0=p0;
    this->p1=p1;
    this->amount=settings.amount;
    this->width=settings.radiusY;
    this->hardness=settings.hardness;
}

std::shared_ptr<DepthOperation> DepthRingOperation::clone() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    DepthBrushSettings settings;
    settings.amount=amount;
    settings.radiusY=width;
    settings.hardness=hardness;
    auto result=DepthRingOperation::createSource([&] { return std::tuple{p0, p1, settings}; });
    result->p0Angle=p0Angle;
    result->p1Angle=p1Angle;
    return result;
}

DepthPlaneOperation::DepthPlaneOperation(nicxlive::core::math::Vec2 center,
    float radiusX,float radiusY,DepthBrushSettings settings) {
    initializeSource(center, radiusX, radiusY, settings);
}
void DepthPlaneOperation::initializeSource(nicxlive::core::math::Vec2 center,
    float radiusX, float radiusY, DepthBrushSettings settings) {
    this->center=center;
    this->radiusX=radiusX;
    this->radiusY=radiusY;
    this->angle=settings.angle;
    this->targetDepth=settings.amount;
    this->flattenStrength=settings.flattenStrength;
}

std::shared_ptr<DepthOperation> DepthPlaneOperation::clone() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    DepthBrushSettings settings;
    settings.angle=angle;
    settings.amount=targetDepth;
    settings.flattenStrength=flattenStrength;
    return DepthPlaneOperation::createSource([&] { return std::tuple{center, radiusX, radiusY, settings}; });
}

ext::nodes::ExDepthOp toExDepthOp(std::shared_ptr<DepthOperation> operation) {
    ext::nodes::ExDepthOp result;
    if (auto attached=std::dynamic_pointer_cast<DepthAttachedPointOperation>(operation)) {
        result.type=ext::nodes::ExDepthOpType::AttachedPoint;
        result.index=attached->index;
        result.amount=attached->amount;
    } else if (auto ring=std::dynamic_pointer_cast<DepthRingOperation>(operation)) {
        result.type=ext::nodes::ExDepthOpType::Ring;
        result.p0=ring->p0;
        result.p1=ring->p1;
        result.amount=ring->amount;
        result.width=ring->width;
        result.hardness=ring->hardness;
        result.p0Angle=ring->p0Angle;
        result.p1Angle=ring->p1Angle;
    } else if (auto plane=std::dynamic_pointer_cast<DepthPlaneOperation>(operation)) {
        result.type=ext::nodes::ExDepthOpType::Plane;
        result.center=plane->center;
        result.radiusX=plane->radiusX;
        result.radiusY=plane->radiusY;
        result.angle=plane->angle;
        result.targetDepth=plane->targetDepth;
        result.flattenStrength=plane->flattenStrength;
    }
    return result;
}

std::shared_ptr<DepthOperation> depthOperationFromExDepthOp(ext::nodes::ExDepthOp op) {
    switch (op.type) {
    case ext::nodes::ExDepthOpType::AttachedPoint:
        return DepthAttachedPointOperation::createSource([&] { return std::tuple{op.index, op.amount}; });
    case ext::nodes::ExDepthOpType::Ring: {
        DepthBrushSettings settings;
        settings.amount=op.amount;
        settings.radiusY=op.width;
        settings.hardness=op.hardness;
        auto result=DepthRingOperation::createSource([&] { return std::tuple{op.p0, op.p1, settings}; });
        result->p0Angle=op.p0Angle;
        result->p1Angle=op.p1Angle;
        return result;
    }
    case ext::nodes::ExDepthOpType::Plane: {
        DepthBrushSettings settings;
        settings.amount=op.targetDepth;
        settings.angle=op.angle;
        settings.flattenStrength=op.flattenStrength;
        return DepthPlaneOperation::createSource([&] {
            return std::tuple{op.center, op.radiusX, op.radiusY, settings};
        });
    }
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("nijigenerate/viewport/depth/tools/operation.d"), 395);
}

RingOrientation ringOrientation(std::shared_ptr<DepthRingOperation> op) {
    const auto p1=compat::ngRequireObject(op)->p1;
    return std::abs(p1.x-op->p0.x)>=std::abs(op->p1.y-op->p0.y)
        ?RingOrientation::Horizontal:RingOrientation::Vertical;
}

float ringCrossCoord(std::shared_ptr<DepthRingOperation> op,std::uint8_t axis) {
    if (axis=='x') {
        const auto first=compat::ngRequireObject(op)->p0.y;
        return (first+op->p1.y)*0.5f;
    }
    const auto first=compat::ngRequireObject(op)->p0.x;
    return (first+op->p1.x)*0.5f;
}

void DepthAttachedPointOperation::apply(DepthEditorHandle editor) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const auto targetIndex=index;
    const auto value=amount;
    compat::ngRequireObject(editor)->addDepth(targetIndex,value);
}

void DepthRingOperation::apply(DepthEditorHandle editor) {
    auto self=std::dynamic_pointer_cast<DepthRingOperation>(retainedSelf());
    applyRingNormalSurfaces(editor,compat::Slice<std::shared_ptr<DepthRingOperation>>{self});
}

DepthOperationHandle DepthRingOperation::hit(DepthEditorHandle editor,Vec2 mouse,
    DepthCamera3D& camera,float radius,float& distance) {
    distance=std::numeric_limits<float>::quiet_NaN();
    auto self=std::dynamic_pointer_cast<DepthRingOperation>(retainedSelf());
    const auto h0=ringEndpointPoint(editor,self,true,camera);
    const auto h1=ringEndpointPoint(editor,self,false,camera);
    const auto hm=ringAmountHandlePoint(editor,self,camera);
    const auto ha0=ringAngleHandlePoint(editor,self,true,camera);
    const auto ha1=ringAngleHandlePoint(editor,self,false,camera);
    distance=hitDistance(mouse,hm);
    if (distance<=radius) return DepthOperationHandle::Amount;
    distance=hitDistance(mouse,ha0);
    if (distance<=radius) return DepthOperationHandle::P0Angle;
    distance=hitDistance(mouse,ha1);
    if (distance<=radius) return DepthOperationHandle::P1Angle;
    distance=hitDistance(mouse,h0);
    if (distance<=radius) return DepthOperationHandle::P0;
    distance=hitDistance(mouse,h1);
    if (distance<=radius) return DepthOperationHandle::P1;
    distance=ringCurveDistance(editor,self,mouse,camera);
    return distance<=radius?DepthOperationHandle::Body:DepthOperationHandle::None;
}

DepthOperationHandle DepthAttachedPointOperation::hit(DepthEditorHandle editor,Vec2 mouse,
    DepthCamera3D& camera,float radius,float& distance) {
    distance=std::numeric_limits<float>::quiet_NaN();
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const auto targetIndex=index;
    const auto local=compat::ngRequireObject(editor)->localVertex(targetIndex);
    const auto point=editor->projectLocalPoint(local,amount,camera);
    distance=hitDistance(mouse,point);
    if (distance<=radius) return DepthOperationHandle::Amount;
    const auto base=editor->projectLocalPoint(local,0,camera);
    distance=hitDistance(mouse,base);
    if (distance<=radius) return DepthOperationHandle::Body;
    return distance<=radius?DepthOperationHandle::Amount:DepthOperationHandle::None;
}

void DepthAttachedPointOperation::drag(DepthOperationHandle handle,DepthEditorHandle editor,
    std::shared_ptr<DepthOperation> startOperation,Vec2,Vec2 currentLocal,
    float startMouseY,float currentMouseY,bool) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    auto start=std::dynamic_pointer_cast<DepthAttachedPointOperation>(startOperation);
    if (!start) return;
    switch (handle) {
    case DepthOperationHandle::Body: {
        const auto nearest=compat::ngRequireObject(editor)->nearestLocalVertexIndex(currentLocal);
        if (nearest>=0) index=static_cast<std::size_t>(nearest);
        return;
    }
    case DepthOperationHandle::Amount: amount=start->amount-(currentMouseY-startMouseY)*0.006f;return;
    case DepthOperationHandle::None:case DepthOperationHandle::P0:case DepthOperationHandle::P1:
    case DepthOperationHandle::RadiusX:case DepthOperationHandle::RadiusY:
    case DepthOperationHandle::P0Angle:case DepthOperationHandle::P1Angle:return;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/tools/operation.d"), 129);
}

void DepthRingOperation::drag(DepthOperationHandle handle,DepthEditorHandle editor,
    std::shared_ptr<DepthOperation> startOperation,Vec2 startLocal,Vec2 currentLocal,
    float startMouseY,float currentMouseY,bool snapToGrid) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    auto start=std::dynamic_pointer_cast<DepthRingOperation>(startOperation);
    if (!start) return;
    switch (handle) {
    case DepthOperationHandle::P0: p0=snapToGrid?compat::ngRequireObject(editor)->snapLocalPoint(currentLocal):currentLocal;return;
    case DepthOperationHandle::P1: p1=snapToGrid?compat::ngRequireObject(editor)->snapLocalPoint(currentLocal):currentLocal;return;
    case DepthOperationHandle::Amount: amount=start->amount-(currentMouseY-startMouseY)*0.006f;return;
    case DepthOperationHandle::P0Angle:p0Angle=std::round(start->p0Angle-(currentMouseY-startMouseY)*0.7f);return;
    case DepthOperationHandle::P1Angle:p1Angle=std::round(start->p1Angle-(currentMouseY-startMouseY)*0.7f);return;
    case DepthOperationHandle::Body: {
        const Vec2 delta{currentLocal.x-startLocal.x,currentLocal.y-startLocal.y};
        p0={start->p0.x+delta.x,start->p0.y+delta.y};
        p1={start->p1.x+delta.x,start->p1.y+delta.y};
        if (snapToGrid) {
            const auto first=p0;
            p0=compat::ngRequireObject(editor)->snapLocalPoint(first);
            p1=editor->snapLocalPoint(p1);
        }
        return;
    }
    case DepthOperationHandle::None:case DepthOperationHandle::RadiusX:case DepthOperationHandle::RadiusY:return;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/tools/operation.d"), 230);
}

void DepthPlaneOperation::apply(DepthEditorHandle editor) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    applyPlaneFlatten(editor,center,radiusX,radiusY,angle,targetDepth,flattenStrength);
}

DepthOperationHandle DepthPlaneOperation::hit(DepthEditorHandle editor,Vec2 mouse,
    DepthCamera3D& camera,float radius,float& distance) {
    distance=std::numeric_limits<float>::quiet_NaN();
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const float angleRad=angle*3.14159265358979323846f/180.0f;
    const Vec2 ux{std::cos(angleRad),std::sin(angleRad)};
    const Vec2 uy{-std::sin(angleRad),std::cos(angleRad)};
    const auto point=center;
    const auto depth=targetDepth;
    const auto amountHandle=compat::ngRequireObject(editor)->projectLocalPoint(point,depth,camera);
    distance=hitDistance(mouse,amountHandle);
    if (distance<=radius) return DepthOperationHandle::Amount;
    const auto rxHandle=editor->projectLocalPoint({center.x+ux.x*radiusX,center.y+ux.y*radiusX},targetDepth,camera);
    distance=hitDistance(mouse,rxHandle);
    if (distance<=radius) return DepthOperationHandle::RadiusX;
    const auto ryHandle=editor->projectLocalPoint({center.x+uy.x*radiusY,center.y+uy.y*radiusY},targetDepth,camera);
    distance=hitDistance(mouse,ryHandle);
    if (distance<=radius) return DepthOperationHandle::RadiusY;
    const auto centerHandle=editor->projectLocalPoint(center,0,camera);
    distance=hitDistance(mouse,centerHandle);
    if (distance<=radius) return DepthOperationHandle::Body;
    return DepthOperationHandle::None;
}

void DepthPlaneOperation::drag(DepthOperationHandle handle,DepthEditorHandle,
    std::shared_ptr<DepthOperation> startOperation,Vec2 startLocal,Vec2 currentLocal,
    float startMouseY,float currentMouseY,bool) {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    auto start=std::dynamic_pointer_cast<DepthPlaneOperation>(startOperation);
    if (!start) return;
    switch (handle) {
    case DepthOperationHandle::Body:
        center={start->center.x+(currentLocal.x-startLocal.x),start->center.y+(currentLocal.y-startLocal.y)};return;
    case DepthOperationHandle::Amount:targetDepth=start->targetDepth-(currentMouseY-startMouseY)*0.006f;return;
    case DepthOperationHandle::RadiusX:radiusX=std::max(1.0f,std::abs(currentLocal.x-center.x));return;
    case DepthOperationHandle::RadiusY:radiusY=std::max(1.0f,std::abs(currentLocal.y-center.y));return;
    case DepthOperationHandle::None:case DepthOperationHandle::P0:case DepthOperationHandle::P1:
    case DepthOperationHandle::P0Angle:case DepthOperationHandle::P1Angle:return;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/tools/operation.d"), 336);
}

void applyRingDepth(DepthEditorHandle editor,Vec2 p0,Vec2 p1,float amount,float width,float hardness) {
    const auto vertices=compat::ngRequireObject(editor)->getVertices();
    const float vx=p1.x-p0.x,vy=p1.y-p0.y;
    const float len=std::max(1.0f,std::sqrt(vx*vx+vy*vy));
    const float ux=vx/len,uy=vy/len,half=len*0.5f;
    const float cx=(p0.x+p1.x)*0.5f,cy=(p0.y+p1.y)*0.5f;
    width=std::max(0.5f,width);hardness=std::max(0.1f,hardness);
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto point=vertices[i];
        const float px=point.x-cx,py=point.y-cy;
        const float along=px*ux+py*uy,side=std::abs(-px*uy+py*ux);
        if (std::abs(along)>half||side>width*2.25f) continue;
        const float t=along/std::max(1.0f,half);
        const float ellipse=std::sqrt(std::max(0.0f,1.0f-t*t));
        const float sideFalloff=std::exp(-std::pow(side/width,hardness)*2.2f);
        editor->addDepth(i,amount*ellipse*sideFalloff);
    }
}

bool ringSampleAt(std::shared_ptr<DepthRingOperation> op,Vec2 point,std::uint8_t axis,RingSample& sample) {
    sample=RingSample{};
    const auto a=compat::ngRequireObject(op)->p0;
    const auto b=op->p1;
    const float along=axis=='x'?point.x:point.y,aAlong=axis=='x'?a.x:a.y,bAlong=axis=='x'?b.x:b.y;
    const float minAlong=std::min(aAlong,bAlong),maxAlong=std::max(aAlong,bAlong);
    if (along<minAlong||along>maxAlong) return false;
    const float span=bAlong-aAlong;
    if (std::abs(span)<0.000001f) return false;
    const float ratio=orderedClamp((along-aAlong)/span,0.0f,1.0f);
    const float angle0=op->p0Angle*3.14159265358979323846f/180.0f;
    const float angle1=op->p1Angle*3.14159265358979323846f/180.0f;
    const float angle=angle0+(angle1-angle0)*ratio;
    const float tangentSlope=op->amount*std::cos(angle)*(angle1-angle0)/span;
    sample={ringCrossCoord(op,axis),op->amount*std::sin(angle),-tangentSlope,
        std::max(1.0f,op->width),std::max(0.1f,op->hardness),1};
    return true;
}

void applyPlaneFlatten(DepthEditorHandle editor,Vec2 center,float radiusX,float radiusY,
    float angle,float targetDepth,float flattenStrength) {
    const auto vertices=compat::ngRequireObject(editor)->getVertices();
    const auto depths=editor->copyEditorDepths();
    float ringSum=std::numeric_limits<float>::quiet_NaN();std::size_t ringCount=0;
    float insideSum=std::numeric_limits<float>::quiet_NaN();std::size_t insideCount=0;
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto n=ellipseNorm(vertices[i],center,radiusX,radiusY,angle);
        if (n<=1.0f) {insideSum+=detail::sourceElement(depths,i,726);++insideCount;}
        else if (n<=1.35f) {ringSum+=detail::sourceElement(depths,i,729);++ringCount;}
    }
    if (insideCount==0) return;
    auto target=ringCount>0?ringSum/static_cast<float>(ringCount):insideSum/static_cast<float>(insideCount);
    if (targetDepth!=0) target=targetDepth;
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto n=ellipseNorm(vertices[i],center,radiusX,radiusY,angle);
        if (n>1.0f) continue;
        const float centerWeight=1.0f-orderedClamp(n,0.0f,1.0f);
        const float strength=orderedClamp(flattenStrength,0.0f,1.0f)*(0.45f+centerWeight*0.55f);
        editor->setDepth(i,detail::sourceElement(depths,i,742)*(1.0f-strength)+target*strength);
    }
}

void applyRingFamilySurface(DepthEditorHandle editor,compat::Slice<std::shared_ptr<DepthRingOperation>> rings,
    std::uint8_t axis) {
    if (rings.empty()) return;
    auto ringBefore = [axis](auto a,auto b) {
        const auto left=ringCrossCoord(a,axis);
        const auto right=ringCrossCoord(b,axis);
        return left<right;
    };
    sort_detail::Sorter<std::shared_ptr<DepthRingOperation>, decltype(ringBefore), sizeof(void*), false>(
        ringBefore).sort({rings.data(), rings.size()});
    const auto vertices=compat::ngRequireObject(editor)->getVertices();
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto point=vertices[i];
        compat::Slice<RingSample> samples;
        for (auto op:rings) {
            RingSample sample;
            if (ringSampleAt(op,point,axis,sample)) samples.append(sample);
        }
        if (samples.empty()) continue;
        auto sampleBefore = [](RingSample a, RingSample b) noexcept { return a.cross < b.cross; };
        constexpr auto sampleSourceBytes = sizeof(std::size_t) == 8 ? 32 : 24;
        sort_detail::Sorter<RingSample, decltype(sampleBefore), sampleSourceBytes, true>(
            sampleBefore).sort({samples.data(), samples.size()});
        const auto candidates=combineRingSamplesByCross(samples);
        if (candidates.empty()) continue;
        const float cross=axis=='x'?point.y:point.x;
        std::ptrdiff_t beforeIndex=-1,afterIndex=-1;
        for (std::size_t j=0;j<candidates.size();++j) {
            const auto sample=candidates[j];
            if (sample.cross<=cross) beforeIndex=static_cast<std::ptrdiff_t>(j);
            if (afterIndex<0&&sample.cross>=cross) afterIndex=static_cast<std::ptrdiff_t>(j);
        }
        float value;
        if (beforeIndex>=0&&afterIndex>=0&&beforeIndex!=afterIndex) {
            const auto before=detail::sourceElement(candidates,static_cast<std::size_t>(beforeIndex),562);
            const auto after=detail::sourceElement(candidates,static_cast<std::size_t>(afterIndex),563);
            const float span=std::max(0.000001f,after.cross-before.cross);
            const float t=orderedClamp((cross-before.cross)/span,0.0f,1.0f);
            value=hermiteDepth(before.z,before.normalSlope,after.z,after.normalSlope,span,t);
        } else {
            const auto nearest=detail::sourceElement(candidates,
                static_cast<std::size_t>(beforeIndex>=0?beforeIndex:afterIndex),568);
            const auto distance=std::abs(cross-nearest.cross);
            const auto falloff=std::exp(-std::pow(distance/std::max(1.0f,nearest.width),nearest.hardness)*2.2f);
            value=nearest.z*falloff;
        }
        if (std::abs(value)>0.000001f) editor->addDepth(i,value);
    }
}

void applyRingNormalSurfaces(DepthEditorHandle editor,compat::Slice<std::shared_ptr<DepthRingOperation>> rings) {
    compat::Slice<std::shared_ptr<DepthRingOperation>> horizontal,vertical;
    for (auto op:rings) {
        switch (ringOrientation(op)) {
        case RingOrientation::Horizontal:horizontal.append(op);break;
        case RingOrientation::Vertical:vertical.append(op);break;
        }
    }
    applyRingFamilySurface(editor,horizontal,'x');
    applyRingFamilySurface(editor,vertical,'y');
}

float ringBaseDepthAt(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,float ratio) {
    const auto p0=compat::ngRequireObject(op)->p0;
    const auto d0=compat::ngRequireObject(editor)->depthAtLocalPoint(p0);
    const auto p1=op->p1;
    const auto d1=editor->depthAtLocalPoint(p1);
    return d0+(d1-d0)*ratio;
}

Vec2 ringEndpointPoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    bool first,DepthCamera3D& camera) {
    const auto point=first?compat::ngRequireObject(op)->p0:compat::ngRequireObject(op)->p1;
    const auto depth=compat::ngRequireObject(editor)->depthAtLocalPoint(point);
    return editor->projectLocalPoint(point,depth,camera);
}

Vec2 ringCurvePoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    float ratio,bool backSide,DepthCamera3D& camera) {
    constexpr float pi=3.14159265358979323846f;
    const float angle0=compat::ngRequireObject(op)->p0Angle*pi/180.0f+(backSide?pi:0.0f);
    const float angle1=op->p1Angle*pi/180.0f+(backSide?pi:0.0f);
    const float angle=angle0+(angle1-angle0)*ratio;
    const Vec2 point{op->p0.x+(op->p1.x-op->p0.x)*ratio,op->p0.y+(op->p1.y-op->p0.y)*ratio};
    const auto baseDepth=ringBaseDepthAt(editor,op,ratio);
    const auto depth=baseDepth+std::sin(angle)*op->amount;
    return editor->projectLocalPoint(point,depth,camera);
}

Vec2 ringAmountHandlePoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,DepthCamera3D& camera) {
    const auto angle0=compat::ngRequireObject(op)->p0Angle;
    const float angle=((angle0+op->p1Angle)*0.5f)*3.14159265358979323846f/180.0f;
    const Vec2 point{(op->p0.x+op->p1.x)*0.5f,(op->p0.y+op->p1.y)*0.5f};
    const auto baseDepth=ringBaseDepthAt(editor,op,0.5f);
    const auto depth=baseDepth+std::sin(angle)*op->amount;
    return editor->projectLocalPoint(point,depth,camera);
}

Vec2 ringAngleHandlePoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    bool first,DepthCamera3D& camera) {
    const auto p1=compat::ngRequireObject(op)->p1;
    const auto p0=op->p0;
    Vec2 tangent{p1.x-p0.x,p1.y-p0.y};
    double squared=0.0;
    squared+=static_cast<double>(tangent.x*tangent.x);
    squared+=static_cast<double>(tangent.y*tangent.y);
    const auto len=std::max(1.0,std::sqrt(squared));
    // Actual inmath Vec2 opBinary("/") takes float even when the caller's len is real.
    const auto divisor=static_cast<float>(len);
    tangent={tangent.x/divisor,tangent.y/divisor};
    const float angle=(first?op->p0Angle:op->p1Angle)*3.14159265358979323846f/180.0f;
    const auto base=first?op->p0:op->p1;
    const float localRadius=compat::ngRequireObject(editor)->depthDisplayScale()*(0.17f/mesheditor::node::DepthDisplayZScale);
    const float depthRadius=0.22f/mesheditor::node::DepthDisplayZScale;
    const auto localOffset=std::cos(angle)*localRadius;
    const Vec2 point{base.x+tangent.x*localOffset,base.y+tangent.y*localOffset};
    const auto baseDepth=editor->depthAtLocalPoint(base);
    const auto depth=baseDepth+std::sin(angle)*depthRadius;
    return editor->projectLocalPoint(point,depth,camera);
}

float ringCurveDistance(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    Vec2 mouse,DepthCamera3D& camera) {
    constexpr std::int32_t segments=48;
    float best=std::numeric_limits<float>::max();
    for (std::int32_t backSide=0;backSide<2;++backSide) {
        auto prev=ringCurvePoint(editor,op,0,backSide==1,camera);
        for (std::int32_t i=1;i<=segments;++i) {
            const float ratio=static_cast<float>(i)/segments;
            const auto next=ringCurvePoint(editor,op,ratio,backSide==1,camera);
            best=std::min(best,distanceToSegment(mouse,prev,next));
            prev=next;
        }
    }
    return best;
}

} // namespace nijigenerate::viewport::depth::tools::operation
