#include <nijigenerate/widgets/modal/base.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets::modal {
namespace {
compat::Slice<ModalReference> modalList;
thread_local std::ptrdiff_t modalIndex = -1;

std::ptrdiff_t nextModalIndex() {
    return std::bit_cast<std::ptrdiff_t>(static_cast<std::uintptr_t>(modalIndex) + std::uintptr_t{1});
}

ModalReference& modalAt(std::size_t index, std::size_t line) {
    if (index >= modalList.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, modalList.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/modal/package.d"), line);
    return modalList[index];
}
}

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Modal::prepareSourceClassInfo() {
    registerSourceClassInfo<Modal>("nijigenerate.widgets.modal.Modal");
}

compat::RetainedString Modal::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t Modal::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool Modal::opEquals(const ModalReference& other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int Modal::opCmp(ModalReference) {
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void Modal::onBeginUpdate() {
    if (!imTitle) {
        compat::SourceCString next(title_.view());
        compat::detail::ngSourceCStringPostconditions(title_.view(), next.data());
        imTitleOwner.emplace(std::move(next));
        imTitle = imTitleOwner->data();
    }
    if (visible && !ImGui::IsPopupOpen(imTitle)) {
        ImGui::OpenPopup(imTitle);
    }
    drewWindow = ImGui::BeginPopupModal(imTitle, &visible,
        hasTitlebar ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration);
}

void Modal::onEndUpdate() {
    if (drewWindow) ImGui::EndPopup();
    if (!visible) ngModalCloseTop();
}

compat::RetainedString Modal::title() {
    return title_;
}

Modal::Modal(compat::RetainedString name, bool hasTitlebar)
    : title_(std::move(name)), visible(true), hasTitlebar(hasTitlebar) {}

void Modal::update() {
    onBeginUpdate();
    if (drewWindow) onUpdate();
    onEndUpdate();
}

void ngModalRender() {
    if (modalIndex > -1) {
        auto modal = modalAt(static_cast<std::size_t>(modalIndex), 97);
        compat::ngRequireObject(modal)->update();
    }
}

bool ngModalIsOpen() {
    return modalIndex > -1;
}

void ngModalAdd(ModalReference modal) {
    if (static_cast<std::size_t>(nextModalIndex()) >= modalList.size()) {
        modalList.resize(modalList.size() + 1);
    }
    modalIndex = nextModalIndex();
    modalAt(static_cast<std::size_t>(modalIndex), 116) = std::move(modal);
}

void ngModalCloseTop() {
    if (modalIndex >= 0) --modalIndex;
}

} // namespace nijigenerate::widgets::modal
