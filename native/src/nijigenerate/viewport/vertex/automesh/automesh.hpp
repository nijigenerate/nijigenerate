#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <compat/object_metadata.hpp>
#include <nodes/deformable.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::viewport::common::mesh { class IncMesh; }
namespace nijigenerate::viewport::vertex::automesh {

// IncMesh is passed through as the actual shared object, never copied or replaced
// with a DTO. Its definition belongs to the common mesh implementation.
class AutoMeshProcessor : public std::enable_shared_from_this<AutoMeshProcessor> {
public:
    AutoMeshProcessor() = default;
    AutoMeshProcessor(const AutoMeshProcessor&) = delete;
    AutoMeshProcessor& operator=(const AutoMeshProcessor&) = delete;
    using Target = std::shared_ptr<nicxlive::core::nodes::Deformable>;
    using Mesh = std::shared_ptr<::nijigenerate::viewport::common::mesh::IncMesh>;
    using Callback = compat::SharedDelegate<bool(Target, Mesh)>;
    virtual ~AutoMeshProcessor() = default;
    virtual Mesh autoMesh(Target target, Mesh meshData, bool mirrorHoriz = false,
        float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) = 0;
    virtual void configure() = 0;
    virtual compat::RetainedString icon() = 0;
    virtual compat::RetainedString procId();
    virtual compat::RetainedString displayName();
    virtual int order();
    virtual compat::Slice<Mesh> autoMesh(compat::Slice<Target> targets, compat::Slice<Mesh> meshData,
        bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0,
        Callback callback = {});
    // Source Object equality follows all existing processor virtual slots.
    virtual bool opEquals(std::shared_ptr<AutoMeshProcessor> other);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<AutoMeshProcessor> other);

protected:
    template<class T, std::size_t N> static void registerSourceClassInfo(const char (&name)[N]) {
        static const bool registered = [&name] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                compat::RetainedString::fromStaticLiteral(name));
            return true;
        }();
        (void)registered;
    }
    virtual void prepareSourceClassInfo();
};

bool ngAutoMeshProcessorObjectEquals(std::shared_ptr<AutoMeshProcessor> left,
    std::shared_ptr<AutoMeshProcessor> right);

} // namespace nijigenerate::viewport::vertex::automesh

namespace nijigenerate::viewport::vertex::automesh::automesh {
using ::nijigenerate::viewport::vertex::automesh::AutoMeshProcessor;
} // namespace nijigenerate::viewport::vertex::automesh::automesh

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::AutoMeshProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.automesh.AutoMeshProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshProcessor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.automesh.AutoMeshProcessor";
    inline static constexpr char unqualifiedElement[] = "AutoMeshProcessor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::automesh::AutoMeshProcessor> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.automesh.AutoMeshProcessor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.automesh.AutoMeshProcessor)";
};
} // namespace nicxlive::core::d_runtime
