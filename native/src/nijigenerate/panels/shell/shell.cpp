#include "shell.hpp"
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/widgets/output_tree.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <array>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::panels::shell {
using namespace widgets;
using namespace widgets::output;
using namespace core::selector;
using core::translation::ngTranslateRetained;
namespace {
template<class Operation> auto allocateBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString duplicate(compat::RetainedString text) {
    if (text.empty()) return {};
    return allocateBytes([&] { return compat::RetainedString(std::string(text.view())); });
}
template<class T, class... Args> auto createSourceObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
std::shared_ptr<ListOutput> createListOutput(CommandIssuerPtr issuer) {
    nicxlive::core::d_runtime::SourceAllocator<ListOutput> allocator;
    auto* storage = allocator.allocate(1);
    try {
        auto store = createSourceObject<TreeStore>();
        allocator.construct(storage, std::move(store), std::move(issuer));
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    const auto destroy = [](ListOutput* value) noexcept {
        std::destroy_at(value);
        nicxlive::core::d_runtime::SourceAllocator<ListOutput>{}.deallocate(value, 1);
    };
    return std::shared_ptr<ListOutput>(storage, destroy,
        nicxlive::core::d_runtime::SourceAllocator<ListOutput>{});
}
}

Command::Command(compat::RetainedString text, widgets::output::OutputPtr result)
    : command(std::move(text)), output(std::move(result)) { prepareSourceClassInfo(); }
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Command::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Command),
            compat::RetainedString::fromStaticLiteral("nijigenerate.panels.shell.shell.Command"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString Command::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Command::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Command::opEquals(std::shared_ptr<Command> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Command::opCmp(std::shared_ptr<Command>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = allocateBytes([&] {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    });
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
ShellPanel::ShellPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Shell"),
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Shell")), false) {}
void ngInitializeShellPanelModule() { ngInitializePanelModule<ShellPanel>(); }
void ShellPanel::updatePreview() {
    const auto receiver = shared_from_this();
    // Preview uses fromStringz; the Enter path below deliberately retains hidden NUL bytes.
    const auto copied = duplicate(command);
    const compat::SourceCString terminated(copied.view());
    compat::detail::ngSourceCStringPostconditions(copied.view(), terminated.data());
    const auto newCommand = terminated.retainedString();
    try {
        auto selector = createSourceObject<Selector>();
        selector->build(SelectorString(newCommand));
        const auto nodes = selector->run();
        if (newCommand.empty() || !nodes.empty()) {
            if (!latestOutput) latestOutput = createListOutput(receiver);
            compat::ngRequireObject(std::dynamic_pointer_cast<ListOutput>(latestOutput))->setResources(nodes);
        }
    } catch (const SelectorUtfException&) {}
}
void ShellPanel::onUpdate() {
    const auto receiver = shared_from_this();
    if (!nextCommand.isNull()) {
        command = nextCommand;
        nextCommand.reset();
    }
    const auto avail = ngAvailableSpace();
    if (ngInputText(compat::RetainedStringSlice(
            compat::RetainedString::fromStaticLiteral("(Command)")), avail.x, command) || forceUpdatePreview) {
        updatePreview();
        forceUpdatePreview = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        const auto newCommand = duplicate(command);
        auto selector = createSourceObject<Selector>();
        selector->build(SelectorString(newCommand));
        const auto nodes = selector->run();
        auto output = createListOutput(receiver);
        output->setResources(nodes);
        command = compat::RetainedString::emptyLiteral();
        history.append(createSourceObject<Command>(newCommand, output));
        latestOutput.reset();
        setCommand(compat::RetainedString::fromStaticLiteral(" "));
    }
    if (ImGui::BeginChild("ShellMain", ImVec2(0, -30), false)) {
        if (latestOutput) latestOutput->onUpdate();
        const auto entries = history;
        for (std::size_t end = entries.size(); end != 0; --end) {
            const auto index = end - 1;
            const auto entry = entries[index];
            using namespace core::translation;
            const std::array args{ngFormatArgument(index),
                ngFormatArgument(compat::ngRequireObject(entry)->command)};
            const auto text = ngFormatTyped("[%d] %s", args);
            const compat::SourceCString textz{std::string_view(text)};
            compat::detail::ngSourceCStringPostconditions(text, textz.data());
            // Source passes the formatted text to igText as its format string.
            ImGui::Text(textz.data());
            const std::array labelArgs{ngFormatArgument(index)};
            const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Output [%d]"));
            const auto label = ngFormatTyped(pattern.view(), labelArgs);
            const compat::SourceCString labelz{std::string_view(label)};
            compat::detail::ngSourceCStringPostconditions(label, labelz.data());
            if (ngBeginCategory(labelz.data(), IncCategoryFlags::DefaultClosed))
                compat::ngRequireObject(compat::ngRequireObject(entry)->output)->onUpdate();
            ngEndCategory();
        }
    }
    ImGui::EndChild();
}
void ShellPanel::addCommand(compat::RetainedString text) {
    nextCommand = allocateBytes([&] {
        auto value = std::string(command.view());
        value.append(text.view());
        value.push_back('\0');
        return compat::RetainedString(std::move(value));
    });
    forceUpdatePreview = true;
}
void ShellPanel::setCommand(compat::RetainedString text) {
    nextCommand = allocateBytes([&] {
        auto value = std::string(text.view());
        value.push_back('\0');
        return compat::RetainedString(std::move(value));
    });
    forceUpdatePreview = true;
}
void ShellPanel::addPopup(core::selector::ResourcePtr) {}
}
