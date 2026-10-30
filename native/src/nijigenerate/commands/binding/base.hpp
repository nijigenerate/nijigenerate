#pragma once

#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/commands/binding/drag_data.hpp>
#include <param/parameter.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>

namespace nijigenerate::commands::binding {
using Node = ::nicxlive::core::nodes::Node;
using Parameter = ::nicxlive::core::param::Parameter;
using ParameterBinding = ::nicxlive::core::param::ParameterBinding;
// One real Resource identity; the legacy typed fields are read-only derived views.
class ResourceRef {
    std::shared_ptr<::nicxlive::core::Resource> resource_;
    std::shared_ptr<Node> node_;
    std::shared_ptr<Parameter> parameter_;
public:
    const std::shared_ptr<Node>& node = node_;
    const std::shared_ptr<Parameter>& parameter = parameter_;
    ResourceRef() = default;
    ResourceRef(std::shared_ptr<::nicxlive::core::Resource> value)
        : resource_(std::move(value)), node_(std::dynamic_pointer_cast<Node>(resource_)),
          parameter_(std::dynamic_pointer_cast<Parameter>(resource_)) {}
    template<class T> requires std::derived_from<T, ::nicxlive::core::Resource>
    ResourceRef(std::shared_ptr<T> value)
        : ResourceRef(std::static_pointer_cast<::nicxlive::core::Resource>(std::move(value))) {}
    ResourceRef(const ResourceRef& other) : ResourceRef(other.resource_) {}
    ResourceRef(ResourceRef&& other) noexcept : ResourceRef(static_cast<const ResourceRef&>(other)) {}
    ResourceRef& operator=(const ResourceRef& other) {
        if (this != &other) {
            const auto next = other.resource_;
            const auto node = std::dynamic_pointer_cast<Node>(next);
            const auto parameter = std::dynamic_pointer_cast<Parameter>(next);
            // Publish all views before releasing the previous actual object.
            [[maybe_unused]] const auto previous = resource_;
            resource_ = next;
            node_ = node;
            parameter_ = parameter;
        }
        return *this;
    }
    ResourceRef& operator=(ResourceRef&& other) noexcept {
        return *this = static_cast<const ResourceRef&>(other);
    }
    std::shared_ptr<::nicxlive::core::Resource> getResource() const { return resource_; }
    friend bool operator==(const ResourceRef& left, const ResourceRef& right) {
        return ::nicxlive::core::ngResourceObjectEquals(left.resource_, right.resource_);
    }
};
struct ResourceHash {
    std::size_t operator()(const ResourceRef& value) const {
        return ::nicxlive::core::ngResourceInterfaceHash(value.getResource());
    }
};
struct ResourceKeyEqual {
    bool operator()(const ResourceRef& query, const ResourceRef& stored) const {
        return ::nicxlive::core::ngResourceInterfaceKeyEquals(query.getResource(), stored.getResource());
    }
};
struct BindingTarget {
    ResourceRef target;
    nicxlive::core::common::NullableString name;
    BindingTarget() = default;
    BindingTarget(const ::nicxlive::core::param::BindTarget& value)
        : target(value.getResource()), name(value.name) {}
    friend bool operator==(const BindingTarget&, const BindingTarget&) = default;
};
struct BindingTargetHash {
    std::size_t operator()(const BindingTarget& value) const {
        // Pinned compiler clone.d buildXtoHash: h = h * 33 +
        // typeid(field).getHash(&field), in declared field order.
        const auto resourceHash = ResourceHash{}(value.target);
        return resourceHash * std::size_t{33} +
            ::nicxlive::core::d_runtime::ngPhobosStringHash(value.name.view());
    }
};
using BindingList = compat::Slice<std::shared_ptr<ParameterBinding>>;
using BindingEntries = ::nicxlive::core::common::ResourceKeyMap<ResourceRef, BindingList,
    ResourceHash, ResourceKeyEqual>;
using SelectedBindings = ::nicxlive::core::common::ResourceKeyMap<BindingTarget,
    std::shared_ptr<ParameterBinding>, BindingTargetHash, std::equal_to<BindingTarget>>;
inline thread_local BindingEntries cParamBindingEntries, cParamBindingEntriesAll;
inline thread_local SelectedBindings cSelectedBindings, cClipboardBindings;
inline thread_local compat::Slice<ResourceRef> cCompatibleNodes;
inline thread_local compat::Vec2u cParamPoint{}, cClipboardPoint{};
inline thread_local std::shared_ptr<Parameter> cClipboardParameter;
inline thread_local bool selectedOnly = false;
void ngInitBindingProviders();
void refreshBindingList(std::shared_ptr<Parameter> param, bool selectedOnly = false);
void paramPointChanged(std::shared_ptr<Parameter> param);
compat::Slice<ResourceRef> getCompatibleNodes();
void copySelectionToNode(std::shared_ptr<Parameter> param, std::shared_ptr<Node> target);
void swapSelectionWithNode(std::shared_ptr<Parameter> param, std::shared_ptr<Node> target);
inline bool ngBindingHasKeypoint(std::shared_ptr<::nicxlive::core::param::ParameterBinding> binding,
                                 compat::Vec2u point) {
    namespace param = ::nicxlive::core::param;
    if (!binding) return false;
    auto isSet = binding->getIsSet();
    if (point.x >= isSet.size() || point.y >= isSet[point.x].size()) return false;
    if (auto valueBinding = std::dynamic_pointer_cast<param::ValueParameterBinding>(binding))
        return point.x < valueBinding->values.size() && point.y < valueBinding->values[point.x].size();
    if (auto parameterBinding = std::dynamic_pointer_cast<param::ParameterParameterBinding>(binding))
        return point.x < parameterBinding->values.size() && point.y < parameterBinding->values[point.x].size();
    if (auto deformationBinding = std::dynamic_pointer_cast<param::DeformationParameterBinding>(binding))
        return point.x < deformationBinding->values.size() && point.y < deformationBinding->values[point.x].size();
    return true;
}
inline bool ngBindingIsSetAt(std::shared_ptr<::nicxlive::core::param::ParameterBinding> binding,
                             compat::Vec2u point) {
    return ngBindingHasKeypoint(binding, point) && binding->isSet({point.x, point.y});
}
} // namespace nijigenerate::commands::binding

