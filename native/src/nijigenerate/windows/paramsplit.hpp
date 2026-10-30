#pragma once
#include "base.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <common/resource_key_map.hpp>
#include <param/parameter.hpp>
#include <resource.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace nijigenerate::windows::paramsplit {
using ParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;
using BindingRef = std::shared_ptr<nicxlive::core::param::ParameterBinding>;
using Bindings = nicxlive::core::common::SharedArray<BindingRef>;
struct ParamMapping {
    std::size_t idx = 0;
    Bindings bindings;
    std::shared_ptr<nicxlive::core::Resource> node;
    bool take = false;
};
ParameterRef ngSplitParameterBindings(std::size_t idx, ParameterRef param,
    compat::Slice<std::uint32_t> takeNodeUUIDs, bool pushAction = true);
class ParamSplitWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    std::size_t idx = 0;
    ParameterRef param;
    struct MappingKeyHash {
        std::size_t operator()(std::uint32_t key) const noexcept { return key; }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint32_t, ParamMapping,
        MappingKeyHash, std::equal_to<std::uint32_t>> mappings;
    std::uint32_t mappingKeyAt(std::size_t index, std::size_t sourceLine);
    ParamMapping& mappingAt(std::uint32_t key, std::size_t sourceLine);
    void initialize(std::size_t index, ParameterRef parameter);
    void buildMapping();
    void apply();
    void oldBindingsList();
    void newBindingsList();
protected:
    void onBeginUpdate() override;
    void onUpdate() override;
public:
    ParamSplitWindow() = default;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::paramsplit::ParamMapping> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.paramsplit.ParamMapping)";
    inline static constexpr char unqualifiedArgument[] = "(ParamMapping)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.paramsplit.ParamMapping";
    inline static constexpr char unqualifiedElement[] = "ParamMapping";
};
}
