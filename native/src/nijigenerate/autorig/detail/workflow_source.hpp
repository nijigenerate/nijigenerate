#pragma once
#include "../workflow.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <functional>
#include <exception>
#include <type_traits>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::autorig::workflow::detail {
inline Text copiedMessage(std::string_view message) {
    try {
        return Text(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T, std::size_t N> void registerClass(const char (&name)[N]) {
    static const bool registered = [&] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T), Text::fromStaticLiteral(name));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
template<class T> std::size_t objectHash(T* object) noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(object));
    return address ^ (address >> 4);
}
template<class T> bool objectEquals(T* object, const std::shared_ptr<T>& other) {
    return other && dynamic_cast<const void*>(object) == dynamic_cast<const void*>(other.get());
}
[[noreturn]] inline int objectCompare(Text name) {
    Text message;
    try {
        std::string bytes("need opCmp for class ");
        bytes += name.view();
        message = copiedMessage(bytes);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message, Text::fromStaticLiteral("object.d"), 195);
}
inline Text text(compat::RetainedStringSlice value) { return value.retainedString(); }
inline Text text(Text value) { return value; }
inline Text sourceFile() {
    return Text::fromStaticLiteral("source/nijigenerate/autorig/workflow.d");
}
[[noreturn]] inline void fail(Text message,std::size_t line) {
    throw compat::ThrowableException(std::move(message),sourceFile(),line);
}
template<std::size_t N> void require(bool condition,const char (&message)[N],std::size_t line) {
    if(!condition) fail(copiedMessage(std::string_view(message,N-1)),line);
}
template<class Lazy> requires std::is_invocable_v<Lazy>
void require(bool condition,Lazy&& message,std::size_t line) {
    if(condition) return;
    Text copy;
    try {
        const Text evaluated(std::invoke(std::forward<Lazy>(message)));
        copy=copiedMessage(evaluated.view());
    } catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    fail(std::move(copy),line);
}
template<class Map,class Key> decltype(auto) aaAt(Map& map,const Key& key,std::size_t line) {
    const auto found=map.find(key);
    if(found==map.end()) throw compat::ngStaticError<compat::RangeError>(sourceFile(),line);
    return (found->second);
}
template<class Array> decltype(auto) arrayAt(Array& values,std::size_t index,std::size_t line) {
    if(index>=values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),sourceFile(),line);
    return values[index];
}
template<class J> decltype(auto) jsonObjectRef(J& value) {
    return value.sourceObjectRef();
}
template<class J> Json jsonIndex(const J& value,std::string_view key) {
    return value.sourceIndexValue(key);
}
inline Text jsonString(const Json& value) {
    return value.as_string().retainedString();
}
inline std::int64_t jsonInteger(const Json& value) {
    return value.as_int64();
}
inline auto jsonArray(const Json& value) {
    return value.sourceArrayValue();
}
// LDC's Windows scope(failure) transport retains the first Throwable and chains
// a collateral failure, except that a new Error bypasses an existing Exception.
template<class Cleanup> [[noreturn]] void failureCleanup(Cleanup cleanup) {
    const auto original=std::current_exception();
    try { std::rethrow_exception(original); }
    catch(compat::Throwable& first) {
        auto retained=::nicxlive::core::d_runtime::ngRetainCurrentThrowable(first);
        try { cleanup(); }
        catch(compat::Throwable& second) {
            auto collateral=::nicxlive::core::d_runtime::ngRetainCurrentThrowable(second);
            if(!dynamic_cast<compat::ThrowableError*>(&first)) {
                if(auto error=dynamic_cast<compat::ThrowableError*>(&second)) {
                    error->bypassedException=std::move(retained);
                    throw;
                }
            }
            compat::Throwable::chainTogether(std::move(retained),std::move(collateral));
        }
        std::rethrow_exception(original);
    }
    catch(...) { cleanup(); throw; }
}
[[noreturn]] inline void convError(std::string message,std::size_t line) {
    throw compat::PhobosConvException(Text(std::move(message)),Text::fromStaticLiteral("std/conv.d"),line);
}
inline AutoRigValueKind storedKind(std::string_view source) {
    constexpr std::pair<std::string_view,AutoRigValueKind> names[]{
        {"FileName",AutoRigValueKind::FileName},{"Path",AutoRigValueKind::Path},
        {"Json",AutoRigValueKind::Json},{"Blob",AutoRigValueKind::Blob}};
    for(const auto& [name,kind]:names) if(source.starts_with(name)) {
        const auto suffix=source.substr(name.size());
        if(suffix.empty()) return kind;
        // std.conv's successful parse is followed by convError on the first remaining scalar.
        const auto count=compat::phobos::ngUnexpectedScalarLength(suffix);
        const auto shown=suffix.front()=='\n'?std::string("\\n"):std::string(suffix.substr(0,count));
        convError("Unexpected '"+shown+"' when converting from type string to type AutoRigValueKind",2013);
    }
    convError("AutoRigValueKind does not have a member named '"+std::string(source)+"'",3049);
}
inline std::uint8_t storedByte(std::int64_t value) {
    if(value>=0 && value<=255) return static_cast<std::uint8_t>(value);
    throw compat::PhobosConvOverflowException(
        value<0?Text::fromStaticLiteral("Conversion negative overflow"):
            Text::fromStaticLiteral("Conversion positive overflow"),
        Text::fromStaticLiteral("std/conv.d"),value<0?1581:1587);
}
} // namespace nijigenerate::autorig::workflow::detail
