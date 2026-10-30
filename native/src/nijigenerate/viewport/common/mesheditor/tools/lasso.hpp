#pragma once
#include "select.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <memory>

namespace nijigenerate::viewport::common::mesheditor::tools::lasso {
enum class LassoType : std::int32_t { PolyLasso=0, RegularLasso=1 };
// Initialize the actual per-thread module AA before editor startup.
void ngBootstrapLassoThread();
class LassoIO : public std::enable_shared_from_this<LassoIO> {
public:
    LassoIO() = default;
    virtual ~LassoIO() = default;
    LassoIO(const LassoIO&) = delete;
    LassoIO& operator=(const LassoIO&) = delete;
    bool addSelect = false, removeSelect = false, undo = false, cleanup = false;
    virtual void update();
    virtual compat::RetainedString toString() {
        const auto keepAlive = weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<LassoIO> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<LassoIO>) {
        const auto keepAlive = weak_from_this().lock();
        prepareSourceClassInfo();
        base::detail::throwObjectComparison(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo() {
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(LassoIO),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoIO"));
            return true;
        }();
        (void)registered;
    }
};
class LassoTool : public select::NodeSelect {
    nicxlive::core::math::Vec2Array lassoPoints;
    compat::Slice<std::size_t> rollbackCheckpoints;
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(LassoTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoTool"));
            return true;
        }();
        (void)registered;
    }
public:
    LassoType lassoType = LassoType::RegularLasso;
    virtual void setNextMode();
    virtual void cleanup();
    virtual bool rollbackOnce();
    virtual void commitCheckpoint();
    virtual void doSelection(EditorRef editor, std::shared_ptr<LassoIO> io);
    virtual bool doSelectionTrigger(EditorRef editor, std::shared_ptr<LassoIO> io);
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    virtual bool isCloses(Vec2 first, Vec2 second);
    virtual std::size_t findClosest(Vec2 target);
    virtual std::size_t isClosestToStart(Vec2 target);
    void draw(base::CameraRef camera, EditorRef editor) override;
};
namespace detail {
bool displayOptions(base::Editors editors);
}
template<class T> requires std::derived_from<T, LassoTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.lasso.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    bool viewportTools(bool deformOnly, base::VertexToolMode mode, base::Editors editors) override {
        return base::ToolInfoBase<T>::viewportTools(deformOnly, mode, std::move(editors));
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::LassoSelection; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\xbd\x92"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Lasso Selection"));
    }
    bool displayToolOptions(bool, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this();
        return detail::displayOptions(std::move(editors));
    }
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoType> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoType;
    inline static constexpr std::string_view name = "LassoType";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::PolyLasso, "PolyLasso"},
        FormatEnumMember<Enum>{Enum::RegularLasso, "RegularLasso"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoType> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.lasso.LassoType)";
    inline static constexpr char unqualifiedArgument[] = "(LassoType)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoType";
    inline static constexpr char unqualifiedElement[] = "LassoType";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoIO> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.lasso.LassoIO)";
    inline static constexpr char unqualifiedArgument[] = "(LassoIO)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoIO";
    inline static constexpr char unqualifiedElement[] = "LassoIO";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoIO> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoIO";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.lasso.LassoIO)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.lasso.LassoTool)";
    inline static constexpr char unqualifiedArgument[] = "(LassoTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoTool";
    inline static constexpr char unqualifiedElement[] = "LassoTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.lasso.LassoTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.lasso.LassoTool)";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.lasso.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.lasso.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::LassoTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::lasso::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.lasso.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::lasso::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
