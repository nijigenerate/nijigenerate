#include "settings_value.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::core::settings {
namespace {
template<std::size_t N> SettingsJsonError getterError(const char (&message)[N], std::size_t line) {
    // Every caller lowers enforce!JSONException, whose non-null message is idup'd.
    compat::RetainedString copiedMessage;
    try {
        copiedMessage = compat::RetainedString(std::string_view(message, N - 1));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return SettingsJsonError(copiedMessage,
        compat::RetainedString::fromStaticLiteral("std/json.d"), line);
}
SettingsJsonValue& sourceIndexedMember(SettingsJsonObject& object, std::string_view key) {
    if (auto* entry = object.if_contains(key)) return *entry;
    compat::RetainedString message;
    try {
        const auto text = "Key not found: " + std::string(key);
        message = compat::RetainedString(std::string_view(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw SettingsJsonError(message,
        compat::RetainedString::fromStaticLiteral("std/json.d"), 806);
}
}
struct SettingsJsonObject::Storage { detail::SettingsJsonMembers members; };
SettingsJsonObject::AssignmentTarget SettingsJsonObject::sourceAssignment(SettingsString key) {
    if (!storage_) storage_ = std::allocate_shared<Storage>(
        nicxlive::core::d_runtime::SourceAllocator<Storage>{});
    [[maybe_unused]] const auto cell = storage_->members[key];
    auto entry = storage_->members.find(key);
    auto& member = entry->second;
    member.name = entry->first;
    return {std::move(entry), &member.item};
}
bool SettingsJsonObject::isNull() const noexcept { return ordered_ ? orderedMembers_.isNull() : !storage_; }
const void* SettingsJsonObject::identity() const noexcept {
    return ordered_ ? static_cast<const void*>(orderedMembers_.data()) : storage_.get();
}
SettingsJsonObject SettingsJsonObject::duplicate() const {
    SettingsJsonObject result;
    for (const auto& member : *this) result[member.name] = member.item;
    return result;
}
std::size_t SettingsJsonObject::size() const noexcept {
    return ordered_ ? orderedMembers_.size() : storage_ ? storage_->members.size() : 0;
}
SettingsJsonObject::iterator SettingsJsonObject::begin() noexcept {
    if (ordered_) return iterator(orderedMembers_.data());
    return storage_ ? iterator(storage_->members.begin()) : iterator{};
}
SettingsJsonObject::iterator SettingsJsonObject::end() noexcept {
    if (ordered_) return iterator(orderedMembers_.empty() ? orderedMembers_.data()
        : orderedMembers_.data() + orderedMembers_.size());
    return storage_ ? iterator(storage_->members.end()) : iterator{};
}
SettingsJsonObject::const_iterator SettingsJsonObject::begin() const noexcept {
    if (ordered_) return const_iterator(orderedMembers_.data());
    return storage_ ? const_iterator(std::as_const(storage_->members).begin()) : const_iterator{};
}
SettingsJsonObject::const_iterator SettingsJsonObject::end() const noexcept {
    if (ordered_) return const_iterator(orderedMembers_.empty() ? orderedMembers_.data()
        : orderedMembers_.data() + orderedMembers_.size());
    return storage_ ? const_iterator(std::as_const(storage_->members).end()) : const_iterator{};
}
SettingsJsonValue* SettingsJsonObject::if_contains(std::string_view key) noexcept {
    SettingsJsonValue* found = nullptr;
    for (auto& member : *this) if (member.key() == key) found = &member.item;
    return found;
}
const SettingsJsonValue* SettingsJsonObject::if_contains(std::string_view key) const noexcept {
    const SettingsJsonValue* found = nullptr;
    for (const auto& member : *this) if (member.key() == key) found = &member.item;
    return found;
}
SettingsJsonValue& SettingsJsonObject::operator[](SettingsString key) {
    // Ordered indexed assignment updates the first matching member. Read lookup
    // uses the last, matching objectNoRef's conversion to an associative array.
    for (auto& member : *this) if (member.key() == key.view()) return member.item;
    if (ordered_) {
        appendOrderedMember(std::move(key), {});
        return orderedMembers_[orderedMembers_.size() - 1].item;
    }
    if (!storage_) storage_ = std::allocate_shared<Storage>(
        nicxlive::core::d_runtime::SourceAllocator<Storage>{});
    auto cell = storage_->members[key];
    auto& member = static_cast<SettingsJsonMember&>(cell);
    member.name = std::move(key);
    return member.item;
}
SettingsJsonValue& SettingsJsonObject::at(std::string_view key) {
    if (auto* found = if_contains(key)) return *found;
    throw std::out_of_range("Missing settings JSON key");
}
const SettingsJsonValue& SettingsJsonObject::at(std::string_view key) const {
    if (auto* found = if_contains(key)) return *found;
    throw std::out_of_range("Missing settings JSON key");
}
bool SettingsJsonObject::erase(std::string_view key) {
    if (ordered_) throw SettingsJsonError::fromStaticLiteral("JSONValue object is ordered, cannot return by ref");
    if (!storage_) return false;
    for (const auto& entry : storage_->members)
        if (entry.second.key() == key) return storage_->members.erase(entry.first) != 0;
    return false;
}
void SettingsJsonObject::appendOrderedMember(SettingsString key, SettingsJsonValue value) {
    if (!ordered_) throw SettingsJsonError::fromStaticLiteral("JSONValue object is unordered, cannot return by ref");
    orderedMembers_.append({std::move(key), std::move(value)});
}
compat::Slice<SettingsJsonMember>& SettingsJsonObject::orderedMembers() {
    if (!ordered_) throw SettingsJsonError::fromStaticLiteral("JSONValue object is unordered, cannot return by ref");
    return orderedMembers_;
}
const compat::Slice<SettingsJsonMember>& SettingsJsonObject::orderedMembers() const {
    if (!ordered_) throw SettingsJsonError::fromStaticLiteral("JSONValue object is unordered, cannot return by ref");
    return orderedMembers_;
}

SettingsJsonValue::SettingsJsonValue(const boost::json::array& value) {
    SettingsJsonArray result;
    for (const auto& item : value) result.append(SettingsJsonValue(item));
    *this = std::move(result);
}
SettingsJsonValue::SettingsJsonValue(const boost::json::object& value) {
    SettingsJsonObject result;
    for (const auto& member : value)
        result[SettingsString(std::string_view(member.key()))] = SettingsJsonValue(member.value());
    *this = std::move(result);
}
SettingsJsonValue::SettingsJsonValue(const boost::json::value& value) {
    if (value.is_bool()) *this = value.as_bool();
    else if (value.is_int64()) *this = value.as_int64();
    else if (value.is_uint64()) *this = value.as_uint64();
    else if (value.is_double()) *this = value.as_double();
    else if (value.is_string()) *this = SettingsString(std::string(value.as_string()));
    else if (value.is_array()) *this = SettingsJsonValue(value.as_array());
    else if (value.is_object()) *this = SettingsJsonValue(value.as_object());
}
bool SettingsJsonValue::as_bool() const {
    if (!is_bool()) throw SettingsJsonError::fromStaticLiteral("JSONValue is not a boolean type");
    return type_ == SettingsJsonType::True;
}
std::int64_t SettingsJsonValue::as_int64() const {
    if (!is_int64()) throw getterError("JSONValue is not an integer", 212);
    return std::bit_cast<std::int64_t>(std::get<std::uint64_t>(store_));
}
std::uint64_t SettingsJsonValue::as_uint64() const {
    if (!is_uint64()) throw getterError("JSONValue is not an unsigned integer", 230);
    return std::get<std::uint64_t>(store_);
}
double SettingsJsonValue::as_double() const {
    if (!is_double()) throw getterError("JSONValue is not a floating type", 249);
    return std::bit_cast<double>(std::get<std::uint64_t>(store_));
}
SettingsString& SettingsJsonValue::as_string() {
    if (!is_string()) throw getterError("JSONValue is not a string", 182);
    return std::get<SettingsString>(store_);
}
const SettingsString& SettingsJsonValue::as_string() const {
    if (!is_string()) throw getterError("JSONValue is not a string", 182);
    return std::get<SettingsString>(store_);
}
SettingsJsonArray& SettingsJsonValue::as_array() {
    if (!is_array()) throw getterError("JSONValue is not an array", 419);
    return std::get<SettingsJsonArray>(store_);
}
const SettingsJsonArray& SettingsJsonValue::as_array() const {
    if (!is_array()) throw getterError("JSONValue is not an array", 419);
    return std::get<SettingsJsonArray>(store_);
}
SettingsJsonObject& SettingsJsonValue::as_object() {
    if (!is_object()) throw SettingsJsonError::fromStaticLiteral("JSONValue is not an object");
    return std::get<SettingsJsonObject>(store_);
}
const SettingsJsonObject& SettingsJsonValue::as_object() const {
    if (!is_object()) throw SettingsJsonError::fromStaticLiteral("JSONValue is not an object");
    return std::get<SettingsJsonObject>(store_);
}

SettingsJsonObject& SettingsJsonValue::sourceObjectRef() {
    if (!is_object()) throw getterError("JSONValue is not an object", 305);
    auto& object = std::get<SettingsJsonObject>(store_);
    if (object.isOrdered())
        throw getterError("JSONValue object is ordered, cannot return by ref", 307);
    return object;
}
const SettingsJsonObject& SettingsJsonValue::sourceObjectRef() const {
    if (!is_object()) throw getterError("JSONValue is not an object", 305);
    const auto& object = std::get<SettingsJsonObject>(store_);
    if (object.isOrdered())
        throw getterError("JSONValue object is ordered, cannot return by ref", 307);
    return object;
}
SettingsJsonObject SettingsJsonValue::sourceObjectNoRef() const {
    if (!is_object()) throw getterError("JSONValue is not an object", 337);
    const auto& object = std::get<SettingsJsonObject>(store_);
    if (!object.isOrdered()) return object;
    SettingsJsonObject result;
    for (const auto& pair : object) result[pair.name] = pair.item;
    return result;
}
SettingsJsonArray SettingsJsonValue::sourceArrayValue() const {
    if (!is_array()) throw getterError("JSONValue is not an array", 419);
    return std::get<SettingsJsonArray>(store_);
}
SettingsJsonArray SettingsJsonValue::sourceArrayNoRef() const {
    if (!is_array()) throw getterError("JSONValue is not an array", 448);
    return std::get<SettingsJsonArray>(store_);
}
SettingsJsonValue& SettingsJsonValue::sourceIndexReference(
        std::string_view key, SettingsJsonObject& retainedObject) {
    retainedObject = sourceObjectNoRef();
    return sourceIndexedMember(retainedObject, key);
}
SettingsJsonValue SettingsJsonValue::sourceIndexValue(std::string_view key) const {
    auto retainedObject = sourceObjectNoRef();
    return sourceIndexedMember(retainedObject, key);
}
SettingsJsonValue SettingsJsonValue::sourceIndexValue(std::size_t index) const {
    if (!is_array()) throw getterError("JSONValue is not an array", 448);
    const auto array = std::get<SettingsJsonArray>(store_);
    if (index >= array.size())
        throw getterError("JSONValue array index is out of range", 787);
    return array[index];
}
void SettingsJsonValue::sourceCheckIndexAssignment() const {
    if (!is_object() && !is_null())
        throw getterError("JSONValue must be object or null", 829);
}
SettingsJsonValue& SettingsJsonValue::sourceIndexAssignSlot(SettingsString key) {
    sourceCheckIndexAssignment();
    if (is_null()) {
        SettingsJsonObject object;
        auto& slot = object[std::move(key)];
        *this = std::move(object);
        return slot;
    }
    // Actual object storage updates its first ordered duplicate, or inserts a
    // retained key; unordered null insertion is installed into this JSONValue.
    return std::get<SettingsJsonObject>(store_)[std::move(key)];
}

bool operator==(const SettingsJsonValue& left, const SettingsJsonValue& right) {
    const auto numeric = [](const SettingsJsonValue& value) {
        return value.is_int64() || value.is_uint64() || value.is_double();
    };
    if (numeric(left) && numeric(right)) {
        if (left.is_double() || right.is_double()) {
            const auto number = [](const SettingsJsonValue& value) {
                if (value.is_double()) return value.as_double();
                if (value.is_uint64()) return static_cast<double>(value.as_uint64());
                return static_cast<double>(value.as_int64());
            };
            return number(left) == number(right);
        }
        if (left.is_uint64() || right.is_uint64()) {
            const auto number = [](const SettingsJsonValue& value) {
                return value.is_uint64() ? value.as_uint64() : static_cast<std::uint64_t>(value.as_int64());
            };
            return number(left) == number(right);
        }
        return left.as_int64() == right.as_int64();
    }
    if (left.type_ != right.type_) return false;
    if (left.is_null()) return true;
    if (left.is_bool()) return left.as_bool() == right.as_bool();
    if (left.is_string()) return left.as_string() == right.as_string();
    if (left.is_array()) {
        const auto& a = left.as_array();
        const auto& b = right.as_array();
        if (a.size() != b.size()) return false;
        for (std::size_t index = 0; index < a.size(); ++index) if (!(a[index] == b[index])) return false;
        return true;
    }
    if (left.is_object()) {
        const auto& a = left.as_object();
        const auto& b = right.as_object();
        if (!a.isOrdered() && !b.isOrdered() && a.identity() == b.identity()) return true;
        if (a.size() != b.size()) return false;
        if (a.isOrdered() && b.isOrdered()) {
            for (const auto& member : a) {
                bool found = false;
                for (const auto& other : b) {
                    if (other.key() == member.key() && other.value() == member.value()) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        if (b.isOrdered()) {
            for (const auto& member : b) {
                const auto* other = a.if_contains(member.key());
                if (other == nullptr || !(*other == member.value())) return false;
            }
            return true;
        }
        if (a.isOrdered()) {
            for (const auto& member : a) {
                const auto* other = b.if_contains(member.key());
                if (other == nullptr || !(*other == member.value())) return false;
            }
            return true;
        }
        for (const auto& member : a) {
            const auto* other = b.if_contains(member.key());
            if (other == nullptr || !(member.value() == *other)) return false;
        }
        return true;
    }
    return false;
}
} // namespace nijigenerate::core::settings
