#pragma once
#include "retained_string.hpp"
#include <nodes/node.hpp>
#include <concepts>
#include <typeinfo>

namespace nijigenerate::compat {
// Runtime RTTI names are registered explicitly; C++ mangled names are never exposed.
RetainedString ngNodeDynamicClassName(const nicxlive::core::nodes::Node& node);
void ngRegisterResourceClassNames();
namespace detail {
void registerNodeClassName(const std::type_info& type, RetainedString name);
}
template<class T> requires std::derived_from<T, nicxlive::core::nodes::Node>
void ngRegisterNodeDynamicClassName(RetainedString name) {
    detail::registerNodeClassName(typeid(T), std::move(name));
}
}
