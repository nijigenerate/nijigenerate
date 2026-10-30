#include "inimport.hpp"

namespace nijigenerate::io::inimport {
template bool incAskImport<psd::PSD>(core::settings::SettingsString);
template bool incAskImport<kra::KRA>(core::settings::SettingsString);
template LayerArray<psd::PSD> incBuildLayerLayout(psd::PSD);
template LayerArray<kra::KRA> incBuildLayerLayout(kra::KRA);
}
