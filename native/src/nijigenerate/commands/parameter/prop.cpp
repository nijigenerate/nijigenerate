#include "prop_commands.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include "prop_table.hpp"
#include "prop_pair_sort.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/binding/base.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <param/binding_impl.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <type_traits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nijigenerate::commands::parameter::prop {
namespace runtime = nicxlive::core::param;
namespace {

template<class Operation> decltype(auto) allocateArray(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N> compat::RetainedString literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}
template<class Array> decltype(auto) sourceAt(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        literal("source/nijigenerate/commands/parameter/prop.d"), line);
    return values[index];
}
std::shared_ptr<runtime::Parameter> parameterAtZero(const std::shared_ptr<Context>& context, std::size_t line) {
    auto values = compat::ngRequireObject(context)->parameters();
    return sourceAt(values, 0, line);
}
template<class Snapshot, class B> Snapshot snapshot(
        std::shared_ptr<B> binding, std::size_t nx, std::size_t ny, std::size_t flagsLine) {
    using Value = std::remove_cvref_t<decltype(binding->getValue({0, 0}))>;
    Snapshot result{binding};
    result.vals.resize(nx);
    for (std::size_t x = 0; x < nx; ++x) {
        result.vals[x].resize(ny);
        if constexpr (std::is_same_v<Value, float>)
            std::fill(result.vals[x].begin(), result.vals[x].end(), std::numeric_limits<float>::quiet_NaN());
        for (std::size_t y = 0; y < ny; ++y)
            result.vals[x][y] = binding->getValue({static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)});
    }
    result.set.resize(nx);
    for (std::size_t x = 0; x < nx; ++x)
        result.set[x] = sourceAt(binding->getIsSet(), x, flagsLine).duplicate();
    return result;
}
template<class Array> decltype(auto) sourceAxisAt(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/prop.d"), line);
    return values[index];
}

