#pragma once

#include "map.hpp"
#include <nijigenerate/compat/exception_message.hpp>
#include <stdexcept>
#include <string>

namespace nijigenerate::actions::asyncprogress { class AsyncGroupActionProgress; }
namespace nicxlive::core::nodes { class GridDeformer; }

namespace nijigenerate::commands::depth::map::detail {

// Retain the complete formatted source error, including embedded NUL bytes.
class DepthMapValueError : public std::runtime_error, public compat::ExceptionMessage {
public:
    explicit DepthMapValueError(std::string text) : std::runtime_error(text), msg(std::move(text)) {}
    const compat::RetainedString msg;
    std::string_view ngExceptionMessage() const noexcept override { return msg.view(); }
};

extern thread_local std::shared_ptr<actions::asyncprogress::AsyncGroupActionProgress> activePsdDepthImportProgress;
float finiteFloat(float value, compat::RetainedStringSlice name);
std::shared_ptr<ext::nodes::DepthMappedNode> requireDepthMapped(std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<ext::nodes::DepthOperationMappedNode> requireDepthOperated(
    std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<nicxlive::core::nodes::GridDeformer> requireDepthGrid(
    std::shared_ptr<nicxlive::core::nodes::Node> node);
void replaceDepthsWithUndo(std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::Slice<float> nextDepths, compat::RetainedStringSlice reason);
std::shared_ptr<actions::depth::DepthOperationMappedChangeAction> ngClearDepthOpsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> target, compat::RetainedStringSlice reason);
void replaceDepthOpsWithUndo(std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::Slice<ext::nodes::ExDepthOp> nextOps, compat::RetainedStringSlice reason);

// Real canonical base; implementations still require the translation provider.
class PsdDepthImportChangeAction : public actions::AsyncGroupAction {
public:
    PsdDepthImportChangeAction() = default;
    PsdDepthImportChangeAction(const PsdDepthImportChangeAction&) = delete;
    PsdDepthImportChangeAction& operator=(const PsdDepthImportChangeAction&) = delete;
    actions::ActionText describe() override;
    actions::ActionText describeUndo() override;
    actions::ActionText getName() override;
};

} // namespace nijigenerate::commands::depth::map::detail
