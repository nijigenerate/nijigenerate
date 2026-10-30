#pragma once
#include <boost/json.hpp>
#include "settings_string.hpp"
#include "settings_value_metadata.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <bit>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace nijigenerate::core::settings {
class SettingsJsonError : public compat::ThrowableException {
public:
    explicit SettingsJsonError(compat::RetainedString message, int sourceLine = 0, int position = 0)
        : compat::ThrowableException(jsonMessage(std::move(message), sourceLine, position),
            compat::RetainedString::fromStaticLiteral("std/json.d"), sourceLine ? 2124 : 2126) {}
    explicit SettingsJsonError(std::string message, int sourceLine = 0, int position = 0)
        : SettingsJsonError(compat::RetainedString(std::move(message)), sourceLine, position) {}
    explicit SettingsJsonError(const char* message, int sourceLine = 0, int position = 0)
        : SettingsJsonError(compat::RetainedString(message), sourceLine, position) {}
    SettingsJsonError(compat::RetainedString message, compat::RetainedString sourceFile,
        std::size_t sourceLine)
        : compat::ThrowableException(std::move(message), std::move(sourceFile), sourceLine) {}
    template<std::size_t N> static SettingsJsonError fromStaticLiteral(const char (&message)[N],
        int sourceLine = 0, int position = 0) {
        return SettingsJsonError(compat::RetainedString::fromStaticLiteral(message), sourceLine, position);
    }
    std::string_view ngThrowableTypeName() const noexcept override { return "std.json.JSONException"; }
    // The actual Exception.msg descriptor retains counted bytes, including NUL.
    std::string_view messageView() const noexcept { return msg.view(); }
private:
    static compat::RetainedString jsonMessage(compat::RetainedString message, int sourceLine, int position) {
        if (!sourceLine) return message;
        const auto bytes = message.view();
        try {
            std::string result(bytes);
            result += " (Line ";
            result += std::to_string(sourceLine);
            result += ":";
            result += std::to_string(position);
            result += ")";
            return compat::RetainedString(std::move(result));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
};
class SettingsJsonValue;
struct SettingsJsonMember;
using SettingsJsonArray = compat::Slice<SettingsJsonValue>;

// Source std.json.JSONType. Null and boolean values have no Store member.
enum class SettingsJsonType : std::int8_t {
    Null, String, Integer, UInteger, Floating, Array, Object, True, False
};

namespace detail {
struct SettingsJsonKeyHash {
    std::size_t operator()(const SettingsString& key) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
    }
};
struct SettingsJsonKeyEqual {
    bool operator()(const SettingsString& left, const SettingsString& right) const noexcept {
        return left.view() == right.view();
    }
};
using SettingsJsonMembers = nicxlive::core::common::ResourceKeyMap<SettingsString, SettingsJsonMember,
    SettingsJsonKeyHash, SettingsJsonKeyEqual>;
}

// A copied initialized object retains its entries; copied null objects allocate
// independently on first insertion, as D associative-array descriptors do.
class SettingsJsonObject {
public:
    template<bool Constant> class Iterator {
        friend class SettingsJsonObject;
        using MapIterator = std::conditional_t<Constant,
            detail::SettingsJsonMembers::const_iterator, detail::SettingsJsonMembers::iterator>;
        using Member = std::conditional_t<Constant, const SettingsJsonMember, SettingsJsonMember>;
        MapIterator unordered_;
        Member* ordered_ = nullptr;
        bool orderedMode_ = false;
        explicit Iterator(MapIterator value) : unordered_(std::move(value)) {}
        explicit Iterator(Member* value) : ordered_(value), orderedMode_(true) {}
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SettingsJsonMember;
        using difference_type = std::ptrdiff_t;
        using reference = Member&;
        using pointer = Member*;
        Iterator() = default;
        reference operator*() const;
        pointer operator->() const;
        Iterator& operator++();
        Iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        friend bool operator==(const Iterator& left, const Iterator& right) {
            if (left.orderedMode_ != right.orderedMode_) return false;
            return left.orderedMode_ ? left.ordered_ == right.ordered_ : left.unordered_ == right.unordered_;
        }
    };
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    SettingsJsonObject() noexcept = default;
    explicit SettingsJsonObject(bool ordered) noexcept : ordered_(ordered) {}
    SettingsJsonObject(const SettingsJsonObject&) = default;
    // Native transport preserves the source AA/ordered-array descriptor.
    SettingsJsonObject(SettingsJsonObject&& other)
        : storage_(other.storage_), orderedMembers_(other.orderedMembers_), ordered_(other.ordered_) {}
    SettingsJsonObject& operator=(const SettingsJsonObject& other) {
        if (this == &other) return *this;
        // The source may be a child inside the destination's current object.
        // Retain every descriptor before releasing any destination backing.
        const auto storage = other.storage_;
        const auto members = other.orderedMembers_;
        const bool ordered = other.ordered_;
        storage_ = storage;
        orderedMembers_ = members;
        ordered_ = ordered;
        return *this;
    }
    SettingsJsonObject& operator=(SettingsJsonObject&& other) {
        return *this = static_cast<const SettingsJsonObject&>(other);
    }
    bool isNull() const noexcept;
    bool isOrdered() const noexcept { return ordered_; }
    std::size_t size() const noexcept;
    bool empty() const noexcept { return size() == 0; }
    SettingsJsonValue& operator[](SettingsString key);
    SettingsJsonValue& at(std::string_view key);
    const SettingsJsonValue& at(std::string_view key) const;
    SettingsJsonValue* if_contains(std::string_view key) noexcept;
    const SettingsJsonValue* if_contains(std::string_view key) const noexcept;
    bool erase(std::string_view key);
    void appendOrderedMember(SettingsString key, SettingsJsonValue value);
    compat::Slice<SettingsJsonMember>& orderedMembers();
    const compat::Slice<SettingsJsonMember>& orderedMembers() const;
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const void* identity() const noexcept;
    SettingsJsonObject duplicate() const;
private:
    friend class SettingsJsonValue;
    struct AssignmentTarget;
    AssignmentTarget sourceAssignment(SettingsString key);
    struct Storage;
    std::shared_ptr<Storage> storage_;
    compat::Slice<SettingsJsonMember> orderedMembers_;
    bool ordered_ = false;
};

// Value copies preserve immutable strings and managed array/object descriptors.
// Boost JSON is an explicit input bridge, not the owner of these shared handles.
class SettingsJsonValue {
public:
    SettingsJsonValue() noexcept = default;
    SettingsJsonValue(const SettingsJsonValue&) = default;
    SettingsJsonValue(SettingsJsonValue&& other) : store_(other.store_), type_(other.type_) {}
    SettingsJsonValue& operator=(const SettingsJsonValue& other) {
        if (this != &other) {
            // Snapshot an aliased child before replacing the parent's storage.
            const auto retained = other.store_;
            const auto type = other.type_;
            type_ = type;
            store_ = retained;
        }
        return *this;
    }
    SettingsJsonValue& operator=(SettingsJsonValue&& other) {
        return *this = static_cast<const SettingsJsonValue&>(other);
    }
    SettingsJsonValue(std::nullptr_t) noexcept {}
    SettingsJsonValue(bool value) { *this = value; }
    template<class T> requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
    SettingsJsonValue(T value) { *this = value; }
    template<class T> requires(std::is_floating_point_v<T>)
    SettingsJsonValue(T value) { *this = value; }
    SettingsJsonValue(SettingsString value) { *this = std::move(value); }
    SettingsJsonValue(const nicxlive::core::common::NullableString& value)
        : SettingsJsonValue(SettingsString(value)) {}
    SettingsJsonValue(std::string value) : SettingsJsonValue(SettingsString(std::move(value))) {}
    SettingsJsonValue(const char* value) : SettingsJsonValue(SettingsString(value)) {}
    SettingsJsonValue(SettingsJsonArray value) { *this = std::move(value); }
    SettingsJsonValue(SettingsJsonObject value) { *this = std::move(value); }
    SettingsJsonValue(const boost::json::value& value);
    SettingsJsonValue(const boost::json::array& value);
    SettingsJsonValue(const boost::json::object& value);

