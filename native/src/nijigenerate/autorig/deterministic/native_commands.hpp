#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/context.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

std::shared_ptr<commands::CommandResult> command(std::shared_ptr<commands::Command> action,
    std::shared_ptr<commands::Context> context);
std::shared_ptr<nicxlive::core::nodes::Node> create(std::shared_ptr<commands::Command> action,
    std::shared_ptr<commands::Context> context);

} // namespace nijigenerate::autorig::deterministic::native_detail
