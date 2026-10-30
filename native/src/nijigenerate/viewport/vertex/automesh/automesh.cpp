// Whole-file port of source/nijigenerate/viewport/vertex/automesh/automesh.d.
#include "automesh.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <algorithm>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh {

void AutoMeshProcessor::prepareSourceClassInfo() {
    using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<AutoMeshProcessor>;
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectTypeNames<AutoMeshProcessor>(
            compat::RetainedString::fromStaticLiteral(Names::className),
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        return true;
    }();
    (void)registered;
    registerSourceClassInfo<AutoMeshProcessor>(Names::className);
}

bool AutoMeshProcessor::opEquals(std::shared_ptr<AutoMeshProcessor> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

bool ngAutoMeshProcessorObjectEquals(std::shared_ptr<AutoMeshProcessor> left,
    std::shared_ptr<AutoMeshProcessor> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

compat::RetainedString AutoMeshProcessor::procId() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto dot = name.view().find_last_of('.');
    return dot == std::string_view::npos ? name : name.subslice(dot + 1, name.size());
}
compat::RetainedString AutoMeshProcessor::displayName() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    return procId();
}
int AutoMeshProcessor::order() { return 0; }

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString AutoMeshProcessor::toString() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AutoMeshProcessor::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int AutoMeshProcessor::opCmp(std::shared_ptr<AutoMeshProcessor>) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

compat::Slice<AutoMeshProcessor::Mesh> AutoMeshProcessor::autoMesh(
    compat::Slice<Target> targets, compat::Slice<Mesh> meshData,
    bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert, Callback callback) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    compat::Slice<Mesh> result;
    // D zip's default stopping policy is the shortest input range.
    const auto count = std::min(targets.size(), meshData.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (callback && !callback(targets[i], {})) return result;
        // Fetch again after the callback: shallow aliases may replace array entries.
        Mesh generated = autoMesh(targets[i], meshData[i], mirrorHoriz, axisHoriz, mirrorVert, axisVert);
        result.append(generated);
        if (callback && !callback(targets[i], generated)) return result;
    }
    return result;
}

} // namespace nijigenerate::viewport::vertex::automesh
