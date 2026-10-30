#include "autorig_json_storage.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <filesystem>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    using Text = nijigenerate::compat::RetainedString;
    nijigenerate::compat::Slice<Text> arguments;
    for (int index = 0; index < argc; ++index) {
        Text argument;
        try {
#if defined(_WIN32)
            argument = Text(nijigenerate::compat::ngPathToUtf8(std::filesystem::path(argv[index])));
#else
            argument = Text(argv[index]);
#endif
        } catch (const std::bad_alloc&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
                std::source_location::current());
        }
        arguments.append(std::move(argument));
    }
    autorig_json_storage_test::ngMain(std::move(arguments));
    return 0;
}
