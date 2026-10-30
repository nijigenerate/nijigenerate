#pragma once

#if defined(NG_AUTOMESH_INIT_DIAG_FORCE) || defined(ngAutoMeshInitDiagForce)

#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/compat/type_sequence.hpp>
#include <nijigenerate/viewport/vertex/automesh/package.hpp>
#include <cstddef>
#include <tuple>

namespace nijigenerate::commands::detail {

template<class Processors> struct AutoMeshForcedCommandAliases;

template<class... Processors>
struct AutoMeshForcedCommandAliases<compat::TypeSequence<Processors...>> {
    using type = compat::TypeSequence<automesh::dynamic::ApplyAutoMeshPT<Processors>...>;
    template<std::size_t Index>
    using at = std::tuple_element_t<Index, std::tuple<automesh::dynamic::ApplyAutoMeshPT<Processors>...>>;

    // Force the actual class definitions, rather than merely naming an
    // uninstantiated specialization. No command or processor is constructed.
    inline static constexpr std::size_t definitionExtent =
        (std::size_t{0} + ... + sizeof(automesh::dynamic::ApplyAutoMeshPT<Processors>));
};

using AutoMeshInitDiagForceAliases = typename AutoMeshForcedCommandAliases<
    viewport::vertex::automesh::AutoMeshProcessorTypes>::type;

inline constexpr std::size_t autoMeshInitDiagForceDefinitionExtent =
    AutoMeshForcedCommandAliases<
        viewport::vertex::automesh::AutoMeshProcessorTypes>::definitionExtent;

} // namespace nijigenerate::commands::detail

namespace nijigenerate::commands {

// The source-generated indexed aliases name these same concrete command types.
template<std::size_t Index>
using AutoMeshInitDiagForce = typename detail::AutoMeshForcedCommandAliases<
    viewport::vertex::automesh::AutoMeshProcessorTypes>::template at<Index>;

} // namespace nijigenerate::commands

#endif
