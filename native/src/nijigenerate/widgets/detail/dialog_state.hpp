#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/widgets/detail/dialog_entry.hpp"

#include <memory>

namespace nicxlive::core { class Texture; }

namespace nijigenerate::widgets::dialog::detail {

// Internal module state, separate for each calling thread; not an application queue API.
extern thread_local compat::Slice<DialogEntry> entries;
extern thread_local compat::Slice<DialogHandlerRef> dialogHandlers;
extern thread_local compat::Slice<std::shared_ptr<nicxlive::core::Texture>> adaTextures;

// Inline slice retention mirrors a managed interior pointer without allocating at lookup.
class DialogEntryReference {
public:
    DialogEntryReference() = default;
    DialogEntryReference(const DialogEntryReference&) = default;
    DialogEntryReference& operator=(const DialogEntryReference&) = default;
    DialogEntryReference(const compat::Slice<DialogEntry>& values, std::size_t index)
        : backing_(values), pointer_(&backing_[index]) {}

    DialogEntry* get() const noexcept { return pointer_; }
    bool isNull() const noexcept { return pointer_ == nullptr; }

private:
    compat::Slice<DialogEntry> backing_;
    DialogEntry* pointer_ = nullptr;
};

DialogEntryReference findDialogEntry(const char* tag);

} // namespace nijigenerate::widgets::dialog::detail