namespace nijigenerate::commands::binding::base {
using ::nijigenerate::commands::binding::ResourceRef;
using ::nijigenerate::commands::binding::BindingTarget;
using ::nijigenerate::commands::binding::BindingList;
using ::nijigenerate::commands::binding::BindingEntries;
using ::nijigenerate::commands::binding::SelectedBindings;
using ::nijigenerate::commands::binding::ParamDragDropData;
using ::nijigenerate::commands::binding::cParamBindingEntries;
using ::nijigenerate::commands::binding::cParamBindingEntriesAll;
using ::nijigenerate::commands::binding::cSelectedBindings;
using ::nijigenerate::commands::binding::cClipboardBindings;
using ::nijigenerate::commands::binding::cCompatibleNodes;
using ::nijigenerate::commands::binding::cParamPoint;
using ::nijigenerate::commands::binding::cClipboardPoint;
using ::nijigenerate::commands::binding::cClipboardParameter;
using ::nijigenerate::commands::binding::selectedOnly;
using ::nijigenerate::commands::binding::ngInitBindingProviders;
using ::nijigenerate::commands::binding::refreshBindingList;
using ::nijigenerate::commands::binding::paramPointChanged;
using ::nijigenerate::commands::binding::ngBindingHasKeypoint;
using ::nijigenerate::commands::binding::ngBindingIsSetAt;
using ::nijigenerate::commands::binding::getCompatibleNodes;
using ::nijigenerate::commands::binding::copySelectionToNode;
using ::nijigenerate::commands::binding::swapSelectionWithNode;
} // namespace nijigenerate::commands::binding::base
