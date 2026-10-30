#pragma once

#include <nijigenerate/commands/detail/map_selection.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::detail {

// Specialize from the real source key declaration. An absent specialization
// must fail compilation, rather than guess a type/member name or enum range.
template<class Key> struct CommandMapKeyMetadata;

// The source contract's real non-enum case is D int, mapped by D39 to int32.
template<> struct CommandMapKeyMetadata<std::int32_t> {
    inline static constexpr std::string_view typeName = "int";
};

template<class Key> struct CommandEnumMember {
    Key value;
    std::string_view name;
};

// Keep a reference to the actual variable: an initializer may replace its AA
// descriptor, and validation must observe that current value, not a snapshot.
template<class Map> void ngValidateCommandMap(Map& map) {
    using Key = KeyTypeOfAA<Map>;
    using Metadata = CommandMapKeyMetadata<Key>;
    if constexpr (std::is_enum_v<Key>) {
        // This is key presence only: an existing null command is valid here.
        const auto validate = [&]<std::size_t Index>() {
            if (map.contains(Metadata::members[Index].value)) return;
            static constexpr auto message = [] {
                constexpr std::string_view prefix = "Command was not registered: ";
                constexpr std::string_view typeName = Metadata::typeName;
                constexpr std::string_view memberName = Metadata::members[Index].name;
                std::array<char, prefix.size() + typeName.size() + 1 + memberName.size()> bytes{};
                std::size_t offset = 0;
                for (const auto part : std::array{prefix, typeName, std::string_view("."), memberName})
                    for (const char character : part) bytes[offset++] = character;
                return bytes;
            }();
            // Phobos enforce duplicates its nonempty message with .idup.
            compat::RetainedString copiedMessage;
            try {
                copiedMessage = compat::RetainedString(std::string_view(message.data(), message.size()));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
            throw compat::ThrowableException(
                copiedMessage,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"), 158);
        };
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (validate.template operator()<Index>(), ...);
        }(std::make_index_sequence<Metadata::members.size()>{});
    } else {
        if (map.size() == 0) {
            static constexpr auto message = [] {
                constexpr std::string_view prefix = "Command map is empty: ";
                constexpr std::string_view typeName = Metadata::typeName;
                std::array<char, prefix.size() + typeName.size()> bytes{};
                std::size_t offset = 0;
                for (const auto part : std::array{prefix, typeName})
                    for (const char character : part) bytes[offset++] = character;
                return bytes;
            }();
            compat::RetainedString copiedMessage;
            try {
                copiedMessage = compat::RetainedString(std::string_view(message.data(), message.size()));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
            throw compat::ThrowableException(
                copiedMessage,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"), 161);
        }
    }
}

template<class Binding> void ngValidateCommandMap() {
    static_assert(ExistingMapBinding<Binding>, "Command map validation requires existing storage");
    ngValidateCommandMap(Binding::get());
}

} // namespace nijigenerate::commands::detail
