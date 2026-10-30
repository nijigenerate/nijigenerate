#pragma once

#include "command.hpp"
#include <memory>
#include <type_traits>

namespace nijigenerate::commands::detail {

// One real command object supplies the erased base call and typed source entry.
template<class Concrete, class Storage = CommandNoArguments>
class CovariantCommand : public ExCommand<Storage>, public std::enable_shared_from_this<Concrete> {
    using Base = ExCommand<Storage>;
public:
    using Base::Base;

    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override {
        using Entry = std::shared_ptr<typename Concrete::Result> (Concrete::*)(const std::shared_ptr<Context>&);
        static_assert(std::is_same_v<decltype(&Concrete::run), Entry>,
            "A covariant command must declare its genuine typed run entry");
        constexpr Entry entry = &Concrete::run;
        return (static_cast<Concrete*>(this)->*entry)(context);
    }
};

} // namespace nijigenerate::commands::detail
