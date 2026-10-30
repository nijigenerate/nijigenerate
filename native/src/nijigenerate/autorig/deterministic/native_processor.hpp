#pragma once

#include <nijigenerate/viewport/vertex/automesh/optimum.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

// Actual module-private subclass, with the original UDA exposed below.
class RigPartMeshProcessor : public viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor {
public:
    explicit RigPartMeshProcessor(float spacing, float divisions = 12,
        compat::Slice<float> scales = {}, bool shoulder = false);
protected:
    void prepareSourceClassInfo() override {
        OptimumAutoMeshProcessor::prepareSourceClassInfo();
        registerSourceClassInfo<RigPartMeshProcessor>(
            "nijigenerate.autorig.deterministic.native.RigPartMeshProcessor");
    }
};

} // namespace nijigenerate::autorig::deterministic::native_detail

namespace nijigenerate::viewport::vertex::automesh::meta {
template<> struct AMSourceAttributes<::nijigenerate::autorig::deterministic::native_detail::RigPartMeshProcessor> {
    static constexpr std::string_view identifier = "RigPartMeshProcessor";
    static constexpr auto attributes = std::tuple{AMProcessorSourceAttribute{"autorig-part","AutoRig Part",500}};
};
}