bool applyAxis(const std::shared_ptr<runtime::Parameter>& param, std::uint32_t axis,
    compat::Slice<float> target, compat::RetainedStringSlice& message) {
    message = {};
    compat::Slice<float> normalized;
    if (!detail::normalizeAxis(axis, target, normalized, message)) return false;
    target = normalized;
    if (target.size() < 2) return true;
    compat::Slice<float> current;
    const auto currentPoints = sourceAxisAt(compat::ngRequireObject(param)->axisPoints, axis, 133);
    for (float value : currentPoints) current.append(value);
    auto cn = current.size(); const auto tn = target.size();
    compat::Slice<bool> cm, tm;
    cm.resize(cn); tm.resize(tn);
    if (cn >= 1 && tn >= 1) {
        sourceAxisAt(cm, 0, 141) = true;
        sourceAxisAt(tm, 0, 141) = true;
    }
    if (cn >= 2 && tn >= 2) {
        sourceAxisAt(cm, cn - 1, 142) = true;
        sourceAxisAt(tm, tn - 1, 142) = true;
    }
    for (std::size_t t = 1; t + 1 < tn; ++t) {
        const float targetValue = sourceAxisAt(target, t, 146);
        float best = std::numeric_limits<float>::infinity();
        auto index = std::numeric_limits<std::size_t>::max();
        for (std::size_t c = 1; c + 1 < cn; ++c) {
            if (sourceAxisAt(cm, c, 150)) continue;
            const float distance = std::abs(sourceAxisAt(current, c, 151) - targetValue);
            if (distance < best) { best = distance; index = c; }
        }
        if (index != std::numeric_limits<std::size_t>::max()) {
            sourceAxisAt(cm, index, 155) = true;
            sourceAxisAt(tm, t, 156) = true;
        }
    }
    for (std::size_t i = 1; i + 1 < cn;) {
        if (!sourceAxisAt(cm, i, 164)) {
            compat::ngRequireObject(param)->deleteAxisPoint(axis, static_cast<std::uint32_t>(i));
            compat::Slice<float> nextCurrent;
            nextCurrent.resize(current.size() - 1);
            for (std::size_t j = 0; j < nextCurrent.size(); ++j)
                nextCurrent[j] = current[j < i ? j : j + 1];
            current = nextCurrent;
            compat::Slice<bool> nextMatched;
            nextMatched.resize(cm.size() - 1);
            for (std::size_t j = 0; j < nextMatched.size(); ++j)
                nextMatched[j] = cm[j < i ? j : j + 1];
            cm = nextMatched;
            --cn;
        } else ++i;
    }
    for (std::size_t t = 1; t + 1 < tn; ++t) {
        if (sourceAxisAt(tm, t, 179)) continue;
        if (sourceAxisAt(target, t, 180) <= 0 || sourceAxisAt(target, t, 180) >= 1) continue;
        compat::ngRequireObject(param)->insertAxisPoint(axis, sourceAxisAt(target, t, 182));
    }
    cn = sourceAxisAt(compat::ngRequireObject(param)->axisPoints, axis, 188).size();
    for (std::size_t i = 1; i + 1 < cn && i + 1 < tn; ++i) {
        auto points = sourceAxisAt(compat::ngRequireObject(param)->axisPoints, axis, 191);
        auto& destination = sourceAxisAt(points, i, 191);
        destination = sourceAxisAt(target, i, 191);
    }
    return true;
}
template<class Range> std::vector<float> halfCell(const Range& values) {
    auto result = allocateArray([&] { return std::vector<float>(values.size()); });
    for (std::size_t i = 0; i < values.size(); ++i) {
        const float a = values[i];
        const float left = i > 0 ? a - values[i - 1] : (values.size() > 1 ? values[1] - a : 1.0f);
        const float right = i + 1 < values.size() ? values[i + 1] - a :
            (values.size() > 1 ? a - values[i - 1] : 1.0f);
        result[i] = (left < right ? left : right) * 0.5f;
    }
    return result;
}
template<class Range> [[maybe_unused]] std::size_t nearestIndex(const Range& values, float value) {
    std::size_t best = 0;
    float distance = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < values.size(); ++i) {
        float d = values[i] - value; if (d < 0) d = -d;
        if (d < distance) { distance = d; best = i; }
    }
    return best;
}
template<class Snapshot, class Range> void assignMatches(Snapshot& s, const Range& nx, const Range& ny,
    const Range& ox, const Range& oy) {
    constexpr std::size_t matchLine = std::same_as<Snapshot, detail::SnapF> ? 251 :
        (std::same_as<Snapshot, detail::SnapP> ? 270 : 289);
    s.b->clear();
    compat::Slice<bool> matchedNew, matchedOld;
    matchedNew.resize(nx.size() * ny.size());
    matchedOld.resize(ox.size() * oy.size());
    std::vector<detail::pair_sort::Pair> pairs;
    for (std::size_t x = 0; x < nx.size(); ++x) for (std::size_t y = 0; y < ny.size(); ++y)
        for (std::size_t a = 0; a < ox.size(); ++a) for (std::size_t b = 0; b < oy.size(); ++b) {
            float dx = nx[x] - ox[a]; if (dx < 0) dx = -dx;
            float dy = ny[y] - oy[b]; if (dy < 0) dy = -dy;
            allocateArray([&] { pairs.push_back({{x, y, a, b, dx + dy}}); });
        }
    detail::pair_sort::sort(pairs);
    for (const auto p : pairs) {
        const auto n = p.xi * ny.size() + p.yi, o = p.ox * oy.size() + p.oy;
        if (sourceAt(matchedNew, n, matchLine) || sourceAt(matchedOld, o, matchLine)) continue;
        sourceAt(matchedNew, n, matchLine + 1) = true;
        sourceAt(matchedOld, o, matchLine + 1) = true;
        const runtime::Vec2u key{static_cast<std::uint32_t>(p.xi), static_cast<std::uint32_t>(p.yi)};
        if (sourceAt(sourceAt(s.set, p.ox, matchLine + 2), p.oy, matchLine + 2)) {
            const auto receiver = s.b;
            // The source virtual argument is by value, including Deformation postblit.
            const auto sample = sourceAt(sourceAt(s.vals, p.ox, matchLine + 2), p.oy, matchLine + 2);
            receiver->setValue(key, sample);
        } else s.b->unset(key);
    }
    s.b->reInterpolate();
}
}
SetParameterNameCommand::SourceConstructorArguments
SetParameterNameCommand::sourceConstructorArguments(const compat::RetainedStringSlice& name) {
    auto description = core::translation::ngTranslateRetained(literal("Set Parameter Name"));
    return {std::move(description), name};
}
SetParameterNameCommand::SetParameterNameCommand(compat::RetainedStringSlice name)
    : SetParameterNameCommand(sourceConstructorArguments(name)) {}
SetParameterNameCommand::SetParameterNameCommand(SourceConstructorArguments arguments)
    : ExCommand({}, arguments.description, arguments.newName) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SetParameterNameCommand),
        literal("nijigenerate.commands.parameter.prop.SetParameterNameCommand"));
}
void SetParameterNameCommand::prepareSourceClassInfo() {
    ExCommand<SetParameterNameArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetParameterNameCommand>();
}
ApplyParameterPropsAxesCommand::SourceConstructorArguments
ApplyParameterPropsAxesCommand::sourceConstructorArguments(const std::array<float, 2>& lo,
    const std::array<float, 2>& hi, const compat::Slice<float>& x, const compat::Slice<float>& y) {
    auto description = core::translation::ngTranslateRetained(literal("Apply Parameter Axes + Props"));
    return {std::move(description), lo, hi, x, y};
}
ApplyParameterPropsAxesCommand::ApplyParameterPropsAxesCommand(std::array<float, 2> lo,
    std::array<float, 2> hi, compat::Slice<float> x, compat::Slice<float> y)
    : ApplyParameterPropsAxesCommand(sourceConstructorArguments(lo, hi, x, y)) {}
