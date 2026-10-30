#include "composite_commands.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitCompositeInspectorCommands() {
    {auto command=std::allocate_shared<CompositeTintCommand>(nicxlive::core::d_runtime::SourceAllocator<CompositeTintCommand> {}); ngRegisterCommandMeta(command); commands[InspectorNodeApplyCommand::CompositeTint]=command;}
    {auto command=std::allocate_shared<CompositeScreenTintCommand>(nicxlive::core::d_runtime::SourceAllocator<CompositeScreenTintCommand> {}); ngRegisterCommandMeta(command); commands[InspectorNodeApplyCommand::CompositeScreenTint]=command;}
    {auto command=std::allocate_shared<CompositeBlendingModeCommand>(nicxlive::core::d_runtime::SourceAllocator<CompositeBlendingModeCommand> {}); ngRegisterCommandMeta(command); commands[InspectorNodeApplyCommand::CompositeBlendingMode]=command;}
    {auto command=std::allocate_shared<CompositeOpacityCommand>(nicxlive::core::d_runtime::SourceAllocator<CompositeOpacityCommand> {}); ngRegisterCommandMeta(command); commands[InspectorNodeApplyCommand::CompositeOpacity]=command;}
    {auto command=std::allocate_shared<CompositeThresholdCommand>(nicxlive::core::d_runtime::SourceAllocator<CompositeThresholdCommand> {}); ngRegisterCommandMeta(command); commands[InspectorNodeApplyCommand::CompositeThreshold]=command;}
}
}
namespace nijigenerate::panels::inspector::composite {
std::shared_ptr<commands::CommandResult> ngRunCompositePropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand id,std::shared_ptr<commands::Context> context,CompositePropertyValue value) {
    using namespace commands::inspector::apply_node;
    switch(id) {
#define NG_DISPATCH_COMPOSITE(Name) \
    case InspectorNodeApplyCommand::Name: \
        return ::nijigenerate::commands::ngDispatchRegisteredCommand<Name##Command>( \
            ::nijigenerate::commands::inspector::apply_node::commands,id,context, \
            {"InspectorNodeApplyCommand." #Name,"InspectorNodeApplyCommand",#Name "Command"}, \
            std::get<std::reference_wrapper<const Name##Access::Value>>(value).get())
    NG_DISPATCH_COMPOSITE(CompositeTint);
    NG_DISPATCH_COMPOSITE(CompositeScreenTint);
    NG_DISPATCH_COMPOSITE(CompositeBlendingMode);
    NG_DISPATCH_COMPOSITE(CompositeOpacity);
    NG_DISPATCH_COMPOSITE(CompositeThreshold);
#undef NG_DISPATCH_COMPOSITE
    default: throw std::logic_error("Native Composite inspector caller supplied an untranslated property expression");
    }
}
std::string ngCompositeBlendName(BlendMode mode) {
    switch(mode) {
    case BlendMode::Normal: return "Normal";
    case BlendMode::Multiply: return "Multiply";
    case BlendMode::Screen: return "Screen";
    case BlendMode::Overlay: return "Overlay";
    case BlendMode::Darken: return "Darken";
    case BlendMode::Lighten: return "Lighten";
    case BlendMode::ColorDodge: return "ColorDodge";
    case BlendMode::LinearDodge: return "LinearDodge";
    case BlendMode::AddGlow: return "AddGlow";
    case BlendMode::ColorBurn: return "ColorBurn";
    case BlendMode::HardLight: return "HardLight";
    case BlendMode::SoftLight: return "SoftLight";
    case BlendMode::Difference: return "Difference";
    case BlendMode::Exclusion: return "Exclusion";
    case BlendMode::Subtract: return "Subtract";
    case BlendMode::Inverse: return "Inverse";
    case BlendMode::DestinationIn: return "DestinationIn";
    case BlendMode::ClipToLower: return "ClipToLower";
    case BlendMode::SliceFromLower: return "SliceFromLower";
    default: {
        // std.conv.text uses the source enum name before the base-value format.
        const std::array arguments{core::translation::ngFormatArgument(static_cast<int>(mode))};
        const auto formatted=core::translation::ngFormatTyped("%s",arguments);
        try {
            return "cast(BlendMode)"+formatted;
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    }
}
}
