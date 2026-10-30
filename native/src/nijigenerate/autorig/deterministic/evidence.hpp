#pragma once
#include "contracts.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <memory>
#include <vector>
#include <array>
#include <limits>
#include <compat/object_metadata.hpp>
namespace nijigenerate::autorig::deterministic::evidence {
using contracts::Point2;
using Json=core::settings::SettingsJsonValue;
using JsonArray=core::settings::SettingsJsonArray;
using Strings=compat::Slice<compat::RetainedStringSlice>;
void ngRigPropagateClippingVisibility(JsonArray&);
void ngRigResolveGroupReceivers(JsonArray&,JsonArray,bool classified=false);
void ngRigPropagateClippingStatic(JsonArray&,JsonArray);
compat::RetainedStringSlice ngRigMaterialFeature(compat::RetainedStringSlice,Strings ancestors={});
Strings ngRigMaterialRoleCandidates(compat::RetainedStringSlice);
Point2 ngRigMeasuredSection(compat::Slice<Point2>,double,Point2 axis={0.,1.});
Json ngRigClassifyMaterials(Json,Json,std::shared_ptr<framework::AutoRigTaskContext> context={});
Json ngRigDeriveEvidence(Json);
class AlphaSupportIndex {
public:
    explicit AlphaSupportIndex(compat::Slice<Point2>);
    virtual ~AlphaSupportIndex() = default;
    AlphaSupportIndex(const AlphaSupportIndex&) = delete;
    AlphaSupportIndex& operator=(const AlphaSupportIndex&) = delete;
    virtual double distance(Point2);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AlphaSupportIndex> other);
    virtual int opCmp(std::shared_ptr<AlphaSupportIndex> other);
protected:
    virtual void prepareSourceClassInfo();
private:
    static constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    struct Entry {
        Point2 point{nan, nan};
        std::array<double,4> bounds{nan, nan, nan, nan};
        std::size_t left = SIZE_MAX, right = SIZE_MAX;
    };
    compat::Slice<Entry> entries_;
    std::size_t root_=0;
    std::size_t build(compat::Slice<Point2>,std::size_t);
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::evidence::AlphaSupportIndex> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.evidence.AlphaSupportIndex)";
    inline static constexpr char unqualifiedArgument[] = "(AlphaSupportIndex)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.evidence.AlphaSupportIndex";
    inline static constexpr char unqualifiedElement[] = "AlphaSupportIndex";
};
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::deterministic::evidence::AlphaSupportIndex> {
    inline static constexpr char className[] = "nijigenerate.autorig.deterministic.evidence.AlphaSupportIndex";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.deterministic.evidence.AlphaSupportIndex)";
};
}
