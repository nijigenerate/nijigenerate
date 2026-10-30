// Mir fuseShape: Copyright 2020 Ilia Ki, Kaleidic Associates Advisory Limited,
// Symmetry Investments. Apache-2.0; see port/licenses/mir-iteration-any-LICENSE.txt.
#include "throwable.hpp"
#include "retained_string.hpp"
#include <cstddef>
#include <memory>
#include <new>

namespace nijigenerate::compat::mir::detail {
namespace {
using nicxlive::core::d_runtime::SourceExceptionViewTag;

constexpr char shapeExceptionMessage[] = "fuseShape Exception: elements have different shapes/lengths";

class FuseShapeExceptionView final : public ThrowableException {
public:
    FuseShapeExceptionView()
        : ThrowableException(SourceExceptionViewTag{}, RetainedString::fromStaticLiteral(shapeExceptionMessage),
            RetainedString::fromStaticLiteral("mir/ndslice/fuse.d"), 319) {}
    FuseShapeExceptionView(const FuseShapeExceptionView& source) noexcept
        : ThrowableException(SourceExceptionViewTag{}, source) {}
    const char* what() const noexcept override { return shapeExceptionMessage; }
};

FuseShapeExceptionView& sourceShapeException() {
    // Static native transport storage adds no heap owner or Source object.
    // Keep its one canonical State alive through all other static finalizers.
    alignas(FuseShapeExceptionView) static std::byte storage[sizeof(FuseShapeExceptionView)];
    static auto* const instance = std::construct_at(reinterpret_cast<FuseShapeExceptionView*>(storage));
    return *instance;
}

// The Source module owns this one Exception. toMutable drops its qualifier;
// each throw retains that same State rather than constructing another object.
[[maybe_unused]] const bool shapeExceptionInitialized = [] {
    (void)sourceShapeException();
    return true;
}();
}

[[noreturn]] void throwFuseShapeException() {
    throw FuseShapeExceptionView(sourceShapeException());
}
} // namespace nijigenerate::compat::mir::detail
