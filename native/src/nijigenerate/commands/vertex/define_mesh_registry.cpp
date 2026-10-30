#include "define_mesh.hpp"

namespace nijigenerate::commands::vertex::define_mesh {

thread_local CommandTable commands;

template<class T> requires (std::same_as<T, VertexCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() {
    initialization::Program::run<initialization::Table>();
}

template void ngInitCommands<VertexCommand>();

} // namespace nijigenerate::commands::vertex::define_mesh
