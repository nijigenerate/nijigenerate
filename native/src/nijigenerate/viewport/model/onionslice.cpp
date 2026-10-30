#include "onionslice.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/ext/nodes/excamera_math.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <runtime_state.hpp>
#include <render/immediate.hpp>
#include <algorithm>
#include <limits>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::model::onionslice {
using nicxlive::core::math::Mat4;
using nicxlive::core::math::Vec2;
using nicxlive::core::nodes::Composite;
using nicxlive::core::Texture;
using backend::snapshot::Snapshot;

namespace {
void prepareOnionSliceClassInfo() {
    static const bool registered = [] {
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        using nicxlive::core::d_runtime::ngRegisterObjectTypeNames;
        using nicxlive::core::d_runtime::SourceObjectTypeNames;
        ngRegisterObjectTypeNames<OnionSlice>(
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<OnionSlice>::className),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<OnionSlice>::templateArgumentName));
        ngRegisterObjectTypeNames<detail::OnionSliceCamera>(
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<detail::OnionSliceCamera>::className),
            compat::RetainedString::fromStaticLiteral(
                SourceObjectTypeNames<detail::OnionSliceCamera>::templateArgumentName));
        ngRegisterObjectClassName(typeid(OnionSlice),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<OnionSlice>::className));
        ngRegisterObjectClassName(typeid(detail::OnionSliceCamera),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<detail::OnionSliceCamera>::className));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool onionSliceSourceClassInfo = [] { prepareOnionSliceClassInfo(); return true; }();
}

// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
nicxlive::core::d_runtime::RetainedString OnionSlice::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareOnionSliceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t OnionSlice::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool OnionSlice::opEquals(std::shared_ptr<OnionSlice> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int OnionSlice::opCmp(std::shared_ptr<OnionSlice>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareOnionSliceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngOnionSliceObjectEquals(std::shared_ptr<OnionSlice> left, std::shared_ptr<OnionSlice> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareOnionSliceClassInfo);
}

OnionSliceGeometry::OnionSliceGeometry() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    area = {nan, nan, nan, nan};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) worldMatrix[row][column] = nan;
    }
}
OnionSliceGeometry::OnionSliceGeometry(nicxlive::core::Rect value, Mat4 camera, Mat4 render, Mat4 model)
    : area(value), worldMatrix(ext::nodes::excamera_detail::inverse(camera) * render * model) {}
