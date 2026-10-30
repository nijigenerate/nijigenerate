#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <memory>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::panels::inspector {

// The canonical template is defined by common.hpp; no inspector is instantiated here.
template<class T> class TypedInspector;

namespace detail {

// Copies retain one callable environment rather than clone captured state.
using NodeInspectorFactory = compat::SharedDelegate<
    std::shared_ptr<TypedInspector<nicxlive::core::nodes::Node>>()>;
using NodeInspectorFactories = compat::Slice<NodeInspectorFactory>;

// Module-private ordinary D storage is thread-local, unlike __gshared state.
extern thread_local NodeInspectorFactories nodeInspectors;
extern thread_local bool inspectorsInitialized;

} // namespace detail
} // namespace nijigenerate::panels::inspector
