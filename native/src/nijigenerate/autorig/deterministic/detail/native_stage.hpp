#pragma once
#include "../native_stage.hpp"
#include "evidence.hpp"
#include "sha256.hpp"
#include "native_memory.hpp"
#include "native_maps.hpp"
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <serde_json.hpp>
namespace nijigenerate::autorig::deterministic::native_detail {
template<class Action, class Context> auto commandSource(Action&& makeAction, Context&& makeContext) {
    auto action = std::forward<Action>(makeAction)();
    auto context = std::forward<Context>(makeContext)();
    return command(std::move(action), std::move(context));
}
template<class Action, class Context> auto createSourceNode(Action&& makeAction, Context&& makeContext) {
    auto action = std::forward<Action>(makeAction)();
    auto context = std::forward<Context>(makeContext)();
    return create(std::move(action), std::move(context));
}
inline framework::Text nativeString(const Json& value) { return value.as_string().retainedString(); }
inline Json indexNative(const Json& value,std::string_view key) { return value.sourceIndexValue(key); }
template<class Values> auto readNativeArray(const Values& values,std::size_t index,std::size_t line) {
    if(index>=values.size())throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),line);
    return values[index];
}
template<class Key> void assignNative(Json& value,Key&& key,Json entry) {
    core::settings::SettingsString retainedKey;
    if constexpr(std::is_array_v<std::remove_reference_t<Key>>)
        retainedKey=compat::RetainedString::fromStaticLiteral(key);
    else retainedKey=core::settings::SettingsString(std::forward<Key>(key));
    value.sourceIndexAssignSlot(std::move(retainedKey))=std::move(entry);
}
inline void enforceNative(bool condition,compat::RetainedString message,std::size_t line) {
    if(!condition)throw compat::ThrowableException(copyNativeMessage(std::move(message)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),line);
}
template<std::size_t N> void enforceNative(bool condition,const char (&message)[N],std::size_t line) {
    if(!condition)enforceNative(false,compat::RetainedString::fromStaticLiteral(message),line);
}
template<class Message> void enforceNativeLazy(bool condition,Message message,std::size_t line) {
    if(!condition)enforceNative(false,message(),line);
}
inline core::settings::SettingsString bytesDigest(std::span<const std::uint8_t> bytes) {
    contracts::detail::Sha256 hash;hash.put(bytes);const auto digest=hash.finish();
    auto result=allocateNativeText(digest.size()*2,'\xff');constexpr char hex[]="0123456789ABCDEF";
    std::size_t offset=0;for(auto b:digest){result[offset++]=hex[b>>4];result[offset++]=hex[b&15];}
    return core::settings::SettingsString(retainNativeText(std::move(result)));
}
template<class T> Json serializedJson(const T& value) {
    auto writer=createNativeObject<nicxlive::core::serde::TypedJsonWriter>();
    writer->serializeValue(value);writer->flush();
    const auto& encoded=writer->str();
    return json::ngParseAutoRigJson(compat::RetainedString::retainedView(writer,encoded.data(),encoded.size()));
}
template<class T> std::span<const std::uint8_t> rawBytes(std::span<const T> cells) {
    return {reinterpret_cast<const std::uint8_t*>(cells.data()),cells.size_bytes()};
}
}
