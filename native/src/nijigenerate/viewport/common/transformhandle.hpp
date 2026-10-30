#pragma once
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <nodes/node.hpp>
#include <functional>
#include <cstddef>
#include <memory>

namespace nijigenerate::viewport::common::transformhandle {
using NodePtr = std::shared_ptr<nicxlive::core::nodes::Node>;
using Vec2 = nicxlive::core::math::Vec2;
using Vec4 = nicxlive::core::math::Vec4;
namespace detail {
struct ActionKeyHash {
    std::size_t operator()(const compat::RetainedStringSlice& key) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
    }
};
struct ActionKeyEqual {
    bool operator()(const compat::RetainedStringSlice& left,
        const compat::RetainedStringSlice& right) const noexcept { return left == right; }
};
}
using ActionMap = nicxlive::core::common::ResourceKeyMap<compat::RetainedStringSlice,
    actions::ActionPtr, detail::ActionKeyHash, detail::ActionKeyEqual>;
class ViewportTransformHandleAdapter : public std::enable_shared_from_this<ViewportTransformHandleAdapter> {
public:
    ViewportTransformHandleAdapter() = default;
    ViewportTransformHandleAdapter(const ViewportTransformHandleAdapter&) = delete;
    ViewportTransformHandleAdapter& operator=(const ViewportTransformHandleAdapter&) = delete;
    virtual ~ViewportTransformHandleAdapter() = default;
    virtual Vec4 bounds(NodePtr node) = 0;
    virtual Vec2 scaleHandleValue(NodePtr node) = 0;
    virtual void beginScale(NodePtr node) = 0;
    virtual void applyScale(NodePtr node, Vec2 scale) = 0;
    virtual void endScale(NodePtr node, Vec2 previousScale, ActionMap& actions) = 0;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<ViewportTransformHandleAdapter> other);
    virtual int opCmp(std::shared_ptr<ViewportTransformHandleAdapter> other);
};
std::shared_ptr<ViewportTransformHandleAdapter> ngViewportTransformHandleAdapter(NodePtr node);
bool ngViewportTransformHandleAdapterObjectEquals(std::shared_ptr<ViewportTransformHandleAdapter> left,
    std::shared_ptr<ViewportTransformHandleAdapter> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::common::transformhandle::ViewportTransformHandleAdapter> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral(
                    "(nijigenerate.viewport.common.transformhandle.ViewportTransformHandleAdapter)"),
            RetainedString::fromStaticLiteral("(ViewportTransformHandleAdapter)"),
            RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.transformhandle.ViewportTransformHandleAdapter"),
            RetainedString::fromStaticLiteral("ViewportTransformHandleAdapter")};
    }
};
} // namespace nicxlive::core::d_runtime
