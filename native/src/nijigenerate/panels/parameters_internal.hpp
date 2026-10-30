#pragma once
#include "parameters.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/binding/binding.hpp>
#include <nijigenerate/commands/parameter/paramedit.hpp>
#include <nijigenerate/commands/parameter/param.hpp>
#include <nijigenerate/commands/parameter/group.hpp>
#include <nijigenerate/commands/parameter/animedit_commands.hpp>
#include <nijigenerate/commands/parameter/base.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/commands/concrete_catalog.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <common/nullable_string.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>
#include <type_traits>
#include <new>
#include <stdexcept>
#include <string>
namespace nijigenerate::panels::parameters::detail {
using Context=commands::Context;
using ContextRef=std::shared_ptr<Context>;

// Only passive byte/backing operations; providers and callbacks remain outside.
template<class Operation> auto allocateParameterStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

inline compat::SourceCString parameterNameCString(const nicxlive::core::common::NullableString& input) {
    const compat::RetainedString retained(input);
    compat::SourceCString result{compat::RetainedStringSlice(retained)};
    compat::detail::ngSourceCStringPostconditions(input.view(), result.data());
    return result;
}
template<class T> compat::Slice<T> retainedSlice(nicxlive::core::common::SharedArray<T> values) {
    if(values.isNull())return {};
    auto owner=values.retentionOwner();
    auto data=values.data();auto length=values.size();
    // Ownerless nonempty SharedArray views have its explicit staticView contract.
    if(!owner)return compat::Slice<T>::fromStaticBuffer(data,length);
    return compat::Slice<T>::retainedView(std::move(owner),data,length);
}
inline ContextRef context(ParameterRef parameter) {
    auto result=std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    result->puppet(project::incActivePuppet());
    result->parameters(Context::Parameters{parameter});return result;
}
template<class Table,class Id,class Assign>
void run(Table& table,Id id,ContextRef ctx,Assign assign) {
    auto found=table.find(id);
    const auto sourceId = [id] {
        const auto typeName=commands::ngCommandKeyTypeName<Id>();
        const auto keyName=commands::ngCommandKeyName(id);
        return allocateParameterStorage([&] { return std::string(typeName) + "." + keyName; });
    };
    if(found==table.end()) {
        const auto idName=sourceId();const auto keyType=commands::ngCommandKeyTypeName<Id>();
        auto message=allocateParameterStorage([&] { return compat::RetainedString(
            "No registered command for id: " + idName + " (key type: " + std::string(keyType) + ")"); });
        throw compat::ThrowableException(std::move(message),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"),358);
    }
    auto command=found->second;
    if(!commands::ngCommandAllowedInCurrentContext(command))return;
    auto invoke = [&]<class C>(auto, std::shared_ptr<commands::Command> value, const std::string&) {
        if constexpr (!std::is_void_v<C>) {
            auto concrete = std::dynamic_pointer_cast<C>(value);
            if(!concrete) {
                const auto idName=sourceId();const auto keyName=commands::ngCommandKeyName(id);
                auto message=allocateParameterStorage([&] { return compat::RetainedString(
                    "Registered command instance type mismatch for id: " + idName +
                    ", expected " + keyName + "Command"); });
                throw compat::ThrowableException(std::move(message),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"),376);
            }
            assign(command);
            commands::ngRunCommand(concrete,ctx);
        } else {
            throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(
                "No concrete command class for parameter UI command"));
        }
    };
    commands::ngVisitConcreteCommand(id,command,invoke);
}
template<class Table,class Id> void run(Table& table,Id id,ContextRef ctx) {
    run(table,id,std::move(ctx),[](const auto&){});
}
template<class Function> bool queue(Function action) {
    return queueParameterUiCommand(ParameterUiCommand(std::move(action)));
}
template<class Table,class Id> bool item(const char* title,Table& table,Id id,ContextRef ctx) {
    if(!ImGui::MenuItem(core::translation::ngTranslateCString(title).data(),"",false,true))return false;
    return queue([&table,id,ctx]{run(table,id,ctx);});
}
template<class Table,class Id> bool rawItem(const char* title,Table& table,Id id,ContextRef ctx) {
    if(!ImGui::MenuItem(title,"",false,true))return false;
    return queue([&table,id,ctx]{run(table,id,ctx);});
}
inline BindingList values(const commands::binding::SelectedBindings& selected) {
    BindingList result;for(const auto& entry:selected)result.append(entry.second);return result;
}
inline void bindingContext(ContextRef ctx,ParameterRef param) {
    ctx->puppet(project::incActivePuppet());ctx->parameters(Context::Parameters{param});
    if(project::incArmedParameter())ctx->armedParameters(Context::Parameters{project::incArmedParameter()});
    ctx->keyPoint(commands::binding::cParamPoint);
}
}
