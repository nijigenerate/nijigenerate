#pragma once

#include <nijigenerate/commands/command.hpp>
#include <cstdint>

// Private declarations from regression.d704-715, not additional regression cases.
namespace nijigenerate_tests::regression::detail {
class RegressionMetaCommand;
class RegressionArgCommand;
class RegressionNonRunnableCommand;
struct RegressionArguments {
    nijigenerate::compat::RetainedString name;
    std::int32_t arg1 = 0;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<nijigenerate_tests::regression::detail::RegressionMetaCommand> {
    inline static constexpr char className[] = "nijigenerate_tests.regression.RegressionMetaCommand";
    inline static constexpr char templateArgumentName[] = "(nijigenerate_tests.regression.RegressionMetaCommand)";
};
template<> struct SourceObjectTypeNames<nijigenerate_tests::regression::detail::RegressionArgCommand> {
    inline static constexpr char className[] = "nijigenerate_tests.regression.RegressionArgCommand";
    inline static constexpr char templateArgumentName[] = "(nijigenerate_tests.regression.RegressionArgCommand)";
};
template<> struct SourceObjectTypeNames<nijigenerate_tests::regression::detail::RegressionNonRunnableCommand> {
    inline static constexpr char className[] = "nijigenerate_tests.regression.RegressionNonRunnableCommand";
    inline static constexpr char templateArgumentName[] = "(nijigenerate_tests.regression.RegressionNonRunnableCommand)";
};
}

namespace nijigenerate::commands {
template<> struct CommandFields<nijigenerate_tests::regression::detail::RegressionArguments> {
    using Storage = nijigenerate_tests::regression::detail::RegressionArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::name, "name", "Name", "string">{},
        FieldDescriptor<&Storage::arg1, "arg1", "", "int", false>{}};
};
template<> struct CommandTraits<nijigenerate_tests::regression::detail::RegressionMetaCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, McpHidden, GuiDialog, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<nijigenerate_tests::regression::detail::RegressionArgCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<
        ExCommand<nijigenerate_tests::regression::detail::RegressionArguments>, Command>;
};
}

namespace nijigenerate_tests::regression::detail {
class RegressionMetaCommand : public nijigenerate::commands::ExCommand<> {
protected:
    void prepareSourceClassInfo() override {
        ExCommand::prepareSourceClassInfo();
        nijigenerate::commands::detail::prepareCommandResultClass<RegressionMetaCommand>();
    }
public:
    RegressionMetaCommand()
        : ExCommand(nijigenerate::compat::RetainedString::fromStaticLiteral("Meta Label"),
            nijigenerate::compat::RetainedString::fromStaticLiteral("Meta Description")) {
        // The original registration only observes dynamic identity and builds
        // metadata; it does not retain this object. Use that same actual callee
        // without manufacturing a shared owner while the constructor is active.
        const auto key = std::type_index(typeid(*this));
        auto metadata = nijigenerate::commands::ngBuildCommandMetadata<RegressionMetaCommand>();
        nijigenerate::commands::detail::registerCommandMeta(key, std::move(metadata));
    }
};

class RegressionArgCommand : public nijigenerate::commands::ExCommand<RegressionArguments> {
protected:
    void prepareSourceClassInfo() override {
        ExCommand::prepareSourceClassInfo();
        nijigenerate::commands::detail::prepareCommandResultClass<RegressionArgCommand>();
    }
public:
    RegressionArgCommand()
        : ExCommand(nijigenerate::compat::RetainedString::fromStaticLiteral("Arg Label"),
            nijigenerate::compat::RetainedString::fromStaticLiteral("Arg Description"),
            nijigenerate::compat::RetainedString::fromStaticLiteral("Alice"), std::int32_t{7}) {}
};

class RegressionNonRunnableCommand : public nijigenerate::commands::ExCommand<> {
protected:
    void prepareSourceClassInfo() override {
        ExCommand::prepareSourceClassInfo();
        nijigenerate::commands::detail::prepareCommandResultClass<RegressionNonRunnableCommand>();
    }
public:
    bool didRun = false;
    RegressionNonRunnableCommand()
        : ExCommand(nijigenerate::compat::RetainedString::fromStaticLiteral("Non-runnable"),
            nijigenerate::compat::RetainedString::fromStaticLiteral("Regression command which must not run")) {}
    bool runnable(std::shared_ptr<nijigenerate::commands::Context>) override { return false; }
    std::shared_ptr<nijigenerate::commands::CommandResult> run(
        std::shared_ptr<nijigenerate::commands::Context>) override {
        didRun = true;
        return nijigenerate::commands::CommandResult::opCall(true);
    }
};
}
