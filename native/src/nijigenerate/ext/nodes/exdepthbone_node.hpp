#pragma once

#include <nodes/node.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <serde_reader.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_data.hpp>
#include <cstddef>
#include <memory>
#include <source_location>
#include <functional>
#include <utility>

namespace nijigenerate::ext::nodes {

class ExDepthBone : public nicxlive::core::nodes::Node {
public:
    compat::RetainedString boneId;
    nicxlive::core::math::Vec3 restHead{0, 0, 0};
    nicxlive::core::math::Vec3 restTail{0, 100, 0};
    float restRoll{0.0f};
    compat::RetainedString constraintType;
    nicxlive::core::math::Vec3 hingeAxis{0, 0, 1};
    bool lockRotation{false};
    bool lockTranslation{false};
    bool allowParentToTargets{true};
    compat::Slice<float> rotationLimits;
    float maxStepRadians{0.0f};

    ExDepthBone(const ExDepthBone&) = delete;
    ExDepthBone& operator=(const ExDepthBone&) = delete;
    ExDepthBone(ExDepthBone&&) = delete;
    ExDepthBone& operator=(ExDepthBone&&) = delete;
    static std::shared_ptr<ExDepthBone> create(std::shared_ptr<Node> parent = nullptr);
    template<class ParentProducer> static std::shared_ptr<ExDepthBone> createSourceDeferred(ParentProducer&& produceParent) {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExDepthBone),
            compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthbone.ExDepthBone"));
        auto result = std::allocate_shared<ExDepthBone>(
            nicxlive::core::d_runtime::SourceAllocator<ExDepthBone>{}, FieldInitializationOnly{});
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        result->constructWithParent(parent);
        return result;
    }
    nicxlive::core::common::NullableString typeId() const override;
    void copyFrom(const Node& source, bool clone = false, bool deepCopy = true) override;
protected:
    void serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer,
        bool recursive = true,
        nicxlive::core::nodes::SerializeNodeFlags flags = nicxlive::core::nodes::SerializeNodeFlags::All) const override;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) override;
    struct FieldInitializationOnly {};
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    explicit ExDepthBone(FieldInitializationOnly);
};

// A source interior pointer retains the actual array containing its physical cell.
class DepthRigBindingPointer {
    compat::Slice<ExDepthRigBinding> backing_;
    ExDepthRigBinding* value_ = nullptr;

    DepthRigBindingPointer(compat::Slice<ExDepthRigBinding> backing, ExDepthRigBinding* value)
        : backing_(backing), value_(value) {}
public:
    DepthRigBindingPointer() = default;
    DepthRigBindingPointer(std::nullptr_t) noexcept {}
    DepthRigBindingPointer(const DepthRigBindingPointer&) = default;
    DepthRigBindingPointer& operator=(const DepthRigBindingPointer& other) {
        if (this == &other) return *this;
        [[maybe_unused]] const auto previous = backing_;
        backing_ = other.backing_;
        value_ = other.value_;
        return *this;
    }
    static DepthRigBindingPointer fromArray(compat::Slice<ExDepthRigBinding> backing,
            std::size_t index, std::source_location source = std::source_location::current()) {
        auto* value = std::addressof(compat::ngArrayAt(backing, index, source));
        return DepthRigBindingPointer(backing, value);
    }
    static DepthRigBindingPointer fromArray(compat::Slice<ExDepthRigBinding> backing,
            std::size_t index, compat::RetainedString sourceFile, std::size_t sourceLine) {
        if (index >= backing.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, backing.size(), sourceFile, sourceLine);
        return DepthRigBindingPointer(backing, std::addressof(backing.data()[index]));
    }
    explicit operator bool() const noexcept { return value_ != nullptr; }
    friend bool operator==(const DepthRigBindingPointer& left,
            const DepthRigBindingPointer& right) noexcept { return left.value_ == right.value_; }
    ExDepthRigBinding* operator->() const { return compat::ngRequireObject(value_); }
    ExDepthRigBinding& operator*() const { return *compat::ngRequireObject(value_); }
    // Native callers must retain this pointer value while using the raw borrow.
    ExDepthRigBinding* get() const noexcept { return value_; }
};

class ExDepthRigRoot : public nicxlive::core::nodes::Node {
public:
    compat::Slice<ExDepthRigBinding> bindings;
    ExDepthRigRoot(const ExDepthRigRoot&) = delete;
    ExDepthRigRoot& operator=(const ExDepthRigRoot&) = delete;
    ExDepthRigRoot(ExDepthRigRoot&&) = delete;
    ExDepthRigRoot& operator=(ExDepthRigRoot&&) = delete;
    static std::shared_ptr<ExDepthRigRoot> create(std::shared_ptr<Node> parent = nullptr);
    template<class ParentProducer> static std::shared_ptr<ExDepthRigRoot> createSourceDeferred(ParentProducer&& produceParent) {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExDepthRigRoot),
            compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot"));
        auto result = std::allocate_shared<ExDepthRigRoot>(
            nicxlive::core::d_runtime::SourceAllocator<ExDepthRigRoot>{}, FieldInitializationOnly{});
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        result->constructWithParent(parent);
        return result;
    }
    nicxlive::core::common::NullableString typeId() const override;
    void copyFrom(const Node& source, bool clone = false, bool deepCopy = true) override;
    virtual compat::Slice<std::shared_ptr<ExDepthBone>> depthBones();
    virtual std::ptrdiff_t findBindingIndex(std::uint64_t targetUuid) const;
    virtual DepthRigBindingPointer getOrCreateBinding(std::shared_ptr<Node> target, ExDepthTargetKind kind);
    virtual void addBoneSource(std::shared_ptr<Node> target, ExDepthTargetKind kind,
        std::shared_ptr<ExDepthBone> bone);
    virtual void removeBoneSource(std::shared_ptr<Node> target, std::shared_ptr<ExDepthBone> bone);
protected:
    void serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer,
        bool recursive = true,
        nicxlive::core::nodes::SerializeNodeFlags flags = nicxlive::core::nodes::SerializeNodeFlags::All) const override;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) override;
    struct FieldInitializationOnly {};
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    explicit ExDepthRigRoot(FieldInitializationOnly);
};

void ngRegisterExDepthBoneNodes();
std::shared_ptr<ExDepthBone> ngCreateDepthBone(std::shared_ptr<nicxlive::core::nodes::Node> parent,
    compat::RetainedString boneId, nicxlive::core::math::Vec3 restHead,
    nicxlive::core::math::Vec3 restTail, float restRoll = 0.0f);
using CopiedRigNode = std::shared_ptr<nicxlive::core::nodes::Node>;
void ngRemapCopiedDepthRigReferences(CopiedRigNode source, CopiedRigNode copy);
void ngRemapCopiedDepthRigReferences(compat::Slice<CopiedRigNode> sources, compat::Slice<CopiedRigNode> copies);

} // namespace nijigenerate::ext::nodes

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExDepthBone> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.exdepthbone.ExDepthBone";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.exdepthbone.ExDepthBone)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExDepthBone> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exdepthbone.ExDepthBone)";
    inline static constexpr char unqualifiedArgument[] = "(ExDepthBone)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exdepthbone.ExDepthBone";
    inline static constexpr char unqualifiedElement[] = "ExDepthBone";
};
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExDepthRigRoot> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExDepthRigRoot> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot)";
    inline static constexpr char unqualifiedArgument[] = "(ExDepthRigRoot)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot";
    inline static constexpr char unqualifiedElement[] = "ExDepthRigRoot";
};
}
