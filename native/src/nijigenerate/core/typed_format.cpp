#include "typed_format.hpp"

namespace nijigenerate::core::translation {

std::string ngFormatTyped(std::string_view pattern, std::span<const TypedFormatArgument> arguments) {
    return detail::formatTypedBody(pattern, arguments);
}
void ngFormatTypedTo(const TypedFormatSink& sink, std::string_view pattern,
        std::span<const TypedFormatArgument> arguments) {
    detail::formatTypedTo(sink, pattern, arguments);
}

} // namespace nijigenerate::core::translation
