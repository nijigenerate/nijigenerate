#pragma once
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nodes/grid_axis_sort.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <limits>
namespace nijigenerate::panels::inspector::griddeform {
template<ModelEditSubMode mode,class T> requires std::derived_from<T,GridDeformer>
void NodeInspector<mode,T>::run() {
    if constexpr(mode==ModelEditSubMode::Layout) {
        using namespace widgets;
        using core::translation::ngTranslateCString;
        if(this->targets.empty()) return;
        auto self=std::static_pointer_cast<Self>(this->shared_from_this());
        auto node=this->targets[0];
        if(ngBeginCategory(ngTranslateCString("GridDeformer").data())) {
            ImGui::Spacing();
            if(this->template shared<DynamicDescriptor<T>>([&] { return dynamic.get(self); },[&] {
                const auto label=ngTranslateCString("Dynamic Deformation (slower)");
                const auto state=dynamic.get(self);
                return ngCheckbox(label.data(),&state->value);
            })) {
                const auto context=std::allocate_shared<commands::Context>(
                    nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
                context->inspectors(commands::Context::Inspectors{self});
                context->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets)));
                const auto state=dynamic.get(self);
                ngDispatchGridDynamicCommand(context,state->value);
            }
            ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Whether the GridDeformer should dynamically deform its targets. Enabling this has a performance cost."))));
            ImGui::Spacing(); auto vertices=compat::ngRequireObject(node)->getVertices();
            ImGui::TextColored(CategoryTextColor,ngTranslateCString("Grid Information").data());
            ImGui::Text(ngTranslateCString("Vertices: %d").data(),static_cast<int>(vertices.size()));
            std::size_t columns=0,rows=0; bool valid=false;
            if(vertices.size()>=4) {
                const auto baseArray=vertices.toArray();
                compat::Slice<float> xs,ys;
                xs.resize(baseArray.size(),std::numeric_limits<float>::quiet_NaN());
                for(std::size_t i=0;i<baseArray.size();++i) xs[i]=baseArray[i].x;
                ys.resize(baseArray.size(),std::numeric_limits<float>::quiet_NaN());
                for(std::size_t i=0;i<baseArray.size();++i) ys[i]=baseArray[i].y;
                nicxlive::core::nodes::grid_axis_detail::sort({xs.data(),xs.size()});
                nicxlive::core::nodes::grid_axis_detail::sort({ys.data(),ys.size()});
                const auto uniqueArray=[](compat::Slice<float> sorted) {
                    compat::Slice<float> result;
                    for(std::size_t i=0;i<sorted.size();) {
                        const auto previous=sorted[i];
                        result.append(previous);
                        do { ++i; } while(i<sorted.size()&&previous==sorted[i]);
                    }
                    return result;
                };
                xs=uniqueArray(xs);
                ys=uniqueArray(ys);
                columns=xs.size();rows=ys.size();valid=columns>=2&&rows>=2&&columns*rows==vertices.size();
            }
            if(valid) {ImGui::Text(ngTranslateCString("Columns: %d").data(),static_cast<int>(columns));
                ImGui::Text(ngTranslateCString("Rows: %d").data(),static_cast<int>(rows));}
            else ImGui::Text(ngTranslateCString("Grid axes are not initialized.").data());
        }
        ngEndCategory(); ngDrawGridDepthBoneSources(node,{},Cursor{},sessions);
    }
}
}
