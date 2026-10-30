#pragma once
#include "base.hpp"
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <common/shared_array.hpp>
#include <vector>
#include <memory>

namespace nijigenerate::windows::imgexport {
class ImageExportWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    compat::RetainedStringSlice outFile;
    std::shared_ptr<nicxlive::core::Puppet> cameraPuppetOwner_;
    std::shared_ptr<ext::nodes::ExCamera> selectedCamera;
    nicxlive::core::common::SharedArray<std::shared_ptr<ext::nodes::ExCamera>> cameras;
    bool transparency = false;
    bool postprocessing = false;
    void initialize(compat::RetainedStringSlice file);
    void export_();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
    void onClose() override;
public:
    ImageExportWindow() = default;
};
}
