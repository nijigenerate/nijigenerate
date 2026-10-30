#include "binding_actions.hpp"

namespace nijigenerate::actions::binding {

// Compile the real built-in scalar/deformation specializations as one source
// module. Other source-compatible binding types remain available in the header.
template class ParameterBindingAddRemoveAction<true>;
template class ParameterBindingAddRemoveAction<false>;
template class ParameterBindingAllValueChangeAction<float>;
template class ParameterBindingAllValueChangeAction<nicxlive::core::param::DeformSlot>;
template class ParameterBindingValueChangeAction<float, nicxlive::core::param::ValueParameterBinding>;
template class ParameterBindingValueChangeAction<nicxlive::core::param::DeformSlot,
    nicxlive::core::param::DeformationParameterBinding>;
template class ParameterBindingValueChangeAction<float,
    compat::Slice<std::shared_ptr<nicxlive::core::param::ValueParameterBinding>>>;
template class ParameterBindingValueChangeAction<nicxlive::core::param::DeformSlot,
    compat::Slice<std::shared_ptr<nicxlive::core::param::DeformationParameterBinding>>>;

} // namespace nijigenerate::actions::binding
