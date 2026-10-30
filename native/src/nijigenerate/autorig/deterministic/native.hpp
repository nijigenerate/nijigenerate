#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <memory>

namespace nicxlive::core::nodes { class Part; }

namespace nijigenerate::autorig::deterministic::native {

// Authored exports; native_stage.hpp publishes the retained stage entry point.
void ngRigForgetNativeSession(compat::RetainedString sessionId = {});
bool ngRigMaterialIsActive(std::shared_ptr<nicxlive::core::nodes::Part> part);

} // namespace nijigenerate::autorig::deterministic::native

#include "native_stage.hpp"
