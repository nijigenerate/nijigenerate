#pragma once
#include <cstdint>
#include <concepts>
#include <memory>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include "detail/model_command_map.hpp"

namespace nijigenerate::commands { class Command; }

namespace nijigenerate::commands::model::set_deform_binding {

enum class ModelCommand : std::int32_t {
    SetDeformBinding = 0,
    SetTRSBinding = 1,
    SetRotationYBinding = 2,
    SetRotationXBinding = 3
};

class SetDeformBindingCommand;
class SetTRSBindingCommand;
class SetRotationYBindingCommand;
class SetRotationXBindingCommand;

using CommandTable = ::nijigenerate::core::selector::SelectorMap<
    ModelCommand, std::shared_ptr<::nijigenerate::commands::Command>>;
extern thread_local CommandTable commands;

void ngInitModelBindingCommands();

}

#include "set_deform_binding_commands.hpp"
