#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/autosave_directory_entry.hpp>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <compat/object_metadata.hpp>

namespace nijigenerate::io::autosave {

using AutosaveString = compat::RetainedStringSlice;

// Definitions belong to their declaration/provider implementation gates.
struct AutosaveRecord {
    AutosaveString autosavePath;
    AutosaveString mainsavePath;
};
using AutosaveRecords = compat::Slice<AutosaveRecord>;
using AutosaveDirectoryEntries = compat::Slice<AutosaveDirectoryEntry>;

void startAutosaveTimer();
std::int32_t incGetAutosaveInterval();
void incSetAutosaveInterval(std::int32_t interval);
std::int32_t incGetAutosaveFileLimit();
void incSetAutosaveFileLimit(std::int32_t fileLimit);
bool incGetAutosaveEnabled();
void incSetAutosaveEnabled(bool enabled);
AutosaveRecords incGetPrevAutosaves();
void incAddPrevAutosave(AutosaveString autosavePath);
void incCheckAutosave();
void incAutosaveProject(AutosaveString path);
void incPruneAutosaveList();
AutosaveString bakStampString();
AutosaveString getAutosaveDir(AutosaveString projectPath);
AutosaveDirectoryEntries currentBackups(AutosaveString projectAutosaveDir);
void incCreateLockfile(AutosaveString projectPath);
void incReleaseLockfile();
bool incCheckLockfile(AutosaveString projectPath);

} // namespace nijigenerate::io::autosave

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::autosave::AutosaveRecord> {
    using Value = ::nijigenerate::io::autosave::AutosaveRecord;
    inline static constexpr std::string_view typeName = "AutosaveRecord";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("autosavePath", offsetof(Value, autosavePath), &Value::autosavePath),
            ngFormatAggregateMember("mainsavePath", offsetof(Value, mainsavePath), &Value::mainsavePath)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::autosave::AutosaveRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.autosave.AutosaveRecord)";
    inline static constexpr char unqualifiedArgument[] = "(AutosaveRecord)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.autosave.AutosaveRecord";
    inline static constexpr char unqualifiedElement[] = "AutosaveRecord";
};
}
