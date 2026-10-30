#pragma once
#include <puppet.hpp>
#include <cstdint>
#include <vector>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::io {
// Independent nijilive fmt/package.d INP memory writer using the typed serializer.
std::vector<std::uint8_t> ngWriteProjectPuppetMemory(const nicxlive::core::Puppet& puppet);
std::vector<std::uint8_t> ngWriteProjectPuppetMemory(nicxlive::core::Puppet& puppet);
void ngWriteProjectPuppetFile(const nicxlive::core::Puppet& puppet, compat::RetainedStringSlice path);
void ngRenameProjectFile(compat::RetainedStringSlice from, compat::RetainedStringSlice to);
}
