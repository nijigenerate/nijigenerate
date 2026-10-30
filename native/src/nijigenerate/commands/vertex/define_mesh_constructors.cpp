#include "define_mesh.hpp"
#include "nijigenerate/core/translation.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::vertex::define_mesh {
namespace {

template<std::size_t N, std::size_t M>
std::pair<compat::RetainedString, compat::RetainedString> translatedText(
    const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(label));
    auto translatedDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(translatedLabel), std::move(translatedDescription)};
}

} // namespace
void DefineMeshCommand::prepareSourceClassInfo() {
    ExCommand<DefineMeshArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<DefineMeshCommand>();
}
void DefineVerticesCommand::prepareSourceClassInfo() {
    ExCommand<DefineVerticesArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<DefineVerticesCommand>();
}
void DefineGridCommand::prepareSourceClassInfo() {
    ExCommand<DefineGridArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<DefineGridCommand>();
}
#define NG_DEFINE_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.vertex.define_mesh." #Type))

DefineMeshCommand::DefineMeshCommand(compat::Slice<float> vertices, compat::Slice<std::uint16_t> indices)
    : DefineMeshCommand(SourceConstructorFrame::capture(vertices, indices)) {}

DefineMeshCommand::SourceConstructorFrame DefineMeshCommand::SourceConstructorFrame::capture(
    compat::Slice<float>& vertices, compat::Slice<std::uint16_t>& indices) {
    auto text = translatedText("Define Mesh",
        "Apply a mesh defined by vertices and indices to selected nodes.");
    return {std::move(text), vertices, indices};
}

DefineMeshCommand::DefineMeshCommand(SourceConstructorFrame frame)
    : ExCommand<DefineMeshArguments>(std::move(frame.text.first), std::move(frame.text.second),
                                    std::move(frame.vertices), std::move(frame.indices)) {
    NG_DEFINE_CLASS_INFO(DefineMeshCommand);
}

DefineVerticesCommand::DefineVerticesCommand(compat::Slice<float> vertices)
    : DefineVerticesCommand(SourceConstructorFrame::capture(vertices)) {}

DefineVerticesCommand::SourceConstructorFrame DefineVerticesCommand::SourceConstructorFrame::capture(
    compat::Slice<float>& vertices) {
    auto text = translatedText("Define Vertices",
        "Apply control vertices to selected Deformable nodes. Does not define triangle topology.");
    return {std::move(text), vertices};
}

DefineVerticesCommand::DefineVerticesCommand(SourceConstructorFrame frame)
    : ExCommand<DefineVerticesArguments>(std::move(frame.text.first), std::move(frame.text.second),
                                        std::move(frame.vertices)) {
    NG_DEFINE_CLASS_INFO(DefineVerticesCommand);
}

DefineGridCommand::DefineGridCommand(compat::Slice<float> axisX, compat::Slice<float> axisY)
    : DefineGridCommand(SourceConstructorFrame::capture(axisX, axisY)) {}

DefineGridCommand::SourceConstructorFrame DefineGridCommand::SourceConstructorFrame::capture(
    compat::Slice<float>& axisX, compat::Slice<float>& axisY) {
    auto text = translatedText("Define Grid",
        "Apply a rectangular grid defined by X/Y axes to selected GridDeformer nodes.");
    return {std::move(text), axisX, axisY};
}

DefineGridCommand::DefineGridCommand(SourceConstructorFrame frame)
    : ExCommand<DefineGridArguments>(std::move(frame.text.first), std::move(frame.text.second),
                                    std::move(frame.axisX), std::move(frame.axisY)) {
    NG_DEFINE_CLASS_INFO(DefineGridCommand);
}
#undef NG_DEFINE_CLASS_INFO

} // namespace nijigenerate::commands::vertex::define_mesh
