#pragma once
#include "base.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
namespace nijigenerate::windows::autosave {
class RestoreSaveWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    compat::RetainedStringSlice projectPath;
    void initialize(compat::RetainedStringSlice path);
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    RestoreSaveWindow() = default;
};
}
