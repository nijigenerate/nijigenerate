// Faithful body of regression.d12430-12466; include once outside namespaces.
#include "root_render_composite.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <texture.hpp>
#include <array>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nicxlive::core::nodes::Node> findNodeRecursive(
    std::shared_ptr<nicxlive::core::nodes::Node> root, std::string_view name);

void testRenderCameraTextureCompositeWorkflow() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::ext::nodes::ExCamera;
    using nicxlive::core::ShallowTexture;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    resetCase();

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-render-composite")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto part = newMeshPart(RetainedString::fromStaticLiteral("render-composite-part"));
        part->tint = {0.8f, 0.7f, 0.6f};
        nijigenerate::project::incActivePuppet()->populateTextureSlots();
        require(nijigenerate::project::incActivePuppet()->textureSlots.size() == 1,
            RetainedString::fromStaticLiteral("render composite should populate texture slot"));

        auto camera = ExCamera::createSourceDeferred([] {
            return nijigenerate::project::incActivePuppet()->root;
        });
        camera->setName(NullableString::fromStaticLiteral("render-composite-camera"));
        camera->setViewport({10, 8});
        nijigenerate::project::incActivePuppet()->root->build();
        const std::array exportSegments{fixtureDir, RetainedString::fromStaticLiteral("render-composite")};
        const auto exportBase = ngBuildPath(exportSegments);
        require(constructSourceCommand<ExportPNGCommand>(exportBase,
            RetainedString::fromStaticLiteral("render-composite-camera"), true, false)->run(
                std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("render composite should export camera PNG"));
        auto exported = ShallowTexture(concatenateSourceStrings({exportBase,
            RetainedString::fromStaticLiteral(".png")}), 4);
        require(exported.width == 10 && exported.height == 8,
            RetainedString::fromStaticLiteral("render composite export should use camera viewport"));

        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("render-composite-model")};
        const auto saveBase = ngBuildPath(saveSegments);
        const auto savePath = concatenateSourceStrings({saveBase,
            RetainedString::fromStaticLiteral(".inx")});
        require(constructSourceCommand<SaveFileCommand>(saveBase)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("render composite should save model"));
        nijigenerate::project::incNewProject();
        require(constructSourceCommand<OpenFileCommand>(savePath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("render composite should reopen model"));
        require(findNodeRecursive(nijigenerate::project::incActivePuppet()->root,
            "render-composite-camera") != nullptr,
            RetainedString::fromStaticLiteral("render composite should preserve camera after reload"));
        require(findNodeRecursive(nijigenerate::project::incActivePuppet()->root,
            "render-composite-part") != nullptr,
            RetainedString::fromStaticLiteral("render composite should preserve texture-backed part after reload"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
} // namespace nijigenerate_tests::regression::detail
