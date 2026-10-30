#pragma once

#include "binding_all_value_action.hpp"
#include "binding_value_notification.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <limits>
#include <new>
#include <source_location>
#include <string>
#include <type_traits>
#include <vector>

namespace nijigenerate::actions::binding {

namespace detail {
template<class T> T bindingInitialValue() {
    if constexpr (std::is_floating_point_v<T>) return std::numeric_limits<T>::quiet_NaN();
    else if constexpr (std::same_as<T, char> || std::same_as<T, char8_t>) return static_cast<T>(0xff);
    else if constexpr (std::same_as<T, char16_t> || std::same_as<T, char32_t> || std::same_as<T, wchar_t>)
        return static_cast<T>(0xffff);
    else if constexpr (std::is_enum_v<T> && requires { core::translation::FormatEnumTraits<T>::members; }) {
        static_assert(std::size(core::translation::FormatEnumTraits<T>::members) > 0);
        return core::translation::FormatEnumTraits<T>::members[0].value;
    } else if constexpr (std::same_as<T, nicxlive::core::math::Vec2>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan};
    } else if constexpr (std::same_as<T, nicxlive::core::math::Vec3>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan};
    } else if constexpr (std::same_as<T, nicxlive::core::math::Vec4>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan};
    } else return T{};
}

inline compat::RetainedStringSlice bindingIntegerName(int value) {
    char bytes[std::numeric_limits<int>::digits10 + 3];
    const auto result = std::to_chars(bytes, bytes + sizeof(bytes), value);
    try {
        return compat::RetainedString(std::string(bytes, result.ptr));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> void swapBindingValue(T& left, T& right) {
    using std::swap;
    swap(left, right);
}

inline void swapBindingValue(nicxlive::core::math::Vec2Array& a,
    nicxlive::core::math::Vec2Array& b) noexcept {
    if (std::addressof(a) == std::addressof(b)) return;
    // Source swap relocates the complete descriptor state. Include the actual
    // contiguous raw backing, keeping it consistent with the swapped lanes.
    std::swap(a.x, b.x);
    std::swap(a.y, b.y);
    std::swap(a.xPtr_, b.xPtr_);
    std::swap(a.yPtr_, b.yPtr_);
    std::swap(a.logicalLength_, b.logicalLength_);
    std::swap(a.laneStride_, b.laneStride_);
    std::swap(a.laneBase_, b.laneBase_);
    std::swap(a.viewCapacity_, b.viewCapacity_);
    std::swap(a.ownsStorage_, b.ownsStorage_);
    std::swap(a.alignment_, b.alignment_);
    std::swap(a.sourceBacking_, b.sourceBacking_);
    a.backing_.swap(b.backing_);
}

inline void swapBindingValue(nicxlive::core::param::DeformSlot& left,
    nicxlive::core::param::DeformSlot& right) noexcept {
    swapBindingValue(left.vertexOffsets, right.vertexOffsets);
}

template<std::size_t N> ActionText describeBindingValue(const char (&patternKey)[N],
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
    const compat::RetainedStringSlice& name) {
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(patternKey));
    auto bindingName = compat::ngRequireObject(binding)->getNameDescriptor();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(std::move(bindingName)),
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    if (formatted.empty()) return {};
    return retainBindingText(std::move(formatted));
}
} // namespace detail

template<class I, class... Args> std::shared_ptr<I> ngCreateBindingValueAction(Args&&... args);

template<class I,class Producer>
std::shared_ptr<I> ngCreateBindingValueActionDeferred(Producer&& produce) {
    return ngCreateBindingValueAction<I>(
        detail::BindingArgumentsProducer<Producer>{std::forward<Producer>(produce)});
}
template<class T, class Binding> class ParameterBindingValueChangeAction;

