#pragma once
#include "base.hpp"
#include <nijigenerate/widgets/inputtext.hpp>

namespace nicxlive::core::common { class NullableString; }

namespace nijigenerate::windows::rename {
class RenameWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    compat::RetainedStringSlice text;
    // Source ref string points to the caller's descriptor, not its byte buffer.
    compat::RetainedString* outputDescriptor = nullptr;
    // An aliasing shared_ptr may address a descriptor inside its real owning object.
    std::shared_ptr<compat::RetainedString> output;
    std::shared_ptr<std::string> stringOutput;
    nicxlive::core::common::NullableString* sdkOutputDescriptor = nullptr;
    std::shared_ptr<nicxlive::core::common::NullableString> sdkOutput;
    void initialize(compat::RetainedString& destination);
    void initialize(std::shared_ptr<compat::RetainedString> destination);
    void initialize(std::shared_ptr<std::string> destination);
    void initialize(nicxlive::core::common::NullableString& destination);
    void initialize(std::shared_ptr<nicxlive::core::common::NullableString> destination);
    void apply();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    RenameWindow() = default;
};
}
