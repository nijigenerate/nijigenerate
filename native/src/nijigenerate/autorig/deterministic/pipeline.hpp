#pragma once
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
namespace nijigenerate::autorig::deterministic::pipeline {
using RigNativeStage = compat::SharedDelegate<framework::Json(compat::RetainedStringSlice,
    framework::Json, framework::Json, compat::ConstSlice<std::uint8_t>,
    std::shared_ptr<framework::AutoRigTaskContext>)>;
compat::Slice<framework::AutoRigTaskSpec> ngRigPipelineTasks();
framework::AutoRigWorkflowSpec ngRigModelWorkflow(compat::RetainedString provider);
}
