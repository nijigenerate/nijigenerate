#pragma once

#include "nijigenerate/widgets/detail/dialog_values.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace nijigenerate::widgets::dialog {

enum class DialogLevel : std::size_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

enum class DialogButtons : std::int32_t {
    NONE = 0,
    OK = 1,
    Cancel = 2,
    Yes = 4,
    No = 8,
};

constexpr DialogButtons operator&(DialogButtons left, DialogButtons right) noexcept {
    return static_cast<DialogButtons>(static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right));
}

constexpr DialogButtons operator|(DialogButtons left, DialogButtons right) noexcept {
    return static_cast<DialogButtons>(static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
}

constexpr DialogButtons operator^(DialogButtons left, DialogButtons right) noexcept {
    return static_cast<DialogButtons>(static_cast<std::int32_t>(left) ^ static_cast<std::int32_t>(right));
}

constexpr DialogButtons operator~(DialogButtons value) noexcept {
    return static_cast<DialogButtons>(std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(~static_cast<std::uint32_t>(value))));
}

constexpr DialogButtons& operator&=(DialogButtons& left, DialogButtons right) noexcept {
    return left = left & right;
}

constexpr DialogButtons& operator|=(DialogButtons& left, DialogButtons right) noexcept {
    return left = left | right;
}

constexpr DialogButtons& operator^=(DialogButtons& left, DialogButtons right) noexcept {
    return left = left ^ right;
}

using DialogTag = detail::RetainedPointer<const char>;
using DialogText = detail::DialogText;
using DialogUserData = detail::RetainedPointer<void>;

class DialogHandler : public std::enable_shared_from_this<DialogHandler> {
public:
    DialogHandler() = delete;
    explicit DialogHandler(DialogTag tag);
    DialogHandler(const DialogHandler&) = delete;
    DialogHandler& operator=(const DialogHandler&) = delete;
    virtual ~DialogHandler() = default;
    virtual bool hasClicked();
    virtual bool onClick(DialogButtons button);
    virtual bool onClickCancel();
    virtual bool onClickYes();
    virtual bool onClickNo();
    virtual void registerHandler();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DialogHandler> other);
    virtual int opCmp(std::shared_ptr<DialogHandler> other);

protected:
    virtual void prepareSourceClassInfo();

private:
    friend void ngHandleDialogHandlers();
    DialogTag tag_;
};

using DialogHandlerRef = std::shared_ptr<DialogHandler>;
bool ngDialogHandlerObjectEquals(DialogHandlerRef left, DialogHandlerRef right);

void ngInitDialogs();
void ngRenderDialogs();
void ngDialog(DialogTag tag, DialogTag title, DialogText body,
              DialogLevel level = DialogLevel::Error, DialogButtons buttons = DialogButtons::OK,
              DialogUserData userData = {});
void ngDialog(DialogTag title, DialogText body, DialogLevel level = DialogLevel::Error,
              DialogButtons buttons = DialogButtons::OK, DialogUserData userData = {});
void ngCleanupDialogs();
DialogButtons ngDialogButtonSelected(const char* tag);
DialogUserData ngDialogButtonUserData(const char* tag);
void ngRegisterDialogHandler(DialogHandlerRef handler);
void ngHandleDialogHandlers();

} // namespace nijigenerate::widgets::dialog

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::dialog::DialogLevel> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.dialog.DialogLevel)";
    inline static constexpr char unqualifiedArgument[] = "(DialogLevel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.dialog.DialogLevel";
    inline static constexpr char unqualifiedElement[] = "DialogLevel";
};

template<> struct SourceTypeSpellings<::nijigenerate::widgets::dialog::DialogButtons> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.dialog.DialogButtons)";
    inline static constexpr char unqualifiedArgument[] = "(DialogButtons)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.dialog.DialogButtons";
    inline static constexpr char unqualifiedElement[] = "DialogButtons";
};

template<> struct SourceTypeSpellings<::nijigenerate::widgets::dialog::DialogHandler> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.dialog.DialogHandler)";
    inline static constexpr char unqualifiedArgument[] = "(DialogHandler)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.dialog.DialogHandler";
    inline static constexpr char unqualifiedElement[] = "DialogHandler";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::dialog::DialogHandler> {
    inline static constexpr char className[] = "nijigenerate.widgets.dialog.DialogHandler";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.dialog.DialogHandler)";
};
}
