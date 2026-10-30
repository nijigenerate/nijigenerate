#include "detail/dynamic_progress.hpp"
#include "detail/dynamic_sync.hpp"
#include <compat/source_allocator.hpp>
#include <common/object_key_policy.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::commands::automesh::dynamic::detail {

void AutoMeshProgressState::prepareSourceClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<AutoMeshProgressState>;
        ngRegisterObjectTypeNames<AutoMeshProgressState>(
            compat::RetainedString::fromStaticLiteral(Names::className),
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(AutoMeshProgressState),
            compat::RetainedString::fromStaticLiteral(Names::className));
        return true;
    }();
    (void)registered;
}
compat::RetainedString AutoMeshProgressState::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AutoMeshProgressState::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool AutoMeshProgressState::opEquals(std::shared_ptr<AutoMeshProgressState> other) {
    return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
}
int AutoMeshProgressState::opCmp(std::shared_ptr<AutoMeshProgressState>) {
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try { message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view())); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message), compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

AutoMeshProgressState::AutoMeshProgressState(std::size_t totalValue, ParentCanceled parentCanceledValue) {
    lock = std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
    total = totalValue;
    parentCanceled = parentCanceledValue;
}

void AutoMeshProgressState::beginTarget(compat::RetainedStringSlice name) {
    synchronizedMonitor(lock, [&] { currentName_ = name; });
}

void AutoMeshProgressState::completeTarget() {
    synchronizedMonitor(lock, [&] { if (done_ < total) ++done_; });
}

void AutoMeshProgressState::requestCancel() {
    synchronizedMonitor(lock, [&] { canceled_ = true; });
}

void AutoMeshProgressState::snapshot(std::size_t& done, std::size_t& totalOut,
    compat::RetainedStringSlice& currentName, bool& canceledOut) {
    // D out arguments are reset before entering the synchronized body.
    done = 0;
    totalOut = 0;
    currentName = {};
    canceledOut = false;
    synchronizedMonitor(lock, [&] {
        done = done_;
        totalOut = total;
        currentName = currentName_;
        canceledOut = canceled_;
    });
}

bool AutoMeshProgressState::canceled() {
    if (parentCanceled && parentCanceled()) requestCancel();
    bool result = false;
    synchronizedMonitor(lock, [&] { result = canceled_; });
    return result;
}

} // namespace nijigenerate::commands::automesh::dynamic::detail