template<class T, class Binding> requires std::derived_from<Binding, nicxlive::core::param::ParameterBindingImpl<T>>
class ParameterBindingValueChangeAction<T, Binding> : public LazyBoundAction,
    public std::enable_shared_from_this<ParameterBindingValueChangeAction<T, Binding>> {
    template<class I, class... Args> friend std::shared_ptr<I> ngCreateBindingValueAction(Args&&...);
    using BaseBinding = nicxlive::core::param::ParameterBindingImpl<T>;
public:
    using TSelf = ParameterBindingValueChangeAction;
    ParameterBindingValueChangeAction(const ParameterBindingValueChangeAction&) = delete;
    ParameterBindingValueChangeAction& operator=(const ParameterBindingValueChangeAction&) = delete;
    ParameterBindingValueChangeAction(ParameterBindingValueChangeAction&&) = delete;
    ParameterBindingValueChangeAction& operator=(ParameterBindingValueChangeAction&&) = delete;
    compat::RetainedStringSlice name;
    std::shared_ptr<Binding> self;
    int pointx = 0;
    int pointy = 0;
    T value = detail::bindingInitialValue<T>();
    bool isSet = false;
    bool undoable = false;
    bool _dirty = false;
    bool notifyDepthBone = false;

    virtual compat::RetainedString toString() {
        const auto owner = this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }

    virtual void markAsDirty() { _dirty = true; }
    void updateNewState() override {
        auto owner = this->weak_from_this().lock();
        detail::notifyBindingValueChanged(self, pointx, pointy, notifyDepthBone);
    }
    void clear() override { _dirty = false; }
    virtual bool dirty() { return _dirty; }

    void rollback() override {
        auto owner = this->weak_from_this().lock();
        if (undoable) {
            swapCurrent(356, 357);
            const auto receiver = compat::ngRequireObject(self);
            receiver->reInterpolate();
            undoable = false;
            detail::notifyBindingValueChanged(self, pointx, pointy, notifyDepthBone);
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        if (!undoable) {
            swapCurrent(369, 370);
            const auto receiver = compat::ngRequireObject(self);
            receiver->reInterpolate();
            undoable = true;
            detail::notifyBindingValueChanged(self, pointx, pointy, notifyDepthBone);
        }
    }
    ActionText describe() override { return describeWithKey("%s->%s changed"); }
    ActionText describeUndo() override { return describeWithKey("%s->%s change cancelled"); }
    ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    bool merge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        return canMerge(std::move(other));
    }
    bool canMerge(ActionPtr other) override {
        const auto change = std::dynamic_pointer_cast<TSelf>(std::move(other));
        return change && name == change->name && pointx == change->pointx && pointy == change->pointy;
    }

protected:
    ParameterBindingValueChangeAction() {
        detail::prepareBindingActionClass<TSelf, T, Binding>("ParameterBindingValueChangeAction");
    }
    void initialize(compat::RetainedStringSlice label, std::shared_ptr<Binding> binding, int x, int y,
        BindingUpdate update = {}, bool notify = true) {
        initializeState<307, 308>(std::move(label), std::move(binding), x, y, std::move(update), notify);
    }
    void initialize(int label, std::shared_ptr<Binding> binding, int x, int y,
        BindingUpdate update = {}, bool notify = true) {
        initializeState<330, 331>(detail::bindingIntegerName(label), std::move(binding), x, y,
            std::move(update), notify);
    }
private:
    template<std::size_t valueLine, std::size_t flagLine>
    void initializeState(compat::RetainedStringSlice label, std::shared_ptr<Binding> binding, int x, int y,
        BindingUpdate update, bool notify) {
        name = std::move(label);
        self = binding;
        pointx = x;
        pointy = y;
        const auto previous = detail::cloneActionValue(
            detail::bindingArrayAt(detail::bindingArrayAt(
                compat::ngRequireObject(binding)->values, index(x), valueLine), index(y), valueLine));
        value = previous;
        isSet = detail::bindingArrayAt(detail::bindingArrayAt(
            compat::ngRequireObject(binding)->BaseBinding::getIsSet(), index(x), flagLine), index(y), flagLine);
        undoable = true;
        _dirty = false;
        notifyDepthBone = notify;
        if (update) {
            update();
            updateNewState();
        }
    }
    static std::size_t index(int value) { return static_cast<std::size_t>(value); }
    void swapCurrent(std::size_t valueLine, std::size_t flagLine) {
        {
            const auto receiver = compat::ngRequireObject(self);
            const auto rows = receiver->values;
            auto row = detail::bindingArrayAt(rows, index(pointx), valueLine);
            auto& current = detail::bindingArrayAt(row, index(pointy), valueLine);
            detail::swapBindingValue(current, value);
        }
        {
            const auto receiver = compat::ngRequireObject(self);
            const auto rows = receiver->BaseBinding::getIsSet();
            auto row = detail::bindingArrayAt(rows, index(pointx), flagLine);
            auto& currentIsSet = detail::bindingArrayAt(row, index(pointy), flagLine);
            std::swap(currentIsSet, isSet);
        }
    }
    template<std::size_t N> ActionText describeWithKey(const char (&key)[N]) {
        auto owner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        const auto receiver = compat::ngRequireObject(self);
        auto bindingName = receiver->getNameDescriptor();
        const std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(std::move(bindingName)),
            core::translation::ngFormatArgument(name)};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
        if (formatted.empty()) return {};
        return detail::retainBindingText(std::move(formatted));
    }
};