OnionSliceGeometry::OnionSliceGeometry(nicxlive::core::math::Vec2Array vertices,
    Mat4 camera, Mat4 render, Mat4 model) {
    Vec2 minimum = vertices[0];
    Vec2 maximum = minimum;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const Vec2 vertex = vertices[index];
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.y = std::min(minimum.y, vertex.y);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y);
    }
    *this = OnionSliceGeometry({minimum.x, minimum.y, maximum.x - minimum.x, maximum.y - minimum.y},
        camera, render, model);
}
Mat4 OnionSliceGeometry::matrix(Mat4 camera) const { return camera * worldMatrix; }
Mat4 detail::OnionSliceCamera::matrix() const {
    const auto camera = compat::ngRequireObject(nicxlive::core::inGetCameraHandle());
    return geometry.matrix(camera->matrix());
}
Mat4 detail::OnionSliceCamera::matrix() {
    // The known Source subclass retains its genuine existing const implementation.
    return static_cast<const OnionSliceCamera&>(*this).OnionSliceCamera::matrix();
}
namespace {
class OnionSliceGeometryReader : public Composite {
public:
    static OnionSliceGeometry capture(const std::shared_ptr<Composite>& composite) {
        nicxlive::core::nodes::PartDrawPacket packet;
        composite->fillDrawPacket(*composite, packet);
        // D captures the array descriptor before evaluating the virtual camera.
        // A camera callback can replace the composite's public vertex descriptor.
        const auto vertices = composite->getVertices();
        const auto camera = compat::ngRequireObject(nicxlive::core::inGetCameraHandle());
        const auto cameraMatrix = camera->matrix();
        return OnionSliceGeometry(vertices, cameraMatrix, packet.renderMatrix, packet.modelMatrix);
    }
};
}
} // namespace nijigenerate::viewport::model::onionslice

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::model::onionslice::OnionSliceGeometryReader> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.model.onionslice.OnionSliceGeometryReader)";
    inline static constexpr char unqualifiedArgument[] = "(OnionSliceGeometryReader)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.model.onionslice.OnionSliceGeometryReader";
    inline static constexpr char unqualifiedElement[] = "OnionSliceGeometryReader";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::model::onionslice::OnionSliceGeometryReader> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.model.onionslice.OnionSliceGeometryReader";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.model.onionslice.OnionSliceGeometryReader)";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::viewport::model::onionslice {
namespace {
[[maybe_unused]] const bool onionSliceReaderSourceClassInfo = [] {
    using namespace nicxlive::core::d_runtime;
    using Names = SourceObjectTypeNames<OnionSliceGeometryReader>;
    const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
    ngRegisterObjectTypeNames<OnionSliceGeometryReader>(name,
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
    ngRegisterObjectClassName(typeid(OnionSliceGeometryReader), name);
    return true;
}();
}
thread_local std::shared_ptr<OnionSlice> OnionSlice::instance;
OnionSlice::OnionSlice() {
    numHistory = 2;
    history.resize(numHistory);
    overlayCamera = std::allocate_shared<detail::OnionSliceCamera>(
        nicxlive::core::d_runtime::SourceAllocator<detail::OnionSliceCamera>{});
    lastIndex = 0;
    snapshot.reset();
    opacity_ = 0.2f;
    color_ = {1, 1, 1};
    lastKeypoint = {0, 0};
}
std::shared_ptr<OnionSlice> OnionSlice::singleton() {
    if (const auto receiver = instance) {
        const auto active = receiver->activePuppet;
        const auto current = project::incActivePuppet();
        if (!nicxlive::core::ngPuppetObjectEquals(active, current)) instance.reset();
    }
    if (!instance) {
        instance = std::allocate_shared<OnionSlice>(
            nicxlive::core::d_runtime::SourceAllocator<OnionSlice>{});
        instance->activePuppet = project::incActivePuppet();
    }
    return instance;
}
void OnionSlice::start() {
    const auto keepAlive = weak_from_this().lock();
    if (!snapshot) {
        snapshot = Snapshot::get(project::incActivePuppet());
        hasLastKeypoint = false;
    }
}
void OnionSlice::stop() {
    const auto keepAlive = weak_from_this().lock();
    if (snapshot) {
        {
            const auto receiver = snapshot;
            receiver->release();
        }
        snapshot.reset();
        const auto historyLength = history.size();
        for (std::size_t index = 0; index < historyLength; ++index) {
            if (const auto texture = history[index].texture) texture->dispose();
            history[index] = detail::OnionSliceFrame{};
        }
    }
}
void OnionSlice::toggle() {
    const auto keepAlive = weak_from_this().lock();
    if (enabled()) stop(); else start();
}
void OnionSlice::capture(std::array<std::uint32_t, 2> keypoint) {
    const auto keepAlive = weak_from_this().lock();
    if (snapshot) {
        if (hasLastKeypoint && keypoint == lastKeypoint) return;
        lastKeypoint = keypoint;
        hasLastKeypoint = true;
        {
            const auto receiver = compat::ngRequireObject(snapshot);
            receiver->onCaptured(Snapshot::CaptureCallback::fromContext(shared_from_this(),
                [](void* context, std::shared_ptr<Texture> texture) {
                    auto* self = static_cast<OnionSlice*>(context);
                    if (!self->snapshot) return;
                    const auto puppet = compat::ngRequireObject(self->activePuppet);
                    const auto root = compat::ngRequireObject(puppet->root);
                    auto composite = std::dynamic_pointer_cast<Composite>(root->parentPtr());
                    if (!composite || composite->getVertices().size() == 0) return;
                    const auto geometry = OnionSliceGeometryReader::capture(composite);
                    if (const auto previous = self->history[self->lastIndex].texture) previous->dispose();
                    const auto duplicateReceiver = compat::ngRequireObject(texture);
                    self->history[self->lastIndex] = detail::OnionSliceFrame{duplicateReceiver->dup(), geometry};
                    self->lastIndex = (self->lastIndex + 1) % self->numHistory;
                }));
        }
        {
            const auto receiver = compat::ngRequireObject(snapshot);
            receiver->capture();
        }
    }
}
void OnionSlice::draw() {
    const auto keepAlive = weak_from_this().lock();
    if (enabled() && project::incArmedParameter()) {
        for (int index = 0; index < numHistory - 1; ++index) {
            const auto current = (lastIndex + index) % numHistory;
            if (history[current].texture) {
                overlayCamera->geometry = history[current].geometry;
                const auto texture = history[current].texture;
                const auto area = history[current].geometry.area;
                const float opacity = opacity_ * (index + 2) * (index + 2) * (index + 2)
                    / numHistory / numHistory / numHistory;
                const auto color = color_;
                const auto camera = overlayCamera;
                // D passes these values before entering the optional-backend draw.
                // Keep them stable if the renderer calls back into this instance.
                nicxlive::core::render::inDrawTextureAtRect(texture, area, {0, 0, 1, 1},
                    opacity, color, {0, 0, 0}, {}, camera);
            }
        }
    }
}
bool OnionSlice::enabled() { return static_cast<bool>(snapshot); }
}
