#include "dynamic_keys.hpp"
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::commands::node::dynamic {

compat::RetainedString detail::NodeTypeKey::toString() const {
    if (suffix.empty()) return name;
    try {
        std::string result(name.view());
        result += '|';
        result += suffix.view();
        return compat::RetainedString(std::move(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

std::size_t detail::NodeTypeKey::toHash() const noexcept {
    return compat::ngPhobosStringHash(name.view()) ^
        (compat::ngPhobosStringHash(suffix.view()) * std::size_t{1315423911u});
}

bool detail::NodeTypeKey::operator==(NodeTypeKey rhs) const noexcept {
    return name == rhs.name && suffix == rhs.suffix;
}

AddNodeKey::AddNodeKey(compat::RetainedString name, compat::RetainedString suffix)
    : base{name, suffix} {}
compat::RetainedString AddNodeKey::name() const { return base.name; }
compat::RetainedString AddNodeKey::suffix() const { return base.suffix; }
compat::RetainedString AddNodeKey::toString() const { return base.toString(); }
std::size_t AddNodeKey::toHash() const noexcept { return base.toHash(); }
bool AddNodeKey::operator==(AddNodeKey rhs) const noexcept { return base == rhs.base; }

InsertNodeKey::InsertNodeKey(compat::RetainedString name, compat::RetainedString suffix)
    : base{name, suffix} {}
compat::RetainedString InsertNodeKey::name() const { return base.name; }
compat::RetainedString InsertNodeKey::suffix() const { return base.suffix; }
compat::RetainedString InsertNodeKey::toString() const { return base.toString(); }
std::size_t InsertNodeKey::toHash() const noexcept { return base.toHash(); }
bool InsertNodeKey::operator==(InsertNodeKey rhs) const noexcept { return base == rhs.base; }

compat::RetainedString ConvertToKey::toString() const { return toType; }
std::size_t ConvertToKey::toHash() const noexcept { return compat::ngPhobosStringHash(toType.view()); }
bool ConvertToKey::operator==(ConvertToKey rhs) const noexcept { return toType == rhs.toType; }

} // namespace nijigenerate::commands::node::dynamic
