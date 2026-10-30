/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
*/
#include "package.hpp"
#include "vertex/automesh/alpha_provider.hpp"
#include <nijigenerate/compat/signal.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/project.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport {
namespace {
class ModelView : public compat::SignalReceiver {
public:
    virtual void onEditModeChanging(project::EditMode) {
        compat::ngRequireObject(base::ngViewport())->withdraw();
    }
    virtual void onEditModeChanged(project::EditMode mode) {
        compat::ngRequireObject(base::ngViewport())->onEditModeChanged(mode);
        compat::ngRequireObject(base::ngViewport())->present();
        if (mode != project::EditMode::VertexEdit) vertex::automesh::alphaPreviewDisposeTexture();
    }
    virtual void onCameraFocused(float focus, nicxlive::core::math::Vec2 position) {
        base::viewportTargetZoom = focus;
        base::viewportTargetPosition = position;
    }
    virtual void onAnimationChanged(project::AnimationPlaybackRef animation) {
        const auto receiver = compat::ngRequireObject(base::ngViewport());
        const auto playback = compat::ngRequireObject(animation);
        const auto value = compat::ngRequireObject(playback->animation());
        receiver->animationChanged(*value);
    }

    // Inherited Object defaults, Digital Mars 2000-2011, BSL-1.0.
    // Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
    virtual compat::RetainedString toString() {
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(ModelView* other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other);
    }
    virtual int opCmp(ModelView*) {
        prepareSourceClassInfo();
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

private:
    static void prepareSourceClassInfo();
};
} // namespace
} // namespace nijigenerate::viewport

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::ModelView> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.ModelView)";
    inline static constexpr char unqualifiedArgument[] = "(ModelView)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.ModelView";
    inline static constexpr char unqualifiedElement[] = "ModelView";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::ModelView> {
    inline static constexpr char className[] = "nijigenerate.viewport.ModelView";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.ModelView)";
};
}

namespace nijigenerate::viewport {
namespace {
void ModelView::prepareSourceClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<ModelView>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<ModelView>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(ModelView), name);
        return true;
    }();
    (void)registered;
}
thread_local std::unique_ptr<ModelView> view;
void connectProject(std::shared_ptr<project::Project> project) {
    compat::ngRequireObject(base::ngViewport())->present();
    compat::ngRequireObject(project)->EditModeChanging.connect<&ModelView::onEditModeChanging>(
        *compat::ngRequireObject(view.get()));
    compat::ngRequireObject(project)->EditModeChanged.connect<&ModelView::onEditModeChanged>(
        *compat::ngRequireObject(view.get()));
    compat::ngRequireObject(project)->CameraFocused.connect<&ModelView::onCameraFocused>(
        *compat::ngRequireObject(view.get()));
    {
        const auto signalOwner = compat::ngRequireObject(project);
        const auto receiver = compat::ngRequireObject(base::ngViewport());
        signalOwner->SelectionChanged.connect<&base::MainViewport::selectionChanged>(*receiver);
    }
    {
        const auto signalOwner = compat::ngRequireObject(project);
        const auto receiver = compat::ngRequireObject(base::ngViewport());
        signalOwner->ArmedParameterChanged.connect<&base::MainViewport::armedParameterChanged>(*receiver);
    }
    compat::ngRequireObject(project)->AnimationChanged.connect<&ModelView::onAnimationChanged>(
        *compat::ngRequireObject(view.get()));
    base::ngViewportReset();
}
}
void ngInitViewportModule() {
    if (view) return;
    try {
        view = std::make_unique<ModelView>();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    project::ngRegisterProjectCallback(&connectProject);
}
}
