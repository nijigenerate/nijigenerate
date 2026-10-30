#include "contracts.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <string_view>
namespace nijigenerate::autorig::deterministic::contracts {
void ngRigCheckpoint(std::shared_ptr<framework::AutoRigTaskContext> context) {
    auto check = [&](std::size_t line) {
        if (context && context->isCanceled())
            throw compat::ThrowableException(detail::copyContractMessage("Automatic rig canceled"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), line);
    };
    check(106);
    if (core::tasks::TaskFiber::hasCurrent()) core::tasks::TaskFiber::yield();
    check(108);
}
}
