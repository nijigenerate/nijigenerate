#include "dynamic_key.hpp"
#include <nijigenerate/compat/phobos_string_hash.hpp>

namespace nijigenerate::commands::automesh::dynamic {

compat::RetainedStringSlice AutoMeshKey::toString() const {
    return id;
}

std::size_t AutoMeshKey::toHash() const noexcept {
    return compat::ngPhobosStringHash(id.view());
}

bool AutoMeshKey::opEquals(AutoMeshKey rhs) const noexcept {
    return id == rhs.id;
}

} // namespace nijigenerate::commands::automesh::dynamic