template<class T, class Binding> requires std::derived_from<Binding, nicxlive::core::param::ParameterBindingImpl<T>>
class ParameterBindingValueChangeAction<T, compat::Slice<std::shared_ptr<Binding>>> : public LazyBoundAction,
    public std::enable_shared_from_this<ParameterBindingValueChangeAction<T, compat::Slice<std::shared_ptr<Binding>>>> {
    template<class I, class... Args> friend std::shared_ptr<I> ngCreateBindingValueAction(Args&&...);
    using BaseBinding = nicxlive::core::param::ParameterBindingImpl<T>;
public:
    using TSelf = ParameterBindingValueChangeAction;
    using TBinding = compat::Slice<std::shared_ptr<Binding>>;
    ParameterBindingValueChangeAction(const ParameterBindingValueChangeAction&) = delete;
    ParameterBindingValueChangeAction& operator=(const ParameterBindingValueChangeAction&) = delete;
    ParameterBindingValueChangeAction(ParameterBindingValueChangeAction&&) = delete;
    ParameterBindingValueChangeAction& operator=(ParameterBindingValueChangeAction&&) = delete;
    compat::RetainedStringSlice name;
    TBinding self;
    int pointx = 0;
    int pointy = 0;
    nicxlive::core::common::SharedArray<T> value;
    nicxlive::core::common::SharedArray<bool> isSet;
    bool undoable = false;
    bool _dirty = false;
    bool notifyDepthBone = false;

    virtual compat::RetainedString toString() {
        const auto owner = this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }

    virtual void markAsDirty() { _dirty = true; }
    void updateNewState() override {
        auto owner = this->weak_from_this().lock();
        notifyCurrent();
    }
    void clear() override { _dirty = false; }
    virtual bool dirty() { return _dirty; }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        if (undoable) {
            swapCurrent(481, 482);
            undoable = false;
            notifyCurrent();
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        if (!undoable) {
            swapCurrent(498, 499);
            undoable = true;
            notifyCurrent();
        }
    }
    ActionText describe() override { return describeWithKey("%s->%s changed"); }
    ActionText describeUndo() override { return describeWithKey("%s->%s change cancelled"); }
    ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    bool merge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        return canMerge(std::move(other));
    }
    bool canMerge(ActionPtr other) override {
        const auto change = std::dynamic_pointer_cast<TSelf>(std::move(other));
        return change && name == change->name && pointx == change->pointx && pointy == change->pointy;
    }

protected:
    ParameterBindingValueChangeAction() {
        detail::prepareBindingActionClass<TSelf, T, TBinding>("ParameterBindingValueChangeAction");
    }
    void initialize(compat::RetainedStringSlice label, TBinding bindings, int x, int y,
        BindingUpdate update = {}, bool notify = true) {
        name = std::move(label);
        self = bindings;
        pointx = x;
        pointy = y;
        auto valuesSource = bindings;
        nicxlive::core::common::SharedArray<T> capturedValues;
        if (!valuesSource.empty()) {
            // Phobos array reserves the result before reading the lazy map,
            // then constructs each result cell without default-constructing T.
            if constexpr (std::same_as<T, bool>) {
                capturedValues = detail::bindingFlags(valuesSource.size());
            } else {
                auto storage = detail::reserveBindingValues<T>(valuesSource.size());
                capturedValues = nicxlive::core::common::SharedArray<T>(std::move(storage));
            }
        }
        std::size_t valueIndex = 0;
        for (auto binding : valuesSource) {
            const auto previous = detail::cloneActionValue(
                detail::bindingArrayAt(detail::bindingArrayAt(
                    compat::ngRequireObject(binding)->values, index(x), 440), index(y), 440));
            if constexpr (std::same_as<T, bool>) capturedValues[valueIndex++] = previous;
            else capturedValues.push_back(previous);
        }
        value = capturedValues;
        auto flagsSource = bindings;
        auto capturedFlags = detail::bindingFlags(flagsSource.size());
        std::size_t flagIndex = 0;
        for (auto binding : flagsSource) {
            detail::bindingArrayAt(capturedFlags, flagIndex, 441) = detail::bindingArrayAt(
                detail::bindingArrayAt(compat::ngRequireObject(binding)->BaseBinding::getIsSet(), index(x), 441),
                    index(y), 441);
            ++flagIndex;
        }
        isSet = capturedFlags;
        undoable = true;
        _dirty = false;
        notifyDepthBone = notify;
        if (update) {
            update();
            updateNewState();
        }
    }
    void initialize(int label, TBinding bindings, int x, int y, BindingUpdate update = {}, bool notify = true) {
        name = detail::bindingIntegerName(label);
        initialize(name, bindings, x, y, update, notify);
    }
