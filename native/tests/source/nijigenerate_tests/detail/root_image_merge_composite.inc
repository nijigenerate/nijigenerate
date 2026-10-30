// Faithful body of regression.d12272-12314; include once outside namespaces.
#include "root_image_merge_composite.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
void writeRegressionPng(nijigenerate::compat::RetainedString path, std::uint8_t r, std::uint8_t g,
    std::uint8_t b, std::int32_t width, std::int32_t height, std::uint8_t a);
std::shared_ptr<nicxlive::core::nodes::Part> findDirectPart(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name);
std::shared_ptr<nicxlive::core::param::Parameter> findParameter(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name);
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero);

void testProjectImageMergeExportCompositeWorkflow() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    resetCase();

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-image-merge-export-composite")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array baseSegments{fixtureDir, RetainedString::fromStaticLiteral("base-layer.png")};
        const auto baseImage = ngBuildPath(baseSegments);
        const std::array mergeSegments{fixtureDir, RetainedString::fromStaticLiteral("merge-layer.png")};
        const auto mergeImage = ngBuildPath(mergeSegments);
        writeRegressionPng(baseImage, 200, 50, 50, 3, 3, 255);
        writeRegressionPng(mergeImage, 50, 200, 50, 4, 2, 255);
        require(constructSourceCommand<ImportImageFolderCommand>(fixtureDir)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("project image composite should import image folder"));
        require(findDirectPart(nijigenerate::project::incActivePuppet(), "base-layer") != nullptr,
            RetainedString::fromStaticLiteral("project image composite should import base layer"));

        require(constructSourceCommand<MergeImageFilesCommand>(mergeImage)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("project image composite should merge explicit image"));
        auto merged = findDirectPart(nijigenerate::project::incActivePuppet(), "merge-layer.png");
        require(merged != nullptr,
            RetainedString::fromStaticLiteral("project image composite should add merged image layer"));

        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("ImageMergeParam"); }, [] { return false; });
        param->min = {0, 0};
        param->max = {1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        auto binding = newValueBinding(param, merged, RetainedString::fromStaticLiteral("transform.t.x"), true);
        binding->setValue({1, 0}, 21.0f);

        const std::array inpSegments{fixtureDir, RetainedString::fromStaticLiteral("image-composite.inp")};
        const auto inpPath = ngBuildPath(inpSegments);
        // Use the existing typed Source memory writer and canonical std.file.write,
        // retaining original inWriteINPPuppet's memory-before-file operation order.
        auto puppet = nijigenerate::project::incActivePuppet();
        const auto bytes = nijigenerate::io::ngWriteProjectPuppetMemory(
            *nijigenerate::compat::ngRequireObject(puppet));
        ngFileWrite(RetainedStringSlice(inpPath),
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        resetCase();
        require(constructSourceCommand<ImportINPCommand>(inpPath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("project image composite should import exported INP"));
        auto loadedMerged = findDirectPart(nijigenerate::project::incActivePuppet(), "merge-layer.png");
        auto loadedParam = findParameter(nijigenerate::project::incActivePuppet(), "ImageMergeParam");
        require(loadedMerged != nullptr && loadedParam != nullptr, RetainedString::fromStaticLiteral(
            "project image composite should restore merged layer and parameter from INP"));
        require(loadedParam->bindings.size() == 1 &&
            loadedParam->bindings[0]->getTarget().target.resource() ==
                std::static_pointer_cast<nicxlive::core::Resource>(loadedMerged),
            RetainedString::fromStaticLiteral("project image composite should reconnect imported binding target"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
} // namespace nijigenerate_tests::regression::detail
