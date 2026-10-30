#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>

namespace nijigenerate::commands::node::dynamic {

namespace detail {
struct NodeTypeKey {
    compat::RetainedString name;
    compat::RetainedString suffix;
    compat::RetainedString toString() const;
    std::size_t toHash() const noexcept;
    bool operator==(NodeTypeKey rhs) const noexcept;
};
} // namespace detail

struct AddNodeKey {
private:
    detail::NodeTypeKey base;
public:
    AddNodeKey() = default;
    AddNodeKey(compat::RetainedString name, compat::RetainedString suffix = {});
    compat::RetainedString name() const;
    compat::RetainedString suffix() const;
    compat::RetainedString toString() const;
    std::size_t toHash() const noexcept;
    bool operator==(AddNodeKey rhs) const noexcept;
};

struct InsertNodeKey {
private:
    detail::NodeTypeKey base;
public:
    InsertNodeKey() = default;
    InsertNodeKey(compat::RetainedString name, compat::RetainedString suffix = {});
    compat::RetainedString name() const;
    compat::RetainedString suffix() const;
    compat::RetainedString toString() const;
    std::size_t toHash() const noexcept;
    bool operator==(InsertNodeKey rhs) const noexcept;
};

struct ConvertToKey {
    compat::RetainedString toType;
    compat::RetainedString toString() const;
    std::size_t toHash() const noexcept;
    bool operator==(ConvertToKey rhs) const noexcept;
};

} // namespace nijigenerate::commands::node::dynamic

namespace nicxlive::core::d_runtime {
#define NG_DYNAMIC_NODE_KEY_SOURCE_NAMES(NativeType, SourceName) \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::node::dynamic::NativeType> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = \
            "(nijigenerate.commands.node.dynamic." SourceName ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" SourceName ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.dynamic." SourceName; \
        inline static constexpr char unqualifiedElement[] = SourceName; \
    };
NG_DYNAMIC_NODE_KEY_SOURCE_NAMES(detail::NodeTypeKey, "NodeTypeKey")
NG_DYNAMIC_NODE_KEY_SOURCE_NAMES(AddNodeKey, "AddNodeKey")
NG_DYNAMIC_NODE_KEY_SOURCE_NAMES(InsertNodeKey, "InsertNodeKey")
NG_DYNAMIC_NODE_KEY_SOURCE_NAMES(ConvertToKey, "ConvertToKey")
#undef NG_DYNAMIC_NODE_KEY_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime
