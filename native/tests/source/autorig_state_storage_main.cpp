#include "autorig_state_storage.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <vector>
#include <utility>
#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    nijigenerate::core::thread_context::ngRegisterMainThread();
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
    autorig_state_storage_test::ngMain(nijigenerate::compat::Slice<String>(std::move(arguments)));
    return 0;
}
