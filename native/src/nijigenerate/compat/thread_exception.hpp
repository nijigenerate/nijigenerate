#pragma once
// Compatibility names for the single canonical SDK thread error provider.
#include "throwable.hpp"
#include <compat/thread_exception.hpp>
namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::ThreadException;
using ::nicxlive::core::d_runtime::ThreadError;
using ::nicxlive::core::d_runtime::SuppressTraceInfo;
using ::nicxlive::core::d_runtime::ngOnThreadError;
using ::nicxlive::core::d_runtime::ngThrowThreadException;
}
