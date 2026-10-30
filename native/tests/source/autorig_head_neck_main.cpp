#include "autorig_head_neck.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <vector>
#include <utility>

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    using String = nijigenerate::compat::RetainedString;
    std::vector<String> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
#if defined(_WIN32)
        arguments.emplace_back(nijigenerate::compat::ngPathToUtf8(std::filesystem::path(argv[index])));
#else
        arguments.emplace_back(argv[index]);
#endif
    }
    autorig_head_neck_test::ngMain(nijigenerate::compat::Slice<String>(std::move(arguments)));
    return 0;
}
