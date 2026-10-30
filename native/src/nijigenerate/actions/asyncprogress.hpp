#pragma once
#include "package.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <tuple>
#include <utility>

namespace nijigenerate::actions::asyncprogress {
class AsyncGroupActionProgress : public std::enable_shared_from_this<AsyncGroupActionProgress> {
    template<class T> struct ProgressAllocator : nicxlive::core::d_runtime::SourceAllocator<T> {
        template<class U> struct rebind { using other = ProgressAllocator<U>; };
        ProgressAllocator() noexcept = default;
        template<class U> ProgressAllocator(const ProgressAllocator<U>&) noexcept {}
        template<class U, class... Args> void construct(U* pointer, Args&&... arguments) {
            auto* memory = const_cast<void*>(static_cast<const volatile void*>(pointer));
            ::new (memory) U(std::forward<Args>(arguments)...);
        }
        template<class U> bool operator==(const ProgressAllocator<U>&) const noexcept { return true; }
    };
public:
    using PumpHandler = api::mcp::task::MainThreadAction<void>;
    using WorkCountHandler = api::mcp::task::MainThreadAction<std::size_t>;
    static std::shared_ptr<AsyncGroupActionProgress> create(std::shared_ptr<AsyncGroupAction> action,
        compat::RetainedString title, compat::RetainedString completedMessage = {},
        compat::RetainedString canceledMessage = {}, WorkCountHandler workCount = {}, PumpHandler pump = {});
    // Derived source constructors use logical initialize, as other retained factories do.
    // A private derived initialize must friend this class; its native ctor initializes fields only.
    template<class Derived, class... Args>
    static std::shared_ptr<Derived> ngCreateDerived(Args&&... args) {
        static_assert(std::is_base_of_v<AsyncGroupActionProgress, Derived>);
        auto receiver = std::allocate_shared<Derived>(ProgressAllocator<Derived>{});
        receiver->initialize(std::forward<Args>(args)...);
        return receiver;
    }
    template<class Derived = AsyncGroupActionProgress, class BuildArguments>
    static std::shared_ptr<Derived> ngCreateDerivedSource(BuildArguments&& buildArguments) {
        static_assert(std::is_base_of_v<AsyncGroupActionProgress, Derived>);
        auto receiver = std::allocate_shared<Derived>(ProgressAllocator<Derived>{});
        auto arguments = std::forward<BuildArguments>(buildArguments)();
        std::apply([&](auto&&... values) { receiver->initialize(values...); }, arguments);
        return receiver;
    }
    virtual ~AsyncGroupActionProgress() = default;
    virtual bool isActive() const;
    virtual bool isVisible() const;
    virtual void dispose();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AsyncGroupActionProgress> other);
    virtual int opCmp(std::shared_ptr<AsyncGroupActionProgress> other);
private:
    using std::enable_shared_from_this<AsyncGroupActionProgress>::shared_from_this;
    std::shared_ptr<AsyncGroupAction> action_;
    compat::RetainedString title_, completedMessage_, canceledMessage_;
    WorkCountHandler workCount_;
    PumpHandler pump_;
    std::uint64_t observerId_ = 0, popupId_ = 0, popupGeneration_ = 0, notifiedCompletionGeneration_ = 0;
    std::size_t observedTotal_ = 0;
    bool pumpQueued_ = false, disposed_ = false;
    void onActionEvent(std::shared_ptr<AsyncGroupAction> source, AsyncGroupActionEvent event);
    void activate();
    void draw();
    void schedulePump();
    void pumpStep(std::uint64_t generation);
    void complete();
    void cancel();
    void closePopup();
    std::size_t remainingWork() const;
protected:
    AsyncGroupActionProgress() = default;
    void initialize(std::shared_ptr<AsyncGroupAction> action, compat::RetainedString title,
        compat::RetainedString completedMessage = {}, compat::RetainedString canceledMessage = {},
        WorkCountHandler workCount = {}, PumpHandler pump = {});
};
bool ngAsyncProgressObjectEquals(std::shared_ptr<AsyncGroupActionProgress> left,
    std::shared_ptr<AsyncGroupActionProgress> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::asyncprogress::AsyncGroupActionProgress> {
    inline static constexpr char className[] = "nijigenerate.actions.asyncprogress.AsyncGroupActionProgress";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.actions.asyncprogress.AsyncGroupActionProgress)";
};
template<> struct SourceTypeSpellings<::nijigenerate::actions::asyncprogress::AsyncGroupActionProgress> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.actions.asyncprogress.AsyncGroupActionProgress)";
    inline static constexpr char unqualifiedArgument[] = "(AsyncGroupActionProgress)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.actions.asyncprogress.AsyncGroupActionProgress";
    inline static constexpr char unqualifiedElement[] = "AsyncGroupActionProgress";
};
}

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::asyncprogress::AsyncGroupActionProgress>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::asyncprogress::AsyncGroupActionProgress>> {};

} // namespace nijigenerate::core::translation
