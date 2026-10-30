#include "detail/physics.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <memory>
#include <new>
#include <source_location>
namespace nijigenerate::autorig::deterministic::physics {
using namespace detail;
namespace {
struct SupportIndexDeleter {
    void operator()(evidence::AlphaSupportIndex* value) const noexcept {
        std::destroy_at(value);
        nicxlive::core::d_runtime::SourceAllocator<evidence::AlphaSupportIndex>{}.deallocate(value, 1);
    }
};
using SupportIndex = std::unique_ptr<evidence::AlphaSupportIndex, SupportIndexDeleter>;
SupportIndex makeSupportIndex(Slice<Point2> points) {
    nicxlive::core::d_runtime::SourceAllocator<evidence::AlphaSupportIndex> allocator;
    auto* storage = allocator.allocate(1);
    try {
        std::construct_at(storage, points);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    return SupportIndex(storage);
}
}
Slice<double> ngRigPhysicsWeights(Json spec,Slice<Point2> points,Json policy) {
    const auto fixedPoint=ngRigPoint(spec.sourceIndexValue("fixed"));
    const auto freePoint=ngRigPoint(spec.sourceIndexValue("free"));
    auto f=ngRigPhysicsFrame(fixedPoint,freePoint);
    double band=ngRigNumber(policy.sourceIndexValue("anchor_band_fraction"));auto limb=get(spec,"limb");
    Slice<Point2> joints;
    double radius=NAN;
    if(!limb.is_null()) {
        joints=ngRigPoints(limb.sourceIndexValue("joints"));
        radius=ngRigNumber(limb.sourceIndexValue("radius"));
    }
    auto protectedPoints=ngRigPoints(get(spec,"protected_pixels",Array{})),boundaryPoints=ngRigPoints(get(spec,"boundary_pixels",Array{}));
    SupportIndex protectedIndex,boundaryIndex;
    if(!protectedPoints.empty())protectedIndex=makeSupportIndex(protectedPoints);
    if(!boundaryPoints.empty())boundaryIndex=makeSupportIndex(boundaryPoints);
    auto result=physics_detail::newArray<double>(points.size());
    for(std::size_t i=0;i<points.size();++i){auto p=points[i];auto s=dot(sub(p,f.origin),f.normal)/f.length;double w=smooth((s-band)/(1-band));
        if(spec.sourceIndexValue("pattern").as_string().view()=="two_ends") {
            auto t=smooth((s-band)/(1-2*band));w=16*t*t*(1-t)*(1-t);
        }
        if(!joints.empty()){w*=smooth((chainDistance(p,joints)-radius)/(radius*.75));for(auto joint:joints)w*=smooth((norm(sub(p,joint))-radius)/(radius*.75));}
        if(protectedIndex) {
            auto margin=ngRigNumber(spec.sourceIndexValue("protection_margin"));
            w*=smooth((protectedIndex->distance(p)-margin)/margin);
        }
        if(boundaryIndex) {
            const auto distance=boundaryIndex->distance(p);
            const auto margin=ngRigNumber(spec.sourceIndexValue("boundary_margin"));
            w*=smooth(distance/margin);
        }
        result[i]=clamp(w,0,1);
    }return result;
}
Slice<Point2> ngRigPhysicsDisplacement(Json spec,Slice<Point2> points,Point2 key,Json policy) {
    const auto fixedPoint=ngRigPoint(spec.sourceIndexValue("fixed"));
    const auto freePoint=ngRigPoint(spec.sourceIndexValue("free"));
    auto f=ngRigPhysicsFrame(fixedPoint,freePoint);
    const auto profiles=policy.sourceIndexValue("profiles");
    const auto profileName=spec.sourceIndexValue("profile").as_string();
    const auto profile=profiles.sourceIndexValue(profileName.view());
    auto amplitude=ngRigNumbers(profile.sourceIndexValue("amplitude_ratio"));
    const auto amplitudeAt = [&amplitude](std::size_t index, std::size_t sourceLine) {
        if (index >= amplitude.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, amplitude.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
            sourceLine);
        return amplitude[index];
    };
    const auto tangentX = amplitudeAt(0,373)*key[0]*f.tangent[0];
    const auto normalX = amplitudeAt(1,373)*key[1]*f.normal[0];
    const auto vectorX = f.length*(tangentX-normalX);
    const auto tangentY = amplitudeAt(0,374)*key[0]*f.tangent[1];
    const auto normalY = amplitudeAt(1,374)*key[1]*f.normal[1];
    Point2 vector{vectorX, f.length*(tangentY-normalY)};
    auto weights=ngRigPhysicsWeights(spec,points,policy);
    auto result=physics_detail::newArray<Point2>(points.size());
    for(std::size_t i=0;i<weights.size();++i){auto w=weights[i];result[i]=Point2{w*vector[0],w*vector[1]};}
    return result;
}
Slice<Point2> ngRigPhysicsProtectTriangles(Slice<Point2> delta,Slice<Point2> vertices,Slice<std::uint32_t> indices,Json spec,Json policy) {
    enforce(delta.size()==vertices.size() && indices.size()%3==0,"Invalid physics triangle arrays",382);
    auto result=physics_detail::duplicatePoints(delta);auto weights=ngRigPhysicsWeights(spec,vertices,policy);
    for(std::size_t i=0;i<indices.size();i+=3){const auto face=indices.slice(i,i+3);bool fixed=false;
        for(auto id:face){enforce(id<vertices.size(),"Physics triangle outside mesh",386);fixed|=weights[id]<=1e-12;}
        if(fixed)for(auto id:face)result[id]=Point2{0.,0.};
    }return result;
}
}
