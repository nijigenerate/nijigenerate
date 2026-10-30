#pragma once
#include "file_helpers.hpp"
#include "file_allocation.hpp"
#include <nijigenerate/compat/exception_message.hpp>
#include <exception>
#include "file_commands.hpp"
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <common/shared_array.hpp>
namespace nijigenerate::commands::puppet::file::detail {
using String=compat::RetainedString;
template<std::size_t N> String tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(String::fromStaticLiteral(value));
}
template<class... T> String format(std::string_view pattern,T&&...values) {
    const std::array args{core::translation::ngFormatArgument(std::forward<T>(values))...};
    auto text = core::translation::ngFormatTyped(pattern,args);
    return fileAllocation([&] { return String(std::move(text)); });
}
String extension(String path);
String withExtension(String path,std::string_view extension);
compat::Slice<core::settings::SettingsString> splitPaths(String value);
void showError(String message);
extern double ngHeadlessVideoClock;
double ngHeadlessVideoNow();
std::shared_ptr<LoadResult<std::shared_ptr<nicxlive::core::Puppet>>> ngLoadedActivePuppet(String message);
std::shared_ptr<ext::nodes::ExCamera> ngResolveExportCamera(String name,String& error);
bool ngRenderCamera(std::shared_ptr<ext::nodes::ExCamera> camera,bool transparency,bool postprocessing,
    nicxlive::core::common::SharedArray<std::uint8_t>& data,int& width,int& height);
void ngMergeImportedPuppet(std::shared_ptr<ext::ExPuppet> source,bool mergeParameters=true,bool mergeAnimations=true);
void ngMergePSDDefault(String path,bool renameMapped,bool retranslateMapped);
void ngMergeKRADefault(String path,bool renameMapped,bool retranslateMapped);
int asInt(double value);
}
