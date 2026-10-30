#pragma once

#include "nijigenerate/actions/depth.hpp"
#include "nijigenerate/commands/command.hpp"
#include "nijigenerate/commands/command_table.hpp"
#include "nijigenerate/core/selector/tokenizer.hpp"
#include "nijigenerate/io/depthmap_psd_values.hpp"
#include "nijigenerate/viewport/depth/draw/gpu.hpp"
#include "nijigenerate/core/settings_value.hpp"
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <cstdint>
#include <functional>

namespace nijigenerate::commands::depth::map {
namespace detail {
struct PsdDepthAccess;
struct DepthMapStringHash {
    std::size_t operator()(const compat::RetainedStringSlice& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
struct DepthMapStringEqual {
    bool operator()(const compat::RetainedStringSlice& a, const compat::RetainedStringSlice& b) const noexcept {
        return a.view() == b.view();
    }
};
struct DepthMapIntegerHash {
    template<class T> std::size_t operator()(T value) const noexcept {
        if constexpr (sizeof(T) > sizeof(std::size_t))
            return static_cast<std::size_t>(value) ^ static_cast<std::size_t>(value >> 32);
        else return static_cast<std::size_t>(value);
    }
};
template<class K, class V> using DepthMapIntegerMap = nicxlive::core::common::ResourceKeyMap<
    K, V, DepthMapIntegerHash, std::equal_to<K>>;
template<class V> using DepthMapStringMap = nicxlive::core::common::ResourceKeyMap<
    compat::RetainedStringSlice, V, DepthMapStringHash, DepthMapStringEqual>;
}

enum class DepthMapCommand : std::int32_t {
    ListDepths,
    SetDepths,
    ClearDepths,
    ListDepthOps,
    SetDepthOps,
    AddDepthOp,
    UpdateDepthOp,
    RemoveDepthOp,
    MoveDepthOp,
    ClearDepthOps,
    ApplyDepthOps,
    ImportPSDDepths,
};

using DepthCommandMap = ::nijigenerate::commands::CommandTableStorage<DepthMapCommand>;

// Source initialization is explicit ngInitCommands, not a module constructor.
extern thread_local DepthCommandMap commands;

struct PsdDepthComposedView {
private:
    io::depthmap_psd::PsdDepthImportResult* imported{};
    friend struct detail::PsdDepthAccess;
#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
    friend PsdDepthComposedView ngPsdDepthComposedViewForRegression(
        io::depthmap_psd::PsdDepthImportResult& imported);
#endif
};

// The by-value PSD grid and GPU job fields require their actual providers.
struct PsdDepthGpuGridComposeWork {
    std::size_t gridIndex{};
    io::depthmap_psd::PsdDepthGridResult sourceGrid;
    compat::Slice<float> oldDepths;
    detail::DepthMapStringMap<compat::RetainedString> layerIdToPath;
    compat::Slice<viewport::depth::draw::DepthDrawLayer> previewLayers;
    viewport::depth::draw::DepthDrawGpuTargetComposeJob job;
    bool completed{};
};
struct PsdDepthGpuComposeWork {
private:
    compat::Slice<PsdDepthGpuGridComposeWork> grids;
    compat::Slice<io::depthmap_psd::PsdDepthGridResult> composedGrids;
    bool active{};
    friend struct detail::PsdDepthAccess;
    friend bool ngSubmitPsdDepthImportGpu(io::depthmap_psd::PsdDepthImportResult& imported,
        PsdDepthGpuComposeWork& work, compat::RetainedString& error);
    friend bool ngPollPsdDepthImportGpu(io::depthmap_psd::PsdDepthImportResult& imported,
        PsdDepthGpuComposeWork& work, bool& ready, PsdDepthComposedView& composed,
        compat::RetainedString& error);
    friend void ngCancelPsdDepthImportGpu(PsdDepthGpuComposeWork& work);
public:
    bool isActive() const;
};

// PSD composition preserves the caller-owned import result through a borrowed view.
bool ngComposePsdDepthTarget(io::depthmap_psd::PsdDepthImportResult& imported,
    io::depthmap_psd::PsdDepthGridResult& gridResult, io::depthmap_psd::PsdDepthGridResult& composed,
    compat::RetainedString& error);
bool ngComposePsdDepthImportResult(io::depthmap_psd::PsdDepthImportResult& imported,
    PsdDepthComposedView& composed, compat::RetainedString& error);
bool ngSubmitPsdDepthImportGpu(io::depthmap_psd::PsdDepthImportResult& imported,
    PsdDepthGpuComposeWork& work, compat::RetainedString& error);
bool ngPollPsdDepthImportGpu(io::depthmap_psd::PsdDepthImportResult& imported,
    PsdDepthGpuComposeWork& work, bool& ready, PsdDepthComposedView& composed,
    compat::RetainedString& error);
void ngCancelPsdDepthImportGpu(PsdDepthGpuComposeWork& work);
bool ngCanApplyPsdDepthImportResult(PsdDepthComposedView composed, compat::RetainedString& error);

// Full command types and original field metadata are exposed below.
class ListDepthsCommand;
class SetDepthsCommand;
class ClearDepthsCommand;
class ListDepthOpsCommand;
class SetDepthOpsCommand;
class AddDepthOpCommand;
class UpdateDepthOpCommand;
class RemoveDepthOpCommand;
class MoveDepthOpCommand;
class ClearDepthOpsCommand;
class ApplyDepthOpsCommand;
class ImportPSDDepthsCommand;

#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
PsdDepthComposedView ngPsdDepthComposedViewForRegression(io::depthmap_psd::PsdDepthImportResult& imported);
bool ngPsdDepthImportProgressVisibleForRegression();
#endif

std::shared_ptr<actions::depth::DepthMappedChangeAction> ngApplyDepthsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::Slice<float> nextDepths, compat::RetainedStringSlice reason);

core::settings::SettingsJsonValue ngDepthOpToJson(ext::nodes::ExDepthOp op);
core::settings::SettingsJsonValue ngPsdDepthImportSummaryToJson(
    io::depthmap_psd::PsdDepthImportResult imported, std::size_t changedGrids);
std::shared_ptr<ExCommandResult<core::settings::SettingsJsonValue>> ngApplyPsdDepthImportResult(
    PsdDepthComposedView composed);

namespace detail {
using LayerMap = decltype(PsdDepthGpuGridComposeWork::layerIdToPath);
struct PsdDepthAccess {
    static auto& imported(PsdDepthComposedView& value) { return value.imported; }
    static auto& grids(PsdDepthGpuComposeWork& value) { return value.grids; }
    static auto& composedGrids(PsdDepthGpuComposeWork& value) { return value.composedGrids; }
    static auto& active(PsdDepthGpuComposeWork& value) { return value.active; }
};
}

} // namespace nijigenerate::commands::depth::map

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_MAP_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::map::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.map." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.map." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_MAP_VALUE_ORIGIN(DepthMapCommand);
NG_DEPTH_MAP_VALUE_ORIGIN(PsdDepthComposedView);
NG_DEPTH_MAP_VALUE_ORIGIN(PsdDepthGpuGridComposeWork);
NG_DEPTH_MAP_VALUE_ORIGIN(PsdDepthGpuComposeWork);
#undef NG_DEPTH_MAP_VALUE_ORIGIN
}

#include "map_commands.hpp"
#include "map_composition.hpp"
