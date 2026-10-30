#pragma once
#include <nijigenerate/compat/retained_string.hpp>
namespace nicxlive::core::nodes { class Drawable; }
namespace nijigenerate::commands::vertex::define_mesh::detail {
bool canApplyMeshTopology(nicxlive::core::nodes::Drawable* target, compat::RetainedString& reason);
}
