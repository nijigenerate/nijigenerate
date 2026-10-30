#include "workflow.hpp"
#include "detail/workflow_processor.hpp"
#include "detail/workflow_source.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <charconv>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

namespace nijigenerate::autorig::workflow {
void AutoRigWorkflowRun::prepareSourceClassInfo() {
    detail::registerClass<AutoRigWorkflowRun>("nijigenerate.autorig.workflow.AutoRigWorkflowRun");
}
Text AutoRigWorkflowRun::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AutoRigWorkflowRun::toHash() { return detail::objectHash(this); }
bool AutoRigWorkflowRun::opEquals(std::shared_ptr<AutoRigWorkflowRun> other) {
    return detail::objectEquals(this, other);
}
int AutoRigWorkflowRun::opCmp(std::shared_ptr<AutoRigWorkflowRun>) {
    prepareSourceClassInfo();
    return detail::objectCompare(nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)));
}
AutoRigWorkflowManager::AutoRigWorkflowManager(std::shared_ptr<AutoRigSessionManager> value)
    : sessions(std::move(value)) { prepareSourceClassInfo(); }
void AutoRigWorkflowManager::prepareSourceClassInfo() {
    detail::registerClass<AutoRigWorkflowManager>("nijigenerate.autorig.workflow.AutoRigWorkflowManager");
}
Text AutoRigWorkflowManager::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t AutoRigWorkflowManager::toHash() { return detail::objectHash(this); }
bool AutoRigWorkflowManager::opEquals(std::shared_ptr<AutoRigWorkflowManager> other) {
    return detail::objectEquals(this, other);
}
int AutoRigWorkflowManager::opCmp(std::shared_ptr<AutoRigWorkflowManager>) {
    prepareSourceClassInfo();
    return detail::objectCompare(nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)));
}
void WorkflowProcessor::prepareSourceClassInfo() {
    detail::registerClass<WorkflowProcessor>("nijigenerate.autorig.workflow.WorkflowProcessor");
}
namespace detail {
Text instanceId(Text stepId, Text taskId) {
    char decimal[std::numeric_limits<std::size_t>::digits10 + 1];
    const auto converted = std::to_chars(std::begin(decimal), std::end(decimal), stepId.size());
    // digits10 + 1 holds every unsigned size_t value, so this conversion cannot
    // run out of buffer and adds no exception absent from the original helper.
    try {
        std::string result(decimal, converted.ptr);
        result.push_back('_');
        if (!stepId.empty()) result.append(stepId.view());
        result.push_back('_');
        if (!taskId.empty()) result.append(taskId.view());
        return Text(std::move(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
}