private:
    static std::size_t index(int value) { return static_cast<std::size_t>(value); }
    void swapCurrent(std::size_t valueLine, std::size_t flagLine) {
        const auto count = self.size();
        for (std::size_t i = 0; i < count; ++i) {
            {
                const auto receiver = compat::ngRequireObject(detail::bindingArrayAt(self, i, valueLine));
                const auto rows = receiver->values;
                auto row = detail::bindingArrayAt(rows, index(pointx), valueLine);
                auto& current = detail::bindingArrayAt(row, index(pointy), valueLine);
                auto previousValues = value;
                auto& previous = detail::bindingArrayAt(previousValues, i, valueLine);
                detail::swapBindingValue(current, previous);
            }
            {
                const auto receiver = compat::ngRequireObject(detail::bindingArrayAt(self, i, flagLine));
                const auto rows = receiver->BaseBinding::getIsSet();
                auto row = detail::bindingArrayAt(rows, index(pointx), flagLine);
                auto& currentIsSet = detail::bindingArrayAt(row, index(pointy), flagLine);
                auto previousFlags = isSet;
                auto& previousIsSet = detail::bindingArrayAt(previousFlags, i, flagLine);
                std::swap(currentIsSet, previousIsSet);
            }
            const auto receiver = compat::ngRequireObject(detail::bindingArrayAt(self, i, flagLine + 1));
            receiver->reInterpolate();
        }
    }
    void notifyCurrent() {
        auto snapshot = self;
        for (auto binding : snapshot)
            detail::notifyBindingValueChanged(std::move(binding), pointx, pointy, notifyDepthBone);
    }
    template<std::size_t N> ActionText describeWithKey(const char (&key)[N]) {
        auto owner = this->weak_from_this().lock();
        const auto snapshot = self;
        if (snapshot.empty()) return {};
        std::string joined;
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            // The source lazy map is read only after join appends its separator.
            if (i) detail::appendBindingText(joined, "\n");
            const auto text = detail::describeBindingValue(key, snapshot[i], name);
            detail::appendBindingText(joined, text.view());
        }
        if (joined.empty()) return {};
        return detail::retainBindingText(std::move(joined));
    }
};

template<class I, class... Args> std::shared_ptr<I> ngCreateBindingValueAction(Args&&... args) {
    nicxlive::core::d_runtime::SourceAllocator<I> allocator;
    auto* storage = allocator.allocate(1);
    try {
        ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    auto action = std::shared_ptr<I>(storage, [allocator](I* object) mutable {
        std::destroy_at(object);
        allocator.deallocate(object, 1);
    }, allocator);
    detail::initializeBindingAction([&](auto&&... values) {
        action->initialize(std::forward<decltype(values)>(values)...);
    },std::forward<Args>(args)...);
    return action;
}

} // namespace nijigenerate::actions::binding

namespace nicxlive::core::d_runtime {
template<class T, class Binding> struct SourceObjectTypeNames<
        ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<T, Binding>> {
    static RetainedString className() {
        return ::nijigenerate::actions::binding::detail::bindingActionClassName<T,Binding>(
            "ParameterBindingValueChangeAction");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::actions::binding::detail::bindingActionTypeSpellings<T,Binding>(
            "ParameterBindingValueChangeAction").qualifiedArgument;
    }
};

template<class T, class Binding> struct SourceTypeSpellings<
        ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<T, Binding>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::binding::detail::bindingActionTypeSpellings<T, Binding>(
            "ParameterBindingValueChangeAction");
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<class T, class Binding>
    requires std::derived_from<Binding, nicxlive::core::param::ParameterBindingImpl<T>>
struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<T, Binding>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<T, Binding>>> {};
template<class T, class Binding>
    requires std::derived_from<Binding, nicxlive::core::param::ParameterBindingImpl<T>>
struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<
        T, compat::Slice<std::shared_ptr<Binding>>>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::binding::ParameterBindingValueChangeAction<
            T, compat::Slice<std::shared_ptr<Binding>>>>> {};

} // namespace nijigenerate::core::translation