ApplyParameterPropsAxesCommand::ApplyParameterPropsAxesCommand(SourceConstructorArguments arguments)
    : ExCommand({}, arguments.description, arguments.min, arguments.max, arguments.axisX, arguments.axisY) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ApplyParameterPropsAxesCommand),
        literal("nijigenerate.commands.parameter.prop.ApplyParameterPropsAxesCommand"));
}
void ApplyParameterPropsAxesCommand::prepareSourceClassInfo() {
    ExCommand<ApplyParameterPropsAxesArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ApplyParameterPropsAxesCommand>();
}
std::shared_ptr<CommandResult> SetParameterNameCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters()) return CommandResult::opCall(false, literal("No parameters"));
    const auto param = parameterAtZero(context, 22);
    const auto puppet = std::dynamic_pointer_cast<ext::ExPuppet>(context->hasPuppet() ? context->puppet() : nullptr);
    if (puppet) {
        const auto found = puppet->findParameter(newName.retainedString());
        if (found) {
            const auto foundUuid = found->getUUID();
            const auto parameterUuid = compat::ngRequireObject(param)->getUUID();
            if (foundUuid != parameterUuid) return CommandResult::opCall(false, literal("Name already taken"));
        }
    }
    const auto oldName = compat::ngRequireObject(param)->getName();
    compat::ngRequireObject(param)->setName(newName.retainedString().toNullableString());
    compat::ngRequireObject(param)->makeIndexable();
    core::actionstack::incActionPush(std::allocate_shared<actions::parameter::ParameterNameChangeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::parameter::ParameterNameChangeAction>{},
        param, oldName, newName.retainedString().toNullableString()));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ApplyParameterPropsAxesCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters()) return CommandResult::opCall(false, literal("No parameters"));
    const auto param = parameterAtZero(context, 57);
    const auto action = std::allocate_shared<actions::parameter::ParameterShapeChangeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::parameter::ParameterShapeChangeAction>{}, literal("axes"), param);
    const runtime::Vec2 newMinimum{min[0], min[1]}, newMaximum{max[0], max[1]};
    const auto oldX = sourceAt(compat::ngRequireObject(param)->axisPoints, 0, 65).duplicate();
    const auto oldY = sourceAt(compat::ngRequireObject(param)->axisPoints, 1, 66).duplicate();
    compat::Slice<detail::SnapF> floats;
    compat::Slice<detail::SnapP> parameters;
    compat::Slice<detail::SnapD> deformations;
    const auto originalBindings = compat::ngRequireObject(param)->bindings;
    for (const auto binding : originalBindings) {
        if (auto value = std::dynamic_pointer_cast<runtime::ValueParameterBinding>(binding))
            floats.append(snapshot<detail::SnapF>(value, oldX.size(), oldY.size(), 74));
        else if (auto value = std::dynamic_pointer_cast<runtime::ParameterParameterBinding>(binding))
            parameters.append(snapshot<detail::SnapP>(value, oldX.size(), oldY.size(), 78));
        else if (auto value = std::dynamic_pointer_cast<runtime::DeformationParameterBinding>(binding))
            deformations.append(snapshot<detail::SnapD>(value, oldX.size(), oldY.size(), 82));
    }
    // Preserve the original mutation-before-validation ordering, including failure paths.
    param->min = newMinimum; param->max = newMaximum;
    compat::RetainedStringSlice message;
    if (axisX.size() >= 2 && !applyAxis(param, 0, axisX, message))
        return CommandResult::opCall(false, message.retainedString());
    if (param->isVec2 && axisY.size() >= 2 && !applyAxis(param, 1, axisY, message))
        return CommandResult::opCall(false, message.retainedString());
    const auto newX = sourceAt(param->axisPoints, 0, 205), newY = sourceAt(param->axisPoints, 1, 206);
    [[maybe_unused]] const auto halfX = halfCell(oldX), halfY = halfCell(oldY);
    // The source computes half cells but does not filter assignments by tolerance.
    for (auto& s : floats) assignMatches(s, newX, newY, oldX, oldY);
    for (auto& s : parameters) assignMatches(s, newX, newY, oldX, oldY);
    for (auto& s : deformations) assignMatches(s, newX, newY, oldX, oldY);
    action->updateNewState(); core::actionstack::incActionPush(action);
    if (project::incArmedParameter() == param) binding::paramPointChanged(param);
    viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged();
    return CommandResult::opCall(true);
}
void ngInitParameterPropCommands() {
    initialization::Program::run<initialization::Table>();
}
}
