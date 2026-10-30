// Faithful body of regression.d12029-12070; include once outside namespaces.
#include "root_simplephysics_composite.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/simplephysics_complete.hpp>
#include <nijigenerate/commands/node/simplephysics_parameter_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_config_commands.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <param/parameter.hpp>
#include <array>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nicxlive::core::nodes::Node> findNodeRecursive(
    std::shared_ptr<nicxlive::core::nodes::Node> root, std::string_view name);

void testSimplePhysicsCompositeRoundTripWorkflow() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::node::simplephysics;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::SourcePhysicsModel;
    using nicxlive::core::nodes::SourceParamMapMode;
    using nicxlive::core::param::Parameter;
    resetCase();

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-simplephysics-composite")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto physics = SimplePhysics::createSourceDeferred([] {
            return nijigenerate::project::incActivePuppet()->root;
        });
        physics->setName(NullableString::fromStaticLiteral("simplephysics-composite"));
        auto param = Parameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("SimplePhysicsCompositeParam"); },
            [] { return false; });
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(physics)});

        require(constructSourceCommand<SetSimplePhysicsParameterCommand>(param)->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should assign parameter"));
        require(constructSourceCommand<SetSimplePhysicsModelTypeCommand>(
            SourcePhysicsModel{SourcePhysicsModel::SpringPendulum})->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should set model type"));
        require(constructSourceCommand<SetSimplePhysicsMapModeCommand>(
            SourceParamMapMode{SourceParamMapMode::XY})->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should set map mode"));
        require(constructSourceCommand<SetSimplePhysicsLocalOnlyCommand>(true)->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should set localOnly"));
        require(constructSourceCommand<SetSimplePhysicsGravityCommand>(2.5f)->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should set gravity"));
        nijigenerate::core::actionstack::incActionUndo();
        require(near(physics->getGravity(), 1.0f), RetainedString::fromStaticLiteral(
            "simplephysics composite undo should restore previous gravity without undoing earlier settings"));
        nijigenerate::core::actionstack::incActionRedo();
        require(near(physics->getGravity(), 2.5f),
            RetainedString::fromStaticLiteral("simplephysics composite redo should restore gravity"));

        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("simplephysics-composite")};
        const auto saveBase = ngBuildPath(saveSegments);
        const auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        require(constructSourceCommand<SaveFileCommand>(saveBase)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should save"));
        nijigenerate::project::incNewProject();
        require(constructSourceCommand<OpenFileCommand>(savePath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("simplephysics composite should reopen"));
        auto loaded = std::dynamic_pointer_cast<SimplePhysics>(findNodeRecursive(
            nijigenerate::project::incActivePuppet()->root, "simplephysics-composite"));
        require(loaded != nullptr,
            RetainedString::fromStaticLiteral("simplephysics composite should preserve node type"));
        require(loaded->getParam() != nullptr &&
            loaded->getParam()->getName().view() == "SimplePhysicsCompositeParam",
            RetainedString::fromStaticLiteral("simplephysics composite should reconnect parameter"));
        require(loaded->getModelType() == SourcePhysicsModel::SpringPendulum &&
            loaded->mapMode == SourceParamMapMode::XY && loaded->localOnly,
            RetainedString::fromStaticLiteral("simplephysics composite should preserve command-edited modes"));
        require(near(loaded->getGravity(), 2.5f),
            RetainedString::fromStaticLiteral("simplephysics composite should preserve redone gravity"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
} // namespace nijigenerate_tests::regression::detail
