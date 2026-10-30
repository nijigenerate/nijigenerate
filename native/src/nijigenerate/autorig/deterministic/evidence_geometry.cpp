#include "detail/evidence.hpp"
#include <compat/object_metadata.hpp>
namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;
AlphaSupportIndex::AlphaSupportIndex(compat::Slice<Point2> points):root_(build(points.duplicate(),0)) {
    prepareSourceClassInfo();
}
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void AlphaSupportIndex::prepareSourceClassInfo() {
    static const bool registered = [] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AlphaSupportIndex),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.autorig.deterministic.evidence.AlphaSupportIndex"));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
compat::RetainedString AlphaSupportIndex::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AlphaSupportIndex::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool AlphaSupportIndex::opEquals(std::shared_ptr<AlphaSupportIndex> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int AlphaSupportIndex::opCmp(std::shared_ptr<AlphaSupportIndex>) {
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string bytes("need opCmp for class ");
        bytes += className.view();
        message = copiedEvidenceMessage(bytes);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
std::size_t AlphaSupportIndex::build(compat::Slice<Point2> points,std::size_t axis) {
    if(points.empty())return SIZE_MAX;
    sort(points,[axis](Point2 a,Point2 b){return a[axis]<b[axis];});
    const auto middle=points.size()/2,id=entries_.size();auto p=points[middle];
    entries_.append(Entry{p,{p[0],p[1],p[0],p[1]}});
    const auto left=build(points.slice(0,middle),1-axis),right=build(points.slice(middle+1,points.size()),1-axis);
    entries_[id].left=left;entries_[id].right=right;
    for(auto child:{left,right})if(child!=SIZE_MAX) {
        const auto bounds=entries_[child].bounds;
        entries_[id].bounds[0]=dmin(entries_[id].bounds[0],bounds[0]);
        entries_[id].bounds[1]=dmin(entries_[id].bounds[1],bounds[1]);
        entries_[id].bounds[2]=dmax(entries_[id].bounds[2],bounds[2]);
        entries_[id].bounds[3]=dmax(entries_[id].bounds[3],bounds[3]);
    }
    return id;
}
double AlphaSupportIndex::distance(Point2 query) {
    double best=std::numeric_limits<double>::infinity();
    auto lowerBound=[&](std::size_t id) { if(id==SIZE_MAX)return std::numeric_limits<double>::infinity();auto b=entries_[id].bounds;
        double dx=dmax(0.,dmax(b[0]-query[0],query[0]-b[2])),dy=dmax(0.,dmax(b[1]-query[1],query[1]-b[3]));return dx*dx+dy*dy; };
    const auto visit=[&](auto&& self,std::size_t id)->void { if(id==SIZE_MAX)return;auto node=entries_[id];
        double dx=query[0]-node.point[0],dy=query[1]-node.point[1];best=dmin(best,dx*dx+dy*dy);
        auto ld=lowerBound(node.left),rd=lowerBound(node.right);auto near=ld<=rd?node.left:node.right,far=ld<=rd?node.right:node.left;
        if(dmin(ld,rd)<=best)self(self,near);if(dmax(ld,rd)<=best)self(self,far); };
    visit(visit,root_);return std::sqrt(best);
}
double detail::quantile(compat::Slice<double> values,double fraction) {
    enforceEvidence(!values.empty(),"Empty measured support",235);auto a=values.duplicate();sort(a,[](double x,double y){return x<y;});
    double index=fraction*(a.size()-1);auto first=static_cast<std::size_t>(index),last=std::min(first+1,a.size()-1);
    return a[first]*(1-(index-first))+a[last]*(index-first);
}
Point2 ngRigMeasuredSection(compat::Slice<Point2> cloud,double fraction,Point2 axis) {
    auto length=norm(axis);enforceEvidence(length>0&&!cloud.empty(),"Degenerate measured section",248);for(auto& x:axis)x/=length;
    Point2 perpendicular={axis[1],-axis[0]};compat::Slice<double> longitudinal,transverse;
    for(auto p:cloud)longitudinal.append(p[0]*axis[0]+p[1]*axis[1]);
    auto low=quantile(longitudinal,.01),high=quantile(longitudinal,.99),station=low+fraction*(high-low);
    for(std::size_t i=0;i<cloud.size();++i)if(std::abs(longitudinal[i]-station)<=dmax(1.,(high-low)*.025))transverse.append(cloud[i][0]*perpendicular[0]+cloud[i][1]*perpendicular[1]);
    if(transverse.empty()) {
        compat::Slice<std::size_t> order;for(std::size_t i=0;i<cloud.size();++i)order.append(i);
        sort(order,[&](auto a,auto b){return std::abs(longitudinal[a]-station)<std::abs(longitudinal[b]-station);});
        for(auto i:order.slice(0,std::max(std::size_t{1},cloud.size()/100)))transverse.append(cloud[i][0]*perpendicular[0]+cloud[i][1]*perpendicular[1]);
    }
    auto coordinate=quantile(transverse,.5);return {coordinate*perpendicular[0]+station*axis[0],coordinate*perpendicular[1]+station*axis[1]};
}
}
