#pragma once
#include <nijigenerate/renderer_config.hpp>
#ifdef NIJIGENERATE_RENDERER_METAL
#include "metal/backend.hpp"
#include "metal/compute.hpp"
namespace nijigenerate::backend { namespace selected = metal; }
#else
#include "vulkan/backend.hpp"
#include "vulkan/compute.hpp"
namespace nijigenerate::backend { namespace selected = vulkan; }
#endif
