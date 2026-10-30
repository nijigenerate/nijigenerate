// Faithful port of source/nijigenerate_tests/welding.d; no additional cases.
#include "welding.hpp"
#include <nijigenerate/core/math/welding.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/source_call_arguments.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <nodes/drawable.hpp>
#include <puppet.hpp>
#include <load_diagnostics.hpp>
#include <fmt/serialize.hpp>
#include <compat/source_allocator.hpp>
#include <compat/scope_exit.hpp>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <memory>
#include <utility>

namespace nijigenerate_tests::welding {
namespace {
using namespace nijigenerate::core::math::welding;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::nodes::Node;
using nicxlive::core::math::Vec2Array;
using nicxlive::core::common::SharedArray;
using Indices = SharedArray<std::ptrdiff_t>;
using Triangles = SharedArray<std::uint16_t>;
using Points = nijigenerate::compat::Slice<Vec2>;
namespace runtime = nicxlive::core::d_runtime;

class Surface : public Drawable {
    explicit Surface(DeferredConstruction tag) : Drawable(tag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
public:
    template<class Parent> static std::shared_ptr<Surface> create(Parent&& parent) {
        auto result = std::allocate_shared<Surface>(runtime::SourceAllocator<Surface>{}, DeferredConstruction{});
        result->constructDrawableOwned(std::forward<Parent>(parent)());
        return result;
    }
    void renderMask(bool dodge = false) override { (void)dodge; }
    void prepare() {
        getDeformation().setLength(getVertices().size());
        getDeformation().fill(Vec2{0, 0});
        postProcessed = 2;
    }
    void weld(std::shared_ptr<Surface> other) {
        auto matrix = other->transform().matrix();
        using SourceMethod = Node::DeformFilterResult (Drawable::*)(
            std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*);
        const auto method = static_cast<SourceMethod>(&Drawable::weldingProcessor);
        nijigenerate::compat::SourceCallArguments<std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*> arguments{
            {other}, {other->getVertices()}, {other->getDeformation()}, {&matrix}};
        (this->*method)(nijigenerate::compat::ngSourceCallArgument<0>(arguments),
            nijigenerate::compat::ngSourceCallArgument<1>(arguments),
            nijigenerate::compat::ngSourceCallArgument<2>(arguments),
            nijigenerate::compat::ngSourceCallArgument<3>(arguments));
    }
};

template<class A, class B> bool equal(const A& left, const B& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(left[i])>, Vec2>) {
            if (left[i].x != right[i].x || left[i].y != right[i].y) return false;
        } else if (left[i] != right[i]) return false;
    }
    return true;
}
bool equal(Vec2 left, Vec2 right) { return left.x == right.x && left.y == right.y; }
#ifndef NDEBUG
#define SOURCE_ASSERT(line, ...) do { if (!(__VA_ARGS__)) throw runtime::ngStaticError<runtime::AssertError>( \
    runtime::RetainedString::fromStaticLiteral("Assertion failure"), \
    runtime::RetainedString::fromStaticLiteral("source/nijigenerate_tests/welding.d"), line); } while (false)
#else
#define SOURCE_ASSERT(line, ...) ((void)0)
#endif

