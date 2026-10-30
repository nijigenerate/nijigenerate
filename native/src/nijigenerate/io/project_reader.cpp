#include "project_reader.hpp"
#include <compat/phobos_file_read.hpp>

namespace nijigenerate::io::project_reader {
[[noreturn]] void ngThrowProjectFileError(compat::RetainedStringSlice path, std::uint32_t code,
        std::source_location origin) {
    throw compat::FileException(path.retainedString(), code, origin);
}
compat::Slice<std::uint8_t> ngReadProjectFile(compat::RetainedStringSlice path) {
    auto bytes = ::nicxlive::core::d_runtime::ngReadBinaryFile(path.retainedString());
    return compat::Slice<std::uint8_t>::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
}
}
