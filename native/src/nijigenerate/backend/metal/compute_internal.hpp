#pragma once
#include "context_internal.hpp"
namespace nijigenerate::backend::metal {
id<MTLComputePipelineState> ngComputePipeline(const char* name,const char* source);
}
