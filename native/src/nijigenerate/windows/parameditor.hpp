#pragma once
#include "base.hpp"
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <param/parameter.hpp>
#include <cmath>
#include <array>
#include <cstdint>
#include <memory>

namespace nijigenerate::windows::parameditor {
enum class ParamEditorTab : std::int32_t { Properties, Axes };
class ParamEditorWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    using Vec2 = nicxlive::core::nodes::Vec2;
    std::shared_ptr<nicxlive::core::param::Parameter> param;
    ParamEditorTab defaultTab = ParamEditorTab::Properties;
    compat::RetainedStringSlice paramName;
    Vec2 min{NAN, NAN}, max{NAN, NAN}, endPoint{NAN, NAN};
    std::array<compat::Slice<widgets::EditableAxisPoint>, 2> points;
    void initialize(std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
        ParamEditorTab tab = ParamEditorTab::Properties);
    bool isValidName();
    float mapAxisLocal(std::uint32_t axis, float value);
    float unmapAxisLocal(std::uint32_t axis, float norm);
    void findEndPoint();
    compat::Slice<widgets::EditableAxisPoint>& axisPoints(std::uint64_t axis, std::size_t sourceLine);
    widgets::EditableAxisPoint& axisPoint(std::uint64_t axis, std::size_t index, std::size_t sourceLine);
    compat::Slice<std::size_t> orderPoints(std::uint64_t axis, std::size_t countLine, std::size_t compareLine);
    void createPoint(std::uint64_t axis);
    void axisPointList(std::uint64_t axis, ImVec2 available);
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    ParamEditorWindow() = default;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::parameditor::ParamEditorTab> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.parameditor.ParamEditorTab)";
    inline static constexpr char unqualifiedArgument[] = "(ParamEditorTab)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.parameditor.ParamEditorTab";
    inline static constexpr char unqualifiedElement[] = "ParamEditorTab";
};
}
