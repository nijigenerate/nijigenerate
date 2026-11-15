#include <nijigenerate/autorig/patterns/core.hpp>
#include <nijigenerate/autorig/patterns/processor.hpp>
#include <nijigenerate/autorig/detail/workflow_processor.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    namespace f = nijigenerate::autorig::framework;
    namespace p = nijigenerate::autorig::patterns;
    namespace w = nijigenerate::autorig::workflow;
    try {
        auto manager = std::make_shared<f::AutoRigSessionManager>(f::Text("/tmp/autorig-pattern-workflow-test"));
        for (const auto& spec : p::ngPatternSpecs()) {
            auto processor = std::make_shared<p::StructuralPatternProcessor>(spec.id);
            manager->registerProcessor(processor);
            auto workflows = processor->workflows();
            if (workflows.size() != 1) throw std::runtime_error("Expected one workflow");
            auto workflow = std::make_shared<w::WorkflowProcessor>(manager, processor->procId(), workflows[0]);
            if (workflow->tasks().size() != 3) throw std::runtime_error("Dependencies duplicated in workflow");
            for (const auto& task : workflow->tasks()) {
                if (task.id.empty()) throw std::runtime_error("Missing task id");
                if (task.outputs.empty()) throw std::runtime_error("Missing task outputs");
            }
        }
        std::cout << "All 35 processor registrations and workflow input/connection contracts passed\n";
    } catch (const nijigenerate::compat::ThrowableException& error) {
        std::cerr << error.msg.view() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
