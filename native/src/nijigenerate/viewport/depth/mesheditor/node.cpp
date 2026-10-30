#include "nijigenerate/viewport/depth/mesheditor/node.hpp"
#include "nijigenerate/viewport/depth/common/vertex_copy.hpp"
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include "nijigenerate/viewport/depth/detail/editor_log_format.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <common/resource_key_map.hpp>
#include "nijigenerate/ext/nodes/exdepthmapped.hpp"
#include <nodes/grid_deformer.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <bit>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::viewport::depth::mesheditor::node {
namespace {
long double pointDistance(nicxlive::core::math::Vec2 a,nicxlive::core::math::Vec2 b) {
    const float x=a.x-b.x,y=a.y-b.y;
    // inmath float components are squared before accumulation into real.
    long double squared=0.0L;
    squared+=static_cast<long double>(x*x);
    squared+=static_cast<long double>(y*y);
    return std::sqrt(squared);
}
}

void DepthMeshEditorOne::prepareSourceClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<DepthMeshEditorOne>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<DepthMeshEditorOne>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(DepthMeshEditorOne), name);
        return true;
    }();
    (void)registered;
}

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthMeshEditorOne::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthMeshEditorOne::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthMeshEditorOne::opEquals(std::shared_ptr<DepthMeshEditorOne> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthMeshEditorOne::opCmp(std::shared_ptr<DepthMeshEditorOne>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string bytes = "need opCmp for class ";
        bytes += className.view();
        message = compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void DepthMeshEditorOne::log(std::string_view) {}

compat::Slice<float> DepthMeshEditorOne::sortedUnique(compat::Slice<float> values) {
    compat::phobos_float_sort::sort({values.data(),values.size()});
    // uniq.array owns a separate result, after sorting the actual input backing.
    compat::Slice<float> result;
    for (const float value:values)
        if (result.empty() || result[result.size()-1]!=value) result.append(value);
    return result;
}

void DepthMeshEditorOne::rebuildTopology() {
    indices.resize(0);
    const auto vertices=common::targetview::detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (vertices.empty()) {
        if (!loggedEmptyVertices) {log("topology skipped: GridDeformer has no vertices");loggedEmptyVertices=true;}
        return;
    }
    compat::Slice<float> xs,ys;
    for (const auto v:vertices) {xs.append(v.x);ys.append(v.y);}
    xs=sortedUnique(xs);ys=sortedUnique(ys);
    if (xs.size()<2||ys.size()<2||xs.size()*ys.size()!=vertices.size()) {
        if (!loggedInvalidTopology) {
            log(detail::topologyInvalid(vertices.size(),xs.size(),ys.size(),xs.size()*ys.size()));
            loggedInvalidTopology=true;
        }
        return;
    }
    struct UlongHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            if constexpr (sizeof(std::size_t) == 4)
                return static_cast<std::size_t>(value ^ (value >> 32));
            else return static_cast<std::size_t>(value);
        }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, std::uint16_t,
        UlongHash, std::equal_to<std::uint64_t>> lookup;
    for (std::size_t i=0;i<vertices.size();++i) {
        const auto v=vertices[i];std::size_t xi=0,yi=0;
        for (std::size_t j=0;j<xs.size();++j) if (xs[j]==v.x) {xi=j;break;}
        for (std::size_t j=0;j<ys.size();++j) if (ys[j]==v.y) {yi=j;break;}
        try {
            lookup[yi*xs.size()+xi]=static_cast<std::uint16_t>(i);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    for (std::size_t y=0;y<ys.size()-1;++y) for (std::size_t x=0;x<xs.size()-1;++x) {
        const auto k0=y*xs.size()+x,k1=y*xs.size()+x+1;
        const auto k2=(y+1)*xs.size()+x,k3=(y+1)*xs.size()+x+1;
        const auto p0=lookup.find(k0),p1=lookup.find(k1),p2=lookup.find(k2),p3=lookup.find(k3);
        if (p0==lookup.end()||p1==lookup.end()||p2==lookup.end()||p3==lookup.end()) {
            if (!loggedInvalidTopology) {
                log(detail::topologyMissing(x,y));loggedInvalidTopology=true;
            }
            continue;
        }
        const auto i0=p0->second,i1=p1->second,i2=p2->second,i3=p3->second;
        indices.append(compat::Slice<std::uint16_t>{i0,i1,i3,i0,i3,i2});
    }
    log(detail::topologyReady(vertices.size(),xs.size(),ys.size(),indices.size()/3));
}

void DepthMeshEditorOne::rebuildBounds() {
    const auto vertices=common::targetview::detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (vertices.empty()) { minPoint={0,0};maxPoint={1,1};return; }
    minPoint=vertices[0];maxPoint=vertices[0];
    for (std::size_t i=1;i<vertices.size();++i) {
        const auto v=vertices[i];
        minPoint.x=std::min(minPoint.x,v.x);minPoint.y=std::min(minPoint.y,v.y);
        maxPoint.x=std::max(maxPoint.x,v.x);maxPoint.y=std::max(maxPoint.y,v.y);
    }
}

DepthMeshEditorOne::Grid DepthMeshEditorOne::getTarget() { return target; }
DepthMeshEditorOne::Node DepthMeshEditorOne::targetNode() {
    return std::dynamic_pointer_cast<nicxlive::core::nodes::Node>(target);
}

void DepthMeshEditorOne::bindTargetView(View view) { targetView=view;syncFromTargetView(); }
void DepthMeshEditorOne::syncFromTargetView() {
    if (!targetView) return;
    depths=targetView->depths.duplicate();
    baseDepths=targetView->baseDepths.duplicate();
}
void DepthMeshEditorOne::replaceBaseDepths(compat::Slice<float> values) {
    baseDepths=values.duplicate();
    if (targetView) targetView->baseDepths=baseDepths.duplicate();
}
compat::Slice<DepthMeshEditorOne::Vec2> DepthMeshEditorOne::getVertices() {
    if (targetView) return compat::ngRequireObject(targetView)->getVertices();
    return common::targetview::detail::copyVertices(compat::ngRequireObject(target)->getVertices());
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::localVertex(std::size_t index) {
    if (targetView) return compat::ngRequireObject(targetView)->localVertex(index);
    const auto vertices=getVertices();
    return index<vertices.size()?vertices[index]:Vec2{0,0};
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::snapLocalPoint(Vec2 point) {
    if (targetView) return compat::ngRequireObject(targetView)->snapLocalPoint(point);
    const auto vertices=getVertices();
    if (vertices.empty()) return point;
    auto best=vertices[0];auto bestDistance=pointDistance(point,best);
    for (std::size_t i=1;i<vertices.size();++i) {
        const auto distance=pointDistance(point,vertices[i]);
        if (distance<bestDistance) {best=vertices[i];bestDistance=distance;}
    }
    return best;
}
std::ptrdiff_t DepthMeshEditorOne::nearestLocalVertexIndex(Vec2 point) {
    if (targetView) return compat::ngRequireObject(targetView)->nearestLocalVertexIndex(point);
    const auto vertices=getVertices();
    if (vertices.empty()) return -1;
    std::ptrdiff_t best=0;auto bestDistance=pointDistance(point,vertices[0]);
    for (std::size_t i=1;i<vertices.size();++i) {
        const auto distance=pointDistance(point,vertices[i]);
        if (distance<bestDistance) {best=static_cast<std::ptrdiff_t>(i);bestDistance=distance;}
    }
    return best;
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::projectedVertex(std::size_t index) {
    if (targetView) return compat::ngRequireObject(targetView)->projectedVertex(index);
    return index<projectedPoints.size()?projectedPoints[index]:Vec2{0,0};
}
float DepthMeshEditorOne::depthDisplayScale() {
    if (targetView) return compat::ngRequireObject(targetView)->depthDisplayScale();
    return common::targetview::ngDepthDisplayScaleForBounds(minPoint,maxPoint);
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::depthViewToModel(Vec2 point,DepthCamera3D& camera,float depth) {
    if (targetView) return compat::ngRequireObject(targetView)->depthViewToModel(point,camera,depth);
    return unprojectDepthPoint(point,-depth*depthDisplayScale(),camera);
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::modelToDepthView(Vec2 point,float depth,DepthCamera3D& camera) {
    if (targetView) return compat::ngRequireObject(targetView)->modelToDepthView(point,depth,camera);
    return projectDepthPoint(point,-depth*depthDisplayScale(),camera);
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::localToWorld(Vec2 point) {
    return targetView?compat::ngRequireObject(targetView)->localToWorld(point):point;
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::projectLocalPoint(Vec2 point,float depth,DepthCamera3D& camera) {
    if (targetView) return compat::ngRequireObject(targetView)->projectLocalPoint(point,depth,camera);
    return modelToDepthView(point,depth,camera);
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::displayWorldToLocal(Vec2 point,DepthCamera3D& camera,float depth) {
    if (targetView) return compat::ngRequireObject(targetView)->displayWorldToLocal(point,camera,depth);
    return depthViewToModel(point,camera,depth);
}
DepthMeshEditorOne::Vec2 DepthMeshEditorOne::worldToLocal(Vec2 point) {
    return targetView?compat::ngRequireObject(targetView)->worldToLocal(point):point;
}
compat::Slice<float> DepthMeshEditorOne::copyEditorDepths() {
    if (targetView) {syncFromTargetView();return compat::ngRequireObject(targetView)->copyWorkingDepths();}
    return depths.duplicate();
}
void DepthMeshEditorOne::replaceEditorDepths(compat::Slice<float> values) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->replaceWorkingDepths(values);
        }
        syncFromTargetView();
        return;
    }
    depths=values.duplicate();
    if (depths.size()!=compat::ngRequireObject(target)->getVertices().size())
        depths.resize(compat::ngRequireObject(target)->getVertices().size(),std::numeric_limits<float>::quiet_NaN());
}
void DepthMeshEditorOne::resetWorkingDepths() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->resetWorkingDepths();
        }
        syncFromTargetView();
        return;
    }
    replaceEditorDepths(baseDepths);
}
void DepthMeshEditorOne::clearBaseDepths() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->clearBaseDepths();
        }
        syncFromTargetView();
        return;
    }
    baseDepths.resize(compat::ngRequireObject(target)->getVertices().size(),std::numeric_limits<float>::quiet_NaN());
    for (auto& value:baseDepths) value=0.0f;
    replaceEditorDepths(baseDepths);
}
void DepthMeshEditorOne::resetFromTarget() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->resetFromTarget();
        }
        syncFromTargetView();
        return;
    }
    depths=depthMapped?compat::ngRequireObject(depthMapped)->copyDepths():compat::Slice<float>{};
    if (depths.isNull()||depths.size()!=compat::ngRequireObject(target)->getVertices().size()) {
        const auto sourceLength=depths.isNull()?std::int64_t{-1}:
            std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(depths.size()));
        const auto targetLength=compat::ngRequireObject(target)->getVertices().size();
        log(detail::depthsInitialized(sourceLength,targetLength));
        depths.resize(compat::ngRequireObject(target)->getVertices().size(),std::numeric_limits<float>::quiet_NaN());
        for (auto& value:depths) value=0.0f;
    } else {log(detail::depthsLoaded(depths.size()));}
    baseDepths=depths.duplicate();
}
float DepthMeshEditorOne::getDepth(std::size_t index) {
    if (targetView) return compat::ngRequireObject(targetView)->getDepth(index);
    return index<depths.size()?depths[index]:0.0f;
}
float DepthMeshEditorOne::depthAtLocalPoint(Vec2 point) {
    if (targetView) return compat::ngRequireObject(targetView)->depthAtLocalPoint(point);
    const auto vertices=getVertices();
    if (vertices.empty()||depths.empty()) return 0.0f;
    std::size_t best=0;auto bestDistance=pointDistance(point,vertices[0]);
    for (std::size_t i=1;i<vertices.size();++i) {
        const auto distance=pointDistance(point,vertices[i]);
        if (distance<bestDistance) {best=i;bestDistance=distance;}
    }
    return best<depths.size()?depths[best]:0.0f;
}
void DepthMeshEditorOne::setDepth(std::size_t index,float value) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->setDepth(index,value);
        }
        syncFromTargetView();
        return;
    }
    if (index>=depths.size()) return;
    depths[index]=common::targetview::ngDepthTargetClampDepth(value);
}
void DepthMeshEditorOne::addDepth(std::size_t index,float value) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (targetView) {
        {
            const auto receiver = compat::ngRequireObject(targetView);
            receiver->addDepth(index,value);
        }
        syncFromTargetView();
        return;
    }
    setDepth(index,getDepth(index)+value);
}
std::ptrdiff_t DepthMeshEditorOne::nearestProjectedVertex(Vec2 point,float radius) {
    if (targetView) return compat::ngRequireObject(targetView)->nearestProjectedVertex(point,radius);
    std::ptrdiff_t best=-1;float bestDistance=radius;
    for (std::size_t i=0;i<projectedPoints.size();++i) {
        const auto distance=pointDistance(projectedPoints[i],point);
        if (distance<bestDistance) {best=static_cast<std::ptrdiff_t>(i);bestDistance=static_cast<float>(distance);}
    }
    return best;
}
bool DepthMeshEditorOne::isVertexSelected(std::ptrdiff_t index) {
    return std::find(selectedVertices.begin(),selectedVertices.end(),index)!=selectedVertices.end();
}
compat::Slice<std::ptrdiff_t> DepthMeshEditorOne::selectedVertexIndices() {return selectedVertices.duplicate();}
void DepthMeshEditorOne::clearVertexSelection() {selectedVertices.resize(0);selectedVertex=-1;}
void DepthMeshEditorOne::selectOneVertex(std::ptrdiff_t index) {
    if (index<0) {clearVertexSelection();return;}
    selectedVertices=compat::Slice<std::ptrdiff_t>{index};selectedVertex=index;
}
void DepthMeshEditorOne::selectVertex(std::ptrdiff_t index) {selectOneVertex(index);}
void DepthMeshEditorOne::addVertexSelection(std::ptrdiff_t index) {
    if (index<0||std::find(selectedVertices.begin(),selectedVertices.end(),index)!=selectedVertices.end()) return;
    selectedVertices.append(index);selectedVertex=index;
}
void DepthMeshEditorOne::deselectVertex(std::ptrdiff_t index) {
    std::size_t existing=0;
    while (existing<selectedVertices.size()&&selectedVertices[existing]!=index) ++existing;
    if (existing<selectedVertices.size()) {
        for (std::size_t i=existing;i+1<selectedVertices.size();++i) selectedVertices[i]=selectedVertices[i+1];
        selectedVertices.resize(selectedVertices.size()-1);
    }
    selectedVertex=selectedVertices.empty()?-1:selectedVertices[selectedVertices.size()-1];
}
void DepthMeshEditorOne::toggleVertexSelection(std::ptrdiff_t index) {
    if (isVertexSelected(index)) deselectVertex(index);else addVertexSelection(index);
}
compat::Slice<std::ptrdiff_t> DepthMeshEditorOne::projectedVerticesInRect(Vec2 p0,Vec2 p1) {
    compat::Slice<std::ptrdiff_t> result;
    const Vec2 low{std::min(p0.x,p1.x),std::min(p0.y,p1.y)};
    const Vec2 high{std::max(p0.x,p1.x),std::max(p0.y,p1.y)};
    for (std::size_t i=0;i<projectedPoints.size();++i) {
        const auto projected=projectedPoints[i];
        if (projected.x<low.x||projected.x>high.x) continue;
        if (projected.y<low.y||projected.y>high.y) continue;
        result.append(static_cast<std::ptrdiff_t>(i));
    }
    return result;
}

} // namespace nijigenerate::viewport::depth::mesheditor::node
