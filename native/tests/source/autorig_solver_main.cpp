#include "autorig_solver.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <filesystem>

#if defined(_WIN32)
int wmain(int count, wchar_t** values) {
#else
int main(int count, char** values) {
#endif
    using namespace nijigenerate;
    compat::Slice<compat::RetainedString> arguments;
    for (int index = 0; index < count; ++index) {
#if defined(_WIN32)
        arguments.append(compat::RetainedString(compat::ngPathToUtf8(std::filesystem::path(values[index]))));
#else
        arguments.append(compat::RetainedString(values[index]));
#endif
    }
    autorig_solver::ngMain(std::move(arguments));
    return 0;
}
