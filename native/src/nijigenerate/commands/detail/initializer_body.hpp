#pragma once

#include <concepts>

namespace nijigenerate::commands::detail {

// An owning initializer supplies this from the same operations used by its
// production entry. Absence is not a false semantic-compilation result.
template<class Key> struct InitializerBody;

template<class Key> concept HasInitializerBody = requires {
    { InitializerBody<Key>::supported() } -> std::same_as<bool>;
};

} // namespace nijigenerate::commands::detail
