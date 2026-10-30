#pragma once

// Whole binding action module. Keep the private clone/storage dependency
// acyclic: binding.hpp remains the generic clone declaration used by storage.
#include "binding_add_remove_action.hpp"
#include "binding_interpolation_action.hpp"
#include "binding_all_value_action.hpp"
#include "binding_value_action.hpp"