    SettingsJsonValue& operator=(std::nullptr_t) noexcept {
        type_ = SettingsJsonType::Null;
        return *this;
    }
    SettingsJsonValue& operator=(bool value) noexcept {
        type_ = value ? SettingsJsonType::True : SettingsJsonType::False;
        return *this;
    }
    template<class T> requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
    SettingsJsonValue& operator=(T value) {
        if constexpr (std::is_unsigned_v<T>) {
            type_ = SettingsJsonType::UInteger;
            store_ = static_cast<std::uint64_t>(value);
        } else {
            type_ = SettingsJsonType::Integer;
            store_ = std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(value));
        }
        return *this;
    }
    template<class T> requires(std::is_floating_point_v<T>)
    SettingsJsonValue& operator=(T value) {
        type_ = SettingsJsonType::Floating;
        store_ = std::bit_cast<std::uint64_t>(static_cast<double>(value));
        return *this;
    }
    SettingsJsonValue& operator=(SettingsString value) {
        type_ = SettingsJsonType::String;
        store_ = std::move(value);
        return *this;
    }
    SettingsJsonValue& operator=(std::string value) {
        type_ = SettingsJsonType::String;
        store_ = SettingsString(std::move(value));
        return *this;
    }
    SettingsJsonValue& operator=(const char* value) {
        type_ = SettingsJsonType::String;
        store_ = SettingsString(value);
        return *this;
    }
    SettingsJsonValue& operator=(const nicxlive::core::common::NullableString& value) {
        type_ = SettingsJsonType::String;
        store_ = SettingsString(value);
        return *this;
    }
    SettingsJsonValue& operator=(SettingsJsonArray value) {
        type_ = SettingsJsonType::Array;
        store_ = std::move(value);
        return *this;
    }
    SettingsJsonValue& operator=(SettingsJsonObject value) {
        type_ = SettingsJsonType::Object;
        store_ = std::move(value);
        return *this;
    }
    // The caller selects the source assign branch before invoking conversion.
    // Reentrant code may change type_ again; the final Store write does not undo it.
    template<SettingsJsonType type, class Construct>
        requires(type == SettingsJsonType::String || type == SettingsJsonType::Integer ||
            type == SettingsJsonType::UInteger || type == SettingsJsonType::Floating ||
            type == SettingsJsonType::Array || type == SettingsJsonType::Object)
    void sourceAssignConverted(Construct&& construct) {
        type_ = type;
        const auto converted = std::forward<Construct>(construct)();
        const auto retained = converted.store_;
        store_ = retained;
    }
    SettingsJsonType sourceType() const noexcept { return type_; }
    bool is_null() const noexcept { return type_ == SettingsJsonType::Null; }
    bool is_bool() const noexcept {
        return type_ == SettingsJsonType::True || type_ == SettingsJsonType::False;
    }
    bool is_int64() const noexcept { return type_ == SettingsJsonType::Integer; }
    bool is_uint64() const noexcept { return type_ == SettingsJsonType::UInteger; }
    bool is_double() const noexcept { return type_ == SettingsJsonType::Floating; }
    bool is_string() const noexcept { return type_ == SettingsJsonType::String; }
    bool is_array() const noexcept { return type_ == SettingsJsonType::Array; }
    bool is_object() const noexcept { return type_ == SettingsJsonType::Object; }
    bool as_bool() const;
    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    double as_double() const;
    SettingsString& as_string();
    const SettingsString& as_string() const;
    SettingsJsonArray& as_array();
    const SettingsJsonArray& as_array() const;
    SettingsJsonObject& as_object();
    const SettingsJsonObject& as_object() const;
    // Explicit Phobos source operations; native as_object/at keep their own API.
    SettingsJsonObject& sourceObjectRef();
    const SettingsJsonObject& sourceObjectRef() const;
    SettingsJsonObject sourceObjectNoRef() const;
    // Copy of the reference-array getter, and the distinct Phobos arrayNoRef.
    SettingsJsonArray sourceArrayValue() const;
    SettingsJsonArray sourceArrayNoRef() const;
    // The caller keeps this object descriptor alive through the reference use.
    SettingsJsonValue& sourceIndexReference(std::string_view key, SettingsJsonObject& retainedObject);
    SettingsJsonValue sourceIndexValue(std::string_view key) const;
    SettingsJsonValue sourceIndexValue(std::size_t index) const;
    SettingsJsonValue& sourceIndexAssignSlot(SettingsString key);
    template<class Assign, class Construct>
    void sourceIndexAssign(SettingsString key, Assign&& assign, Construct&& construct);
    friend bool operator==(const SettingsJsonValue& left, const SettingsJsonValue& right);
private:
    void sourceCheckIndexAssignment() const;
    // One actual payload, independent of the Source type tag. The numeric Store
    // members share their exact 64-bit lane; descriptors retain their real owners.
    // Its default is Source Store.str.init, even when type_ is null.
    std::variant<SettingsString, std::uint64_t, SettingsJsonArray, SettingsJsonObject> store_;
    SettingsJsonType type_ = SettingsJsonType::Null;
};

