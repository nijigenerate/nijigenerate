#include "config.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/psd/retained_file.hpp>
#include <array>
#include <cstdio>
#include <cerrno>
#include <type_traits>
#include <utility>
namespace nijigenerate::commands::automesh::config {
namespace {
template<std::size_t N> compat::RetainedString sourceArgument(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<class T> requires (!std::is_array_v<std::remove_reference_t<T>>)
decltype(auto) sourceArgument(T&& value) { return std::forward<T>(value); }
template<class Table,class Key,class C,class... A>
    requires(ngRuntimeCommandRegistrationSupported<C, Table, Key,
        decltype(sourceArgument(std::declval<A&&>()))...>())
void put(Table& table,Key key,A&&...args) {
    auto command=ngCreateRegisteredCommand<C>(sourceArgument(std::forward<A>(args))...);
    table[key]=command;
}
}
template<> void ngInitCommands<AutoMeshConfigCommand>() {
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshListProcessors,AutoMeshListProcessorsCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshGetActive,AutoMeshGetActiveCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshSetActive,AutoMeshSetActiveCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshApplyActive,AutoMeshApplyActiveCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshGetSchema,AutoMeshGetSchemaCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshGetValues,AutoMeshGetValuesCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshSetPreset,AutoMeshSetPresetCommand>(commands);
    ngRegisterCommandIfSupported<AutoMeshConfigCommand::AutoMeshSetValues,AutoMeshSetValuesCommand>(commands);
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshListProcessorsCommand>(commands,AutoMeshConfigCommand::AutoMeshListProcessors);
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshGetActiveCommand>(commands,AutoMeshConfigCommand::AutoMeshGetActive);
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshSetActiveCommand>(commands,AutoMeshConfigCommand::AutoMeshSetActive,"");
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshApplyActiveCommand>(commands,AutoMeshConfigCommand::AutoMeshApplyActive);
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshGetSchemaCommand>(commands,AutoMeshConfigCommand::AutoMeshGetSchema,"");
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshGetValuesCommand>(commands,AutoMeshConfigCommand::AutoMeshGetValues,"","Simple");
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshSetPresetCommand>(commands,AutoMeshConfigCommand::AutoMeshSetPreset,"","");
    put<decltype(commands),AutoMeshConfigCommand,AutoMeshSetValuesCommand>(commands,AutoMeshConfigCommand::AutoMeshSetValues,"","Simple","{}");
}
template<> void ngInitCommands<AutoMeshGetConfigKey>() {
    detail::initializeProcessors<AutoMeshGetConfigKey>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
template<> void ngInitCommands<AutoMeshSetConfigKey>() {
    detail::initializeProcessors<AutoMeshSetConfigKey>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
template<> void ngInitCommands<AutoMeshSetSimpleKey>() {
    detail::initializeProcessors<AutoMeshSetSimpleKey>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
template<> void ngInitCommands<AutoMeshSetAdvancedKey>() {
    detail::initializeProcessors<AutoMeshSetAdvancedKey>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
template<> void ngInitCommands<AutoMeshSetPresetKey>() {
    detail::initializeProcessors<AutoMeshSetPresetKey>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
std::shared_ptr<Command> ensureGetAutoMeshConfigCommand(compat::RetainedString id) {
    AutoMeshGetConfigKey key{id};
    if(auto found=autoMeshGetConfigCommands.find(key);found!=autoMeshGetConfigCommands.end()) return found->second;
    if(id.view()=="optimum") { put<decltype(autoMeshGetConfigCommands),AutoMeshGetConfigKey,GetAutoMeshConfig_OptimumAutoMeshProcessorCommand>(autoMeshGetConfigCommands,key,"Simple"); return autoMeshGetConfigCommands[key]; }
    if(id.view()=="contour") { put<decltype(autoMeshGetConfigCommands),AutoMeshGetConfigKey,GetAutoMeshConfig_ContourAutoMeshProcessorCommand>(autoMeshGetConfigCommands,key,"Simple"); return autoMeshGetConfigCommands[key]; }
    if(id.view()=="grid") { put<decltype(autoMeshGetConfigCommands),AutoMeshGetConfigKey,GetAutoMeshConfig_GridAutoMeshProcessorCommand>(autoMeshGetConfigCommands,key,"Simple"); return autoMeshGetConfigCommands[key]; }
    if(id.view()=="skeleton") { put<decltype(autoMeshGetConfigCommands),AutoMeshGetConfigKey,GetAutoMeshConfig_SkeletonExtractorCommand>(autoMeshGetConfigCommands,key,"Simple"); return autoMeshGetConfigCommands[key]; }
    return {};
}
std::shared_ptr<Command> ensureSetAutoMeshConfigCommand(compat::RetainedString id) {
    AutoMeshSetConfigKey key{id};
    if(auto found=autoMeshSetConfigCommands.find(key);found!=autoMeshSetConfigCommands.end()) return found->second;
    if(id.view()=="optimum") { put<decltype(autoMeshSetConfigCommands),AutoMeshSetConfigKey,SetAutoMeshConfig_OptimumAutoMeshProcessorCommand>(autoMeshSetConfigCommands,key,"Simple","{}"); return autoMeshSetConfigCommands[key]; }
    if(id.view()=="contour") { put<decltype(autoMeshSetConfigCommands),AutoMeshSetConfigKey,SetAutoMeshConfig_ContourAutoMeshProcessorCommand>(autoMeshSetConfigCommands,key,"Simple","{}"); return autoMeshSetConfigCommands[key]; }
    if(id.view()=="grid") { put<decltype(autoMeshSetConfigCommands),AutoMeshSetConfigKey,SetAutoMeshConfig_GridAutoMeshProcessorCommand>(autoMeshSetConfigCommands,key,"Simple","{}"); return autoMeshSetConfigCommands[key]; }
    if(id.view()=="skeleton") { put<decltype(autoMeshSetConfigCommands),AutoMeshSetConfigKey,SetAutoMeshConfig_SkeletonExtractorCommand>(autoMeshSetConfigCommands,key,"Simple","{}"); return autoMeshSetConfigCommands[key]; }
    return {};
}
template<> void ngInitCommands<AutoMeshTypedCommand>() {
    const auto before=autoMeshTypedCommands.size();
    detail::initializeProcessors<AutoMeshTypedCommand>(viewport::vertex::automesh::AutoMeshProcessorTypes{});
#ifdef CMD_LOG
    const std::array args{core::translation::ngFormatArgument(before),core::translation::ngFormatArgument(autoMeshTypedCommands.size())};
    auto output=core::translation::ngFormatTyped("[CMD] AutoMeshTypedCommand init: before=%s after=%s",args);
    output.push_back('\n');
    if (std::fwrite(output.data(),1,output.size(),stdout)!=output.size())
        throw io::psd::RetainedFileError(errno,"Could not write to file");
#else
    (void)before;
#endif
}
}
