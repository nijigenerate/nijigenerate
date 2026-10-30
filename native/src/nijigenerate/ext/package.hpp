#pragma once

#include <puppet.hpp>
#include <nijigenerate/ext/nodes/package.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::ext {
void incInitExt();

class ExPuppet : public Puppet {
public:
    compat::Slice<std::shared_ptr<ExParameterGroup>> groups;
    compat::RetainedString depthDrawSessionManifestJson;

    ExPuppet() = default;
    explicit ExPuppet(const std::shared_ptr<nicxlive::core::nodes::Node>& root) : Puppet(root) {}
    static std::shared_ptr<ExPuppet> create();
    static std::shared_ptr<ExPuppet> create(std::shared_ptr<nicxlive::core::nodes::Node> root);
    ExPuppet(const ExPuppet&) = delete;
    ExPuppet& operator=(const ExPuppet&) = delete;
    ExPuppet(ExPuppet&&) = delete;
    ExPuppet& operator=(ExPuppet&&) = delete;
    ParameterPtr findParameter(std::uint32_t uuid) override;
    virtual ParameterPtr findParameter(compat::RetainedString name);
    bool getIsNodeBound(const std::shared_ptr<nicxlive::core::nodes::Node>& node) override;
    void removeParameter(const ParameterPtr& parameter) override;
    void serializeSelf(serde::TypedJsonWriter& serializer) override;
    void serializeSelf(serde::TypedJsonWriter& serializer) const override;
    virtual std::shared_ptr<ExParameterGroup> findGroup(std::uint32_t uuid);
    virtual std::shared_ptr<ExParameterGroup> findGroup(compat::RetainedString name);
    virtual void addGroup(std::shared_ptr<ExParameterGroup> group);
    virtual void removeGroup(std::shared_ptr<ExParameterGroup> group);
    serde::SerdeException deserializeFromFghj(const serde::Fghj& data) override;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;
    void reconstruct() override;
    void finalize() override;
private:
    explicit ExPuppet(DeferredConstruction state) : Puppet(state) {}
};
} // namespace nijigenerate::ext

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::ExPuppet> {
    inline static constexpr char className[] = "nijigenerate.ext.ExPuppet";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.ExPuppet)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::ExPuppet> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.ExPuppet)";
    inline static constexpr char unqualifiedArgument[] = "(ExPuppet)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.ExPuppet";
    inline static constexpr char unqualifiedElement[] = "ExPuppet";
};
} // namespace nicxlive::core::d_runtime