struct SettingsJsonMember {
    SettingsString name;
    SettingsJsonValue item;
    std::string_view key() const noexcept { return name.view(); }
    SettingsJsonValue& value() noexcept { return item; }
    const SettingsJsonValue& value() const noexcept { return item; }
};

// Retain the actual AA cell across a value conversion that erases/replaces it.
struct SettingsJsonObject::AssignmentTarget {
    detail::SettingsJsonMembers::iterator owner;
    SettingsJsonValue* value;
};

template<class Assign, class Construct>
void SettingsJsonValue::sourceIndexAssign(SettingsString key, Assign&& assign, Construct&& construct) {
    sourceCheckIndexAssignment();
    if (is_object() && std::get<SettingsJsonObject>(store_).isOrdered()) {
        auto object = std::get<SettingsJsonObject>(store_);
        for (auto& member : object) if (member.name == key) {
            std::forward<Assign>(assign)(member.item);
            return;
        }
        auto value = std::forward<Construct>(construct)();
        object.appendOrderedMember(std::move(key), std::move(value));
        *this = std::move(object);
    } else {
        SettingsJsonObject object;
        if (is_object()) object = sourceObjectNoRef();
        const auto target = object.sourceAssignment(std::move(key));
        std::forward<Assign>(assign)(*target.value);
        *this = std::move(object);
    }
}

template<bool Constant>
typename SettingsJsonObject::Iterator<Constant>::reference SettingsJsonObject::Iterator<Constant>::operator*() const {
    return orderedMode_ ? *ordered_ : unordered_->second;
}
template<bool Constant>
typename SettingsJsonObject::Iterator<Constant>::pointer SettingsJsonObject::Iterator<Constant>::operator->() const {
    return std::addressof(operator*());
}
template<bool Constant>
SettingsJsonObject::Iterator<Constant>& SettingsJsonObject::Iterator<Constant>::operator++() {
    if (orderedMode_) ++ordered_;
    else ++unordered_;
    return *this;
}
} // namespace nijigenerate::core::settings

