#pragma once
#include <nijigenerate/commands/binding/base.hpp>
#include <cstdint>

namespace nijigenerate::ext { class ExParameterGroup; }

namespace nijigenerate::commands::parameter {
inline thread_local compat::Slice<binding::ResourceRef> cAllBoundNodes;
} // namespace nijigenerate::commands::parameter

namespace nijigenerate::commands::parameter::base {
using ::nijigenerate::commands::parameter::cAllBoundNodes;
using ParameterRef = std::shared_ptr<::nicxlive::core::param::Parameter>;
using ParameterGroupRef = std::shared_ptr<::nijigenerate::ext::ExParameterGroup>;

void mirrorAll(ParameterRef param, std::uint32_t axis);
void mirroredAutofill(ParameterRef param, std::uint32_t axis, float min, float max);
template<bool pushAction = true>
void pasteParameter(ParameterRef param, ParameterRef srcParam = {}, std::uint32_t axis = 2);
void convertTo2D(ParameterRef param);
bool removeParameter(ParameterRef param);
void incMoveParameter(ParameterRef from, ParameterGroupRef to = {}, std::int32_t index = 0);
ParameterGroupRef incCreateParamGroup(std::int32_t index = 0);
} // namespace nijigenerate::commands::parameter::base