void runWeldingBody() {
    auto points = Points{{0, 0}, {0.1f, 0}, {10, 0}};
    auto targets = Points{{0, 0}, {10, 0}};
    SOURCE_ASSERT(34, equal(ngMatchWeldingVertices(points, targets), Indices{0, 0, 1}));
    SOURCE_ASSERT(35, equal(ngMatchWeldingVertices(points, targets, Indices{1}), Indices{-1, -1, 1}));
    SOURCE_ASSERT(36, equal(ngMatchWeldingVertices(Points{points[2], points[0]}, targets,
        Indices{0, -1, 1}), Indices{1, 0}));
    SOURCE_ASSERT(37, equal(ngMatchWeldingVertices(points, Points{}), Indices{-1, -1, -1}));
    SOURCE_ASSERT(38, equal(ngMatchWeldingVertices(Points{{std::numeric_limits<float>::quiet_NaN(), 0}},
        targets), Indices{-1}));
    auto removed = ngRemapWeldingLinks(Indices{0, 1, 2}, Indices{0, 1, 2}, Indices{0, -1, 1}, 2);
    SOURCE_ASSERT(41, equal(removed.forward, Indices{0, 2}) && equal(removed.reverse, Indices{0, -1, 1}));
    auto reordered = ngRemapWeldingLinks(Indices{0, -1, 1}, Indices{0, 2}, Indices{2, 1, 0}, 4);
    SOURCE_ASSERT(43, equal(reordered.forward, Indices{1, -1, 0, -1}) && equal(reordered.reverse, Indices{2, 0}));
    auto denseRemoved = ngRemapWeldingLinks(Indices{0, 0, 1}, Indices{0, 2}, Indices{-1, 0, 1}, 2);
    SOURCE_ASSERT(45, equal(denseRemoved.forward, Indices{0, 1}) && equal(denseRemoved.reverse, Indices{0, 1}));
    auto coarseRemoved = ngRemapWeldingLinks(Indices{0, 2}, Indices{0, 0, 1}, Indices{-1, 0}, 1);
    SOURCE_ASSERT(47, equal(coarseRemoved.forward, Indices{2}) && equal(coarseRemoved.reverse, Indices{-1, -1, 0}));
    auto coarse = Points{{0, 0}, {10, 0}, {0, 10}};
    auto fine = Points{{5, 0}, {5, -5}, {6, -5}};
    auto refined = ngRefineWeldingMesh(fine, Triangles{0, 1, 2}, coarse, coarse, Triangles{0, 1, 2}, Mat4::identity());
    SOURCE_ASSERT(51, refined.addedVertices == 1 && equal(refined.vertices.slice(0, 3), coarse));
    SOURCE_ASSERT(52, equal(refined.vertices[3], fine[0]) && refined.indices.size() == 6);
    SOURCE_ASSERT(53, ngRefineWeldingMesh(fine, Triangles{0, 1, 2}, refined.vertices, refined.vertices,
        refined.indices, Mat4::identity()).addedVertices == 0);
    auto curved = Points{{0, 0}, {10, -6}, {20, 0}, {8, 6}};
    auto chord = Points{{0, 0}, {20, 0}, {0, 20}};
    auto contour = ngRefineWeldingMesh(curved, Triangles{0, 1, 2, 0, 2, 3}, chord, chord,
        Triangles{0, 1, 2}, Mat4::identity());
    SOURCE_ASSERT(61, contour.addedVertices == 1 && equal(contour.vertices[3], curved[1]));
    SOURCE_ASSERT(62, equal(contour.vertices.slice(0, 3), chord));
    SOURCE_ASSERT(63, ngRefineWeldingMesh(curved, Triangles{0, 1, 2, 0, 2, 3}, contour.vertices,
        contour.vertices, contour.indices, Mat4::identity()).addedVertices == 0);

    auto puppet = nicxlive::core::Puppet::create();
    auto a = Surface::create([&] { return puppet->root; });
    auto b = Surface::create([&] { return puppet->root; });
    a->getVertices().setLength(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) a->getVertices()[i] = points[i];
    b->getVertices().setLength(targets.size());
    for (std::size_t i = 0; i < targets.size(); ++i) b->getVertices()[i] = targets[i];
    Indices indices{0, -1, 1};
    a->addWeldedTarget(b, indices, 0.25f);
    indices[0] = -1;
    SOURCE_ASSERT(76, equal(a->weldedLinks[0].indices, Indices{0, -1, 1}));
    SOURCE_ASSERT(77, equal(b->weldedLinks[0].indices, Indices{0, 2}));
    a->addWeldedTarget(b, Indices{0, 0, 1}, 0.25f);
    SOURCE_ASSERT(79, equal(a->weldedLinks[0].indices, Indices{0, 0, 1}));
    SOURCE_ASSERT(80, equal(b->weldedLinks[0].indices, Indices{0, 2}));
    using Counter = Indices (Drawable::*)(std::shared_ptr<Drawable>, Indices);
    const auto counter = static_cast<Counter>(&Drawable::updatedCounterWeldingIndices);
    SOURCE_ASSERT(81, equal((a.get()->*counter)(b, Indices{-1, 0, 1}), Indices{1, 2}));
    a->weldedLinks[0].indices = Indices{-1, 0, 1};
    b->weldedLinks[0].indices = Indices{1, 2};
    SOURCE_ASSERT(85, equal((b.get()->*counter)(a, Indices{1, 2}), Indices{-1, 0, 1}));
    a->addWeldedTarget(b, Indices{0, 0, 1}, 0.25f);
    bool rejected = false;
    try { a->addWeldedTarget(b, Indices{0, -1, 2}, 0.25f); }
    catch (runtime::ThrowableException&) { rejected = true; }
    SOURCE_ASSERT(89, rejected);
    a->weldedLinks[0].indices = Indices{0, 0, 2};
    rejected = false;
    try { (void)nicxlive::core::fmt::inToJson(std::static_pointer_cast<Drawable>(a)); }
    catch (runtime::ThrowableException&) { rejected = true; }
    SOURCE_ASSERT(93, rejected);
    a->finalize();
    b->finalize();
    SOURCE_ASSERT(96, equal(a->weldedLinks[0].indices, Indices{0, 0, -1}));
    SOURCE_ASSERT(97, equal(b->weldedLinks[0].indices, Indices{0, 2}));
    SOURCE_ASSERT(98, a->weldedLinks[0].weight == 0.25f && b->weldedLinks[0].weight == 0.75f);
    SOURCE_ASSERT(99, nicxlive::core::serde::inLastLoadDiagnostics.repairedWeldingMappings == 1);
    a->addWeldedTarget(b, Indices{0, 0, 1}, 0.25f);
    a->prepare();
    b->prepare();
    a->weldedLinks[0].indices = Indices{-1, 0, 1};
    b->getDeformation()[0] = Vec2{8, 0};
    a->weld(b);
    SOURCE_ASSERT(107, equal(static_cast<Vec2>(a->getDeformation()[0]), Vec2{0, 0}));
    a->prepare();
    b->prepare();
    a->addWeldedTarget(b, Indices{0, 0, 1}, 0.25f);
    a->weldingApplied = {};
    b->weldingApplied = {};
    b->getDeformation()[0] = Vec2{8, 0};
    a->weld(b);
    SOURCE_ASSERT(116, std::abs(static_cast<Vec2>(a->getDeformation()[0]).x - 6.0125f) < 0.0001f);
    SOURCE_ASSERT(117, std::abs(static_cast<Vec2>(a->getDeformation()[1]).x - 5.9125f) < 0.0001f);
    SOURCE_ASSERT(118, std::abs(static_cast<Vec2>(b->getDeformation()[0]).x - 6.0125f) < 0.0001f);
    a->weldingApplied = {};
    b->weldingApplied = {};
    a->getDeformation().fill(Vec2{0, 0});
    b->getDeformation().fill(Vec2{0, 0});
    b->getDeformation()[0] = Vec2{8, 0};
    b->weld(a);
    SOURCE_ASSERT(126, std::abs(static_cast<Vec2>(a->getDeformation()[0]).x - 6.0125f) < 0.0001f);
    SOURCE_ASSERT(127, std::abs(static_cast<Vec2>(a->getDeformation()[1]).x - 5.9125f) < 0.0001f);
    SOURCE_ASSERT(128, std::abs(static_cast<Vec2>(b->getDeformation()[0]).x - 6.0125f) < 0.0001f);
    nijigenerate::providers::mcp::stdio::standardOutput().writeln(
        "PASS: dense Welding, seam restriction, reordered vertices, load/save bounds, averaged anchors and filter-order independence");
}
#undef SOURCE_ASSERT
void APIENTRY sourceGenVertexArrays(GLsizei count, GLuint* arrays) noexcept {
    for (GLsizei i = 0; i < count; ++i) arrays[i] = 0;
}
} // namespace

void ngTestWelding() {
    const auto previousGen = nijigenerate::platform::ngGLGetGenVertexArraysBinding();
    nijigenerate::platform::ngGLSetGenVertexArraysBinding(&sourceGenVertexArrays);
    runtime::ngRunWithScopeExit(&runWeldingBody, [&] {
        nijigenerate::platform::ngGLRestoreGenVertexArraysBinding(previousGen);
    });
}
} // namespace nijigenerate_tests::welding
