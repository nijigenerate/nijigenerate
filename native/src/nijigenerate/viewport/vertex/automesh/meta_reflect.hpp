#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::vertex::automesh::meta {
template<class Host> struct AMReflectionSource;

// Actual source interface: these four slots have no authored default bodies.
class IAutoMeshReflect {
public:
    virtual ~IAutoMeshReflect() = default;
    virtual compat::RetainedString schema() = 0;
    virtual compat::RetainedString values(compat::RetainedString levelName) = 0;
    virtual bool applyPreset(compat::RetainedString name) = 0;
    virtual bool writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) = 0;
};

} // namespace nijigenerate::viewport::vertex::automesh::meta

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.vertex.automesh.meta.IAutoMeshReflect)";
    inline static constexpr char unqualifiedArgument[] = "(IAutoMeshReflect)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.vertex.automesh.meta.IAutoMeshReflect";
    inline static constexpr char unqualifiedElement[] = "IAutoMeshReflect";
};
}
