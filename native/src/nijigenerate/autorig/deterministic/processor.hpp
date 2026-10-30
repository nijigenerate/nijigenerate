#pragma once
#include "pipeline.hpp"
#include <compat/object_metadata.hpp>
namespace nijigenerate::autorig::deterministic::processor {
class AnimeFrontViewRigProcessor : public framework::AutoRigProcessor {
public:
    using Projection = compat::SharedDelegate<framework::Json(framework::Json, framework::Json,
        std::shared_ptr<framework::AutoRigTaskContext>)>;
    explicit AnimeFrontViewRigProcessor(Projection projection = {}, pipeline::RigNativeStage stage = {});
    framework::Text procId() override;
    framework::Text displayName() override;
    compat::Slice<framework::AutoRigTaskSpec> tasks() override;
    compat::Slice<framework::AutoRigWorkflowSpec> workflows() override;
    void executeTask(framework::Text, std::shared_ptr<framework::AutoRigTaskContext>) override;
protected:
    void prepareSourceClassInfo() override;
private:
    Projection applyProjection_;
    pipeline::RigNativeStage nativeStage_;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::autorig::deterministic::processor::AnimeFrontViewRigProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.autorig.deterministic.processor.AnimeFrontViewRigProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(AnimeFrontViewRigProcessor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.autorig.deterministic.processor.AnimeFrontViewRigProcessor";
    inline static constexpr char unqualifiedElement[] = "AnimeFrontViewRigProcessor";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::autorig::deterministic::processor::AnimeFrontViewRigProcessor> {
    inline static constexpr char className[] =
        "nijigenerate.autorig.deterministic.processor.AnimeFrontViewRigProcessor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.autorig.deterministic.processor.AnimeFrontViewRigProcessor)";
};
}
