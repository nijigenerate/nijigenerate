#pragma once

#include <math/types.hpp>
#include <compat/object_metadata.hpp>
#include <functional>
#include <memory>
#include <cmath>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/commands/binding/base.hpp>

namespace nicxlive::core::param { class Parameter; }
namespace nijigenerate::ext { class ExParameterGroup; }

namespace nijigenerate::panels::parameters {

// This is M185's own source type, distinct from the binding command payload.
struct ParamDragDropData {
    std::shared_ptr<::nicxlive::core::param::Parameter> param;
};

namespace detail {
// Descriptor copies retain one actual callable environment, not a closure copy.
using ParameterUiCommand = compat::SharedDelegate<void()>;
extern thread_local ParameterUiCommand pendingParameterUiCommand;
bool queueParameterUiCommand(ParameterUiCommand action);
// The raw payload address is backed by a retained TLS owner in the view implementation.
extern thread_local ParamDragDropData* dragDropData;
void pushColorScheme(::nicxlive::core::math::Vec3 color);
void popColorScheme();
void setTransparency(float alpha, float text);
void resetTransparency();
} // namespace detail

void incRunPendingParameterUiCommand();
using ParameterRef=std::shared_ptr<::nicxlive::core::param::Parameter>;
using BindingList=commands::binding::BindingList;
bool incKeypointActions(ParameterRef,BindingList sourceBindings,BindingList targetBindings);
void incBindingMenuContents(ParameterRef,commands::binding::SelectedBindings);
void incBindingList(ParameterRef);
void ngParameterViewEditButtons(bool armed,bool horizontal,std::size_t,ParameterRef,bool childVisible);
void ngParameterView(bool armed,bool showCategory,bool fixedWidth,std::size_t,ParameterRef,
    compat::RetainedString* grabParameter,bool canGroup,::nicxlive::core::math::Vec3 color);
template<bool armed=false,bool showCategory=true,bool fixedWidth=false,class Array>
void incParameterView(std::size_t index,ParameterRef parameter,compat::RetainedString* grabParameter,
    bool canGroup,Array& parameters,::nicxlive::core::math::Vec3 color={NAN,NAN,NAN}) {
    (void)parameters; // The original reference is only forwarded to an unused formal argument.
    ngParameterView(armed,showCategory,fixedWidth,index,std::move(parameter),grabParameter,canGroup,color);
}
template<bool armed,bool horizontal,class Array>
void incParameterViewEditButtons(std::size_t index,ParameterRef parameter,Array& parameters,bool childVisible=true) {
    (void)parameters;ngParameterViewEditButtons(armed,horizontal,index,std::move(parameter),childVisible);
}
void incParameterMenuContents(::nicxlive::core::common::SharedArray<ParameterRef>);
void incParameterMenuContents(compat::Slice<ParameterRef>);
bool incParameterGropuMenuContents(std::shared_ptr<ext::ExParameterGroup>);
compat::Vec2u incParamPoint();

} // namespace nijigenerate::panels::parameters

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::parameters::ParamDragDropData> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.panels.parameters.ParamDragDropData)"),
            RetainedString::fromStaticLiteral("(ParamDragDropData)"),
            RetainedString::fromStaticLiteral("nijigenerate.panels.parameters.ParamDragDropData"),
            RetainedString::fromStaticLiteral("ParamDragDropData")};
    }
};
}

// Importing the original module exposes its concrete panel as well as helpers.
#include "parameters_panel.hpp"
