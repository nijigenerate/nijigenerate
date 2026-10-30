#include "autorig_framework.hpp"
#include <nijigenerate/core/thread_context.hpp>
#include <SDL3/SDL_main.h>

int main(int count, char** values) {
    nijigenerate::core::thread_context::ngRegisterMainThread();
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args;
    for (int i = 0; i < count; ++i) args.append(nijigenerate::compat::RetainedString(values[i]));
    autorig_framework_test::ngMain(args);
}
