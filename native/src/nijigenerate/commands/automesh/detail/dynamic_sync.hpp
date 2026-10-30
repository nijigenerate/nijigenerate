#pragma once
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <compat/source_mutex.hpp>
#include <memory>
#include <utility>

namespace nijigenerate::commands::automesh::dynamic::detail {
template<class Body> void synchronizedMonitor(
        std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> mutex, Body&& body) {
    const auto monitor = compat::ngRequireObject(std::move(mutex));
    monitor->lock();
    compat::ngRunWithScopeExit(std::forward<Body>(body), [monitor] { monitor->unlock(); });
}
}
