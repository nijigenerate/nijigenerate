#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/selector/resource.hpp>
#include <compat/object_metadata.hpp>
#include <memory>

namespace nijigenerate::widgets::output {

class CommandIssuer {
public:
    CommandIssuer() = default;
    CommandIssuer(const CommandIssuer&) = delete;
    CommandIssuer& operator=(const CommandIssuer&) = delete;
    virtual ~CommandIssuer() = default;
    virtual void addCommand(compat::RetainedString command) = 0;
    virtual void setCommand(compat::RetainedString command) = 0;
    virtual void addPopup(core::selector::ResourcePtr resource) = 0;
};
using CommandIssuerPtr = std::shared_ptr<CommandIssuer>;

class Output : public std::enable_shared_from_this<Output> {
public:
    Output() = default;
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;
    virtual ~Output() = default;
    virtual void onUpdate() = 0;
    virtual void reset() = 0;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Output> other);
    virtual int opCmp(std::shared_ptr<Output> other);
};
using OutputPtr = std::shared_ptr<Output>;

class ViewOutput : public Output {
protected:
    CommandIssuerPtr panel;
    compat::Slice<core::selector::ResourcePtr> nodes;
    core::selector::ResourcePtr focused;
    compat::RetainedString parameterGrabStr;

public:
    explicit ViewOutput(CommandIssuerPtr panel);
    void onUpdate() override;
    virtual void refresh(compat::Slice<core::selector::ResourcePtr> resources);
    virtual void addResources(compat::Slice<core::selector::ResourcePtr> nodes);
    void reset() override;
};

} // namespace nijigenerate::widgets::output

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::CommandIssuer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.output.CommandIssuer)";
    inline static constexpr char unqualifiedArgument[] = "(CommandIssuer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.CommandIssuer";
    inline static constexpr char unqualifiedElement[] = "CommandIssuer";
};
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::Output> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.output.Output)";
    inline static constexpr char unqualifiedArgument[] = "(Output)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.Output";
    inline static constexpr char unqualifiedElement[] = "Output";
};
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::ViewOutput> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.output.ViewOutput)";
    inline static constexpr char unqualifiedArgument[] = "(ViewOutput)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.ViewOutput";
    inline static constexpr char unqualifiedElement[] = "ViewOutput";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::output::ViewOutput> {
    inline static constexpr char className[] = "nijigenerate.widgets.output.ViewOutput";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.output.ViewOutput)";
};
}
