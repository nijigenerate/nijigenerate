#pragma once

#include "targetview.hpp"
#include "nijigenerate/actions/depth.hpp"

namespace nijigenerate::viewport::depth::common::session {

std::shared_ptr<actions::depth::DepthMappedChangeAction> ngDepthViewWorkingDepthsChangeAction(
    std::shared_ptr<targetview::DepthTargetView> view, compat::RetainedStringSlice reason);

class DepthViewSession : public std::enable_shared_from_this<DepthViewSession> {
private:
    float depthDisplayScale{std::numeric_limits<float>::quiet_NaN()};
    std::ptrdiff_t findTargetIndex(std::uint64_t gridUuid);

public:
    DepthCamera3D camera;
    compat::Slice<std::shared_ptr<targetview::DepthTargetView>> targets;
    std::uint64_t selectedGridUuid{};

    DepthViewSession() = default;
    virtual ~DepthViewSession() = default;
    DepthViewSession(const DepthViewSession&) = delete;
    DepthViewSession& operator=(const DepthViewSession&) = delete;

    virtual std::shared_ptr<targetview::DepthTargetView> ensureTarget(
        std::shared_ptr<nicxlive::core::nodes::Deformable> target);
    virtual void setTargets(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Deformable>> nextTargets);
    virtual void setDepthDisplayScale(float value);
    virtual std::shared_ptr<targetview::DepthTargetView> targetByGrid(std::uint64_t gridUuid);
    virtual bool selectTarget(std::uint64_t gridUuid);
    virtual std::shared_ptr<targetview::DepthTargetView> selectedTarget();
    virtual void removeTarget(std::uint64_t gridUuid);
    virtual void refreshGeometry();
    virtual void resetWorkingDepths();
    virtual void resetFromTargets();
    virtual std::shared_ptr<actions::depth::DepthMappedChangeAction> workingDepthsChangeAction(
        std::uint64_t gridUuid, compat::RetainedStringSlice reason);
    virtual std::shared_ptr<actions::depth::DepthMappedChangeAction> selectedWorkingDepthsChangeAction(
        compat::RetainedStringSlice reason);
    virtual void clear();

    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthViewSession> other);
    virtual int opCmp(std::shared_ptr<DepthViewSession> other);
protected:
    virtual void prepareSourceClassInfo();
};

bool ngDepthViewSessionObjectEquals(std::shared_ptr<DepthViewSession> left,
    std::shared_ptr<DepthViewSession> right);

} // namespace nijigenerate::viewport::depth::common::session

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::common::session::DepthViewSession> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.common.session.DepthViewSession)";
    inline static constexpr char unqualifiedArgument[] = "(DepthViewSession)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.common.session.DepthViewSession";
    inline static constexpr char unqualifiedElement[] = "DepthViewSession";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::common::session::DepthViewSession> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.common.session.DepthViewSession";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.common.session.DepthViewSession)";
};
}
