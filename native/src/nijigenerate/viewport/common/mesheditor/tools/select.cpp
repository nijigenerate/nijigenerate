#include "select.hpp"
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::select {
namespace {
template<class Function>
auto makeMirrorCallback(Function&& function) {
    try {
        return std::make_shared<const std::function<void(std::uint32_t)>>(
            std::forward<Function>(function));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
using Mat4 = nicxlive::core::math::Mat4;
using Mat3 = core::math::source_matrix::Mat3;
using Vec3 = nicxlive::core::math::Vec3;
float length(Vec3 v) {
#if defined(_WIN32) && defined(_MSC_VER)
    using SourceReal = double;
#else
    using SourceReal = long double;
#endif
    SourceReal sum = 0;
    sum += v.x * v.x;
    sum += v.y * v.y;
    sum += v.z * v.z;
    return static_cast<float>(std::sqrt(sum));
}
float dot(Vec3 a, Vec3 b) {
    float sum = 0;
    sum += a.x * b.x;
    sum += a.y * b.y;
    sum += a.z * b.z;
    return sum;
}
float dot(base::Vec2 a, base::Vec2 b) {
    float sum = 0;
    sum += a.x * b.x;
    sum += a.y * b.y;
    return sum;
}
}

void NodeSelect::setToolMode(VertexToolMode, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    isDragging = false;
    if (!editor) compat::ngFailNullAccess(true);
    editor->isSelecting = false;
    viewport::base::ngSetViewportAlwaysUpdate(false);
}
Vec3 NodeSelect::calculateMousePosIntersection(EditorRef editor, Vec2 mouse) {
    editor = compat::ngRequireObject(editor);
    const Vec3 translation{editor->transform[0][3], editor->transform[1][3], editor->transform[2][3]};
    Mat4 rs = editor->transform;
    rs[0][3] = rs[1][3] = rs[2][3] = 0;
    const float sx = length({rs[0][0], rs[1][0], rs[2][0]});
    const float sy = length({rs[0][1], rs[1][1], rs[2][1]});
    const float sz = length({rs[0][2], rs[1][2], rs[2][2]});
    Mat4 r{};
    r[0][0] = sx; r[1][1] = sy; r[2][2] = sz; r[3][3] = 1;
    const Mat4 s = Mat4::multiply(rs, core::math::source_matrix::inverse(r));
    Mat3 product{};
    for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col)
        for (int k = 0; k < 3; ++k) product[row][col] += s[row][k] * r[k][col];
    const auto inverse = core::math::source_matrix::inverse(product);
    const float n[3]{0, 0, 1};
    float normal[3]{};
    for (int col = 0; col < 3; ++col) for (int row = 0; row < 3; ++row)
        normal[row] += inverse[col][row] * n[col];
    const Vec3 nPrime{normal[0], normal[1], normal[2]};
    const float dPrime = 0.0f - dot(nPrime, translation);
    const Vec3 point{mouse.x, mouse.y, 0};
    const float numerator = -(dot({nPrime.x, nPrime.y}, Vec2{point.x, point.y}) + dPrime);
    const float denominator = nPrime.z;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Vec3 projection{nan, nan, nan};
    if (denominator != 0) {
        const float t = numerator / denominator;
        projection = {point.x + t * 0.0f, point.y + t * 0.0f, point.z + t * 1.0f};
    } else { projection.x = nan; projection.y = nan; }
    editor->mousePos = {projection.x, projection.y};
    return projection;
}
std::int32_t NodeSelect::peek(ImGuiIO*, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    editor = compat::ngRequireObject(editor);
    editor->lastMousePos = editor->mousePos;
    editor->mousePos = core::input::ngInputGetMousePosition();
    if (editor->deformOnly) {
        const auto point = calculateMousePosIntersection(editor, {-editor->mousePos.x, -editor->mousePos.y});
        const auto transform = core::math::source_matrix::inverse(editor->transform);
        const auto output = core::math::ngTransformHomogeneousPoint(transform, point);
        editor->mousePos = {output.x, output.y};
    } else editor->mousePos = {-editor->mousePos.x, -editor->mousePos.y};
    editor->vtxAtMouse = editor->getVertexFromPoint(editor->mousePos);
    return 0;
}
bool NodeSelect::onDragUpdate(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (!isDragging) return false;
    editor = compat::ngRequireObject(editor);
    const auto selected = editor->selected;
    // Escaping delegates share the source foreach value's one live location.
    const auto index = std::allocate_shared<std::uint64_t>(
        nicxlive::core::d_runtime::SourceAllocator<std::uint64_t>{});
    for (auto value : selected) {
        *index = value;
        editor->foreachMirror(makeMirrorCallback(
            [editor, index, mouse](std::uint32_t axis) {
                const auto mirrored = editor->mirrorVertex(axis, *index);
                const auto vertices = editor->getVerticesByIndex({mirrored});
                if (vertices.empty()) {
                    throw compat::ngStaticError<compat::ArrayIndexError>(0, 0, compat::RetainedString::fromStaticLiteral(
                        "source/nijigenerate/viewport/common/mesheditor/tools/select.d"), 177);
                }
                auto* vertex = vertices[0];
                if (!vertex) return;
                editor->updateAddVertexAction(vertex);
                editor->markActionDirty();
                const auto delta = editor->mirror(axis,
                    {mouse.x - editor->lastMousePos.x, mouse.y - editor->lastMousePos.y});
                vertex->position.x += delta.x;
                vertex->position.y += delta.y;
            }));
    }
    if (!editor->selected.empty()) editor->maybeSelectOne = UINT64_MAX;
    editor->refreshMesh();
    return true;
}
void NodeSelect::draw(base::CameraRef, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    editor = compat::ngRequireObject(editor);
    if (!editor->deformOnly) return;
    const auto points = viewport::common::ngCreateCircleBuffer(editor->mousePos, {5, 5}, 32);
    core::dbg::inDbgSetBuffer(points);
    core::dbg::inDbgPointsSize(4);
    core::dbg::inDbgDrawLines({0, 1, 0, 1}, editor->transform);
}
} // namespace nijigenerate::viewport::common::mesheditor::tools::select
