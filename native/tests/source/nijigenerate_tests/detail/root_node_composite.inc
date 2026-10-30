// Faithful body of regression.d12111-12161; include once outside namespaces.
#include "root_node_composite.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nijigenerate::ext::ExParameter> new2DParameter(nijigenerate::compat::RetainedString name);
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero);
std::shared_ptr<nicxlive::core::nodes::Node> findNodeRecursive(
    std::shared_ptr<nicxlive::core::nodes::Node> root, std::string_view name);
std::shared_ptr<nicxlive::core::param::Parameter> findParameter(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name);

void testNodeCompositeHierarchyBindingRoundTrip() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::node::node;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::ValueParameterBinding;
    resetCase();

    auto parentA = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto parentB = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto child = Node::createSourceDeferred([&] { return parentA; });
    parentA->setName(NullableString::fromStaticLiteral("CompositeParentA"));
    parentB->setName(NullableString::fromStaticLiteral("CompositeParentB"));
    child->setName(NullableString::fromStaticLiteral("CompositeChild"));

    auto param = new2DParameter(RetainedString::fromStaticLiteral("NodeCompositeParam"));
    auto binding = newValueBinding(param, child, RetainedString::fromStaticLiteral("transform.t.x"), true);
    binding->setValue({2, 2}, 33.0f);

    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{child});
    // Passive native expression transport: the actual array is allocated only
    // when the real command's constructor argument is converted in-place.
    struct RenameArrayArgument {
        operator Slice<RetainedString>() && {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("CompositeChildRenamed")};
        }
    };
    require(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
        RenameArrayArgument{})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("node composite should rename child"));
    require(constructSourceCommand<MoveNodeCommand>(parentB, std::uint64_t{0})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("node composite should move child to second parent"));
    require(child->parentPtr() == parentB,
        RetainedString::fromStaticLiteral("node composite move should apply"));

    nijigenerate::core::actionstack::incActionUndo();
    require(child->parentPtr() == parentA, RetainedString::fromStaticLiteral(
        "node composite undo should restore original parent without undoing rename"));
    require(child->getName().view() == "CompositeChildRenamed",
        RetainedString::fromStaticLiteral("node composite undo move should preserve previous rename"));
    nijigenerate::core::actionstack::incActionRedo();
    require(child->parentPtr() == parentB,
        RetainedString::fromStaticLiteral("node composite redo should restore moved parent"));

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-node-composite")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("node-composite")};
        const auto saveBase = ngBuildPath(saveSegments);
        const auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        require(constructSourceCommand<SaveFileCommand>(saveBase)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("node composite should save"));
        nijigenerate::project::incNewProject();
        require(constructSourceCommand<OpenFileCommand>(savePath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("node composite should reopen"));

        auto loadedParentB = findNodeRecursive(nijigenerate::project::incActivePuppet()->root, "CompositeParentB");
        auto loadedChild = findNodeRecursive(nijigenerate::project::incActivePuppet()->root, "CompositeChildRenamed");
        require(loadedParentB != nullptr && loadedChild != nullptr && loadedChild->parentPtr() == loadedParentB,
            RetainedString::fromStaticLiteral("node composite should preserve moved hierarchy after reload"));
        auto loadedParam = findParameter(nijigenerate::project::incActivePuppet(), "NodeCompositeParam");
        auto loadedBinding = std::dynamic_pointer_cast<ValueParameterBinding>(
            nijigenerate::compat::ngRequireObject(loadedParam)->getBinding(loadedChild,
                NullableString::fromStaticLiteral("transform.t.x")));
        require(loadedBinding != nullptr && near(loadedBinding->getValue({2, 2}), 33.0f),
            RetainedString::fromStaticLiteral("node composite should reconnect binding to renamed moved child"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
} // namespace nijigenerate_tests::regression::detail
