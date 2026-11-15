#pragma once
#include <nijigenerate/autorig/framework.hpp>
#include <string>

namespace nijigenerate::autorig::patterns {
class StructuralPatternProcessor final : public framework::AutoRigProcessor {
public:
    explicit StructuralPatternProcessor(std::string pattern);
    framework::Text procId() override;
    framework::Text displayName() override;
    compat::Slice<framework::AutoRigTaskSpec> tasks() override;
    compat::Slice<framework::AutoRigWorkflowSpec> workflows() override;
    void executeTask(framework::Text, std::shared_ptr<framework::AutoRigTaskContext>) override;
private:
    std::string pattern_;
};
}
