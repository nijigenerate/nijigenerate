#pragma once

#include "depthmap_psd_values.hpp"
#include <nijigenerate/io/psd/layer.hpp>
#include <puppet.hpp>
#include <memory>

namespace nijigenerate::io::depthmap_psd {

PsdDepthImportResult ngBuildPsdDepthsFromPSD(std::shared_ptr<nicxlive::core::Puppet> puppet,
    compat::RetainedString path, PsdDepthImportSettings settings);
PsdDepthImportResult ngBuildPsdDepthsFromImage(std::shared_ptr<nicxlive::core::Puppet> puppet,
    compat::RetainedString path, PsdDepthImportSettings settings);
PsdDepthImportResult ngBuildPsdDepthsFromSource(std::shared_ptr<nicxlive::core::Puppet> puppet,
    compat::RetainedString path, PsdDepthImportSettings settings);
PsdDepthImportResult ngPsdDepthReplaceDepthSource(std::shared_ptr<nicxlive::core::Puppet> puppet,
    PsdDepthImportResult& previous, compat::RetainedString depthSourcePath, PsdDepthImportSettings settings);
PsdDepthImportResult ngPsdDepthReplaceColorSource(std::shared_ptr<nicxlive::core::Puppet> puppet,
    PsdDepthImportResult& previous, compat::RetainedString colorSourcePath, PsdDepthImportSettings settings);
bool ngPsdDepthLayerHasPixelData(psd::Layer& layer);

} // namespace nijigenerate::io::depthmap_psd
