#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <common/shared_array.hpp>
#include <param/parameter.hpp>
#include <memory>

namespace nicxlive::core::param {
class ValueParameterBinding;
class ParameterParameterBinding;
class DeformationParameterBinding;
}

namespace nijigenerate::commands::parameter::prop::detail {

using AxisSnapshotValues = compat::Slice<compat::Slice<float>>;
// Use the genuine bool[] carrier, not vector<bool> or a uint8 mask.
using AxisSnapshotFlags = compat::Slice<::nicxlive::core::common::SharedArray<bool>>;

// Native internal identities for the original run's local snapshot values.
struct SnapF {
    std::shared_ptr<::nicxlive::core::param::ValueParameterBinding> b;
    AxisSnapshotValues vals;
    AxisSnapshotFlags set;
};

struct SnapP {
    std::shared_ptr<::nicxlive::core::param::ParameterParameterBinding> b;
    AxisSnapshotValues vals;
    AxisSnapshotFlags set;
};

struct SnapD {
    std::shared_ptr<::nicxlive::core::param::DeformationParameterBinding> b;
    compat::Slice<compat::Slice<::nicxlive::core::param::DeformSlot>> vals;
    AxisSnapshotFlags set;
};

} // namespace nijigenerate::commands::parameter::prop::detail
