#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <compat/object_metadata.hpp>
#include <imgui.h>
#include <cstddef>
#include <memory>
#include <optional>

namespace nijigenerate::widgets::modal {

class Modal;
using ModalReference = std::shared_ptr<Modal>;

class Modal {
private:
    compat::RetainedString title_;
    std::optional<compat::SourceCString> imTitleOwner;
    const char* imTitle = nullptr;
    bool visible = false;
    bool hasTitlebar = false;

protected:
    bool drewWindow = false;
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;

    virtual void onUpdate() = 0;
    virtual void onBeginUpdate();
    virtual void onEndUpdate();
    virtual compat::RetainedString title();

public:
    Modal(compat::RetainedString name, bool hasTitlebar);
    virtual ~Modal() = default;
    Modal(const Modal&) = delete;
    Modal& operator=(const Modal&) = delete;
    Modal(Modal&&) = delete;
    Modal& operator=(Modal&&) = delete;
    virtual void update() final;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(const ModalReference& other);
    virtual int opCmp(ModalReference other);

protected:
    template<class T, std::size_t N> static void registerSourceClassInfo(const char (&name)[N]) {
        static const bool registered = [&name] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                compat::RetainedString::fromStaticLiteral(name));
            return true;
        }();
        (void)registered;
    }
    virtual void prepareSourceClassInfo();
};

void ngModalRender();
bool ngModalIsOpen();
void ngModalAdd(ModalReference modal);
void ngModalCloseTop();

} // namespace nijigenerate::widgets::modal

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::modal::Modal> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.modal.Modal)";
    inline static constexpr char unqualifiedArgument[] = "(Modal)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.modal.Modal";
    inline static constexpr char unqualifiedElement[] = "Modal";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::modal::Modal> {
    inline static constexpr char className[] = "nijigenerate.widgets.modal.Modal";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.modal.Modal)";
};
}
