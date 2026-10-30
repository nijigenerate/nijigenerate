#include <nijigenerate/widgets/inputtext_internal.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
namespace nijigenerate::widgets::inputtext::detail {
namespace {
int resizeCallback(ImGuiInputTextCallbackData* data) {
    auto* user=static_cast<TextCallbackUserData*>(compat::ngRequireObject(data)->UserData);
    if (data->EventFlag==ImGuiInputTextFlags_CallbackResize) {
        if (data->BufTextLen<0) data->BufTextLen=0;
        auto& buffer=*compat::ngRequireObject(compat::ngRequireObject(user)->buf);
        const auto previous=buffer.size();
        const auto wrapped=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(data->BufTextLen)+1u);
        const auto length=static_cast<std::size_t>(wrapped);
        try {
            buffer.resize(length);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        // D char.init is 0xFF; SharedArray's generic growth uses char{}, so adapt here.
        for (auto i=previous;i<length;++i) buffer[i]=static_cast<char>(0xff);
        data->Buf=buffer.data();
        data->Buf[data->BufTextLen]='\0';
    }
    return 0;
}
}
extern "C" int ngInputTextResizeCallback(ImGuiInputTextCallbackData* data) { return resizeCallback(data); }
extern "C" int ngLabeledInputTextResizeCallback(ImGuiInputTextCallbackData* data) { return resizeCallback(data); }
extern "C" int ngMultilineInputTextResizeCallback(ImGuiInputTextCallbackData* data) { return resizeCallback(data); }
}
