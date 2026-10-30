#pragma once

#include "framework_primitives.hpp"
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::autorig::framework {

struct AutoRigPortSpec {
    compat::RetainedStringSlice id;
    AutoRigValueKind kind{};
    bool required = true;
    compat::RetainedStringSlice mediaType;
    compat::RetainedStringSlice description;
    compat::RetainedStringSlice viewHint;
};

struct AutoRigConnection {
    compat::RetainedStringSlice input;
    compat::RetainedStringSlice sourceTask;
    compat::RetainedStringSlice sourceOutput;
};

struct AutoRigTaskSpec {
    compat::RetainedStringSlice id;
    compat::RetainedStringSlice label;
    compat::Slice<compat::RetainedStringSlice> dependencies;
    compat::Slice<AutoRigPortSpec> inputs;
    compat::Slice<AutoRigPortSpec> outputs;
    compat::Slice<AutoRigConnection> connections;
    bool retainFailureOutputs{};
    bool ownsActionBoundary{};
    bool reclaimScratchMemory{};
};

struct AutoRigWorkflowStep {
    compat::RetainedStringSlice id;
    compat::RetainedStringSlice processorId;
    compat::RetainedStringSlice taskId;
    compat::Slice<compat::RetainedStringSlice> dependencies;
};

struct AutoRigWorkflowInputBinding {
    compat::RetainedStringSlice workflowInput;
    compat::RetainedStringSlice targetStep;
    compat::RetainedStringSlice targetTask;
    compat::RetainedStringSlice targetPort;
};

struct AutoRigWorkflowConnection {
    compat::RetainedStringSlice sourceStep;
    compat::RetainedStringSlice sourcePort;
    compat::RetainedStringSlice targetStep;
    compat::RetainedStringSlice targetTask;
    compat::RetainedStringSlice targetPort;
};

struct AutoRigWorkflowOutputBinding {
    compat::RetainedStringSlice workflowOutput;
    compat::RetainedStringSlice sourceStep;
    compat::RetainedStringSlice sourcePort;
};

} // namespace nijigenerate::autorig::framework

namespace nicxlive::core::d_runtime {
#define NG_FRAMEWORK_SCHEMA_ORIGINS(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::autorig::framework::Type> { \
    inline static constexpr bool isNativeObject = false; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.framework." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.framework." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigPortSpec)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigConnection)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigTaskSpec)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigWorkflowStep)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigWorkflowInputBinding)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigWorkflowConnection)
NG_FRAMEWORK_SCHEMA_ORIGINS(AutoRigWorkflowOutputBinding)
#undef NG_FRAMEWORK_SCHEMA_ORIGINS
}
