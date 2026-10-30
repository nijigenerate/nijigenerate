#include "file_runtime.hpp"
#include <nijigenerate/io/psd/document.hpp>
#include <nijigenerate/io/kra/document.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/windows/kramerge_path.hpp>
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nodes/projectable.hpp>
#include <bit>
#include <numeric>
#include <texture.hpp>
namespace nijigenerate::commands::puppet::file::detail {
namespace {
using Node=nicxlive::core::nodes::Node;
using Part=nicxlive::core::nodes::Part;
using Drawable=nicxlive::core::nodes::Drawable;
using ExPart=ext::nodes::ExPart;
using Texture=nicxlive::core::Texture;
using Vec3=nicxlive::core::math::Vec3;
using Mat4=nicxlive::core::math::Mat4;
template<bool PSD> void updatePart(std::shared_ptr<Node> node) {
    auto part=std::dynamic_pointer_cast<Part>(node);
    const auto projectable=std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(node);
    if(part && (!PSD || !projectable)) {
        auto& sourceMesh=part->Drawable::getMesh();
        const auto sourceMeshOwner=part->mesh;
        auto mesh=viewport::common::mesh::IncMesh::create(sourceMesh,sourceMeshOwner);
        auto data=mesh->export_();data.fixWinding();
        for(std::size_t i=0;i<data.uvs.size();++i) {
            const auto tex=part->textures.at(0);
            data.uvs[i].x/=static_cast<float>(tex->width());
            data.uvs[i].y/=static_cast<float>(tex->height());
            data.uvs[i].x+=0.5f;data.uvs[i].y+=0.5f;
        }
        part->rebuffer(data);
    }
    const auto children=compat::ngRequireObject(node)->children;
    for(const auto child:children)updatePart<PSD>(child);
}
int addInt(int a,int b) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));}
template<bool PSD,class Document> void merge(Document& document,bool renameMapped,bool retranslateMapped,
        const std::shared_ptr<nicxlive::core::Puppet>& puppet,
        nicxlive::core::common::SharedArray<std::shared_ptr<ExPart>> parts) {
    using Layer=std::remove_cvref_t<decltype(document.layers[0])>;
    struct Binding { Layer layer;std::shared_ptr<Texture> texture;std::shared_ptr<Node> node;bool replace;String path; };
    std::vector<Binding> bindings;
    std::vector<String> segments;std::string segment;
    const auto find=[&](std::string_view path,bool byName)->std::shared_ptr<ExPart> {
        const auto currentParts=parts;
        for(const auto& part:currentParts) {
            const auto layerPath=compat::ngRequireObject(part)->layerPath;
            std::string fallback;
            if(layerPath.empty()) {
                const auto receiver=compat::ngRequireObject(part);
                const auto name=receiver->getName();
                fileAllocation([&] {
                    fallback.reserve(name.size()+1);
                    fallback.push_back('/');
                    fallback.append(name.view());
                });
            }
            const auto candidate=layerPath.empty()?std::string_view(fallback):layerPath.view();
            if(byName ? windows::kramerge::detail::baseName(candidate)==windows::kramerge::detail::baseName(path)
                      : candidate==path) return part;
        }
        return {};
    };
    const auto sourceLayers=document.layers;
    for(std::size_t ordinal=0;ordinal<sourceLayers.size();++ordinal) {
        auto layer=sourceLayers[PSD?sourceLayers.size()-1-ordinal:ordinal];
        if(layer.type!=decltype(layer.type)::Any) {
            bool open;
            if constexpr(PSD)open=layer.name.view()!="</Layer set>"&&layer.name.view()!="</Layer group>";
            else open=layer.type!=io::kra::LayerType::SectionDivider;
            if(open)fileAllocation([&] { segments.push_back(layer.name); });
            else if(!segments.empty())segments.pop_back();
            fileAllocation([&] {
                segment.clear();
                for(const auto& part:segments){segment+='/';segment+=part.view();}
            });
            continue;
        }
        layer.extractLayerImage();
        if constexpr(!PSD)if(layer.data.empty())continue;
        auto input=layer.data;
        auto pixels=nicxlive::core::SourceTextureBytes::retainedView(
            input.retentionOwner(),input.data(),input.size());
        nicxlive::core::inTexPremultiply(pixels);
        const auto width=std::bit_cast<std::int32_t>(layer.width());
        const auto height=std::bit_cast<std::int32_t>(layer.height());
        auto texture=Texture::createSource(pixels,width,height);
        layer.data={};
        const auto fullPath=fileAllocation([&] { return segment+"/"+std::string(layer.name.view()); });
        auto part=find(fullPath,false);if(!part)part=find(fullPath,true);
        const auto parent = part?std::static_pointer_cast<Node>(part):puppet->root;
        auto path = fileAllocation([&] { return String(fullPath); });
        fileAllocation([&] { bindings.push_back({layer,texture,parent,static_cast<bool>(part),path}); });
    }
    // Both original Binding value records exceed 32 bytes: Phobos' short-sort cutoff is 32.
    auto order=fileAllocation([&] { return std::vector<std::size_t>(bindings.size()); });
    std::iota(order.begin(),order.end(),0);
    const auto depth=[&](std::size_t index) {
        const auto& binding=bindings[index];
        const auto node=binding.node;
        const auto value=node?node->depth():0;
        return binding.replace?addInt(value,-1):value;
    };
    compat::phobos_index_sort::sort<64>(order,[&](auto a,auto b){return depth(a)<depth(b);},"Puppet layer depth sort");
    const int centerX=document.width/2,centerY=document.height/2;
    for(auto index:order) {
        auto binding=bindings[index];
        const auto width=std::bit_cast<std::int32_t>(binding.layer.width());
        const auto height=std::bit_cast<std::int32_t>(binding.layer.height());
        const Vec3 world{static_cast<float>(addInt(binding.layer.left,width/2))-static_cast<float>(centerX),
                         static_cast<float>(addInt(binding.layer.top,height/2))-static_cast<float>(centerY),0};
        auto local=binding.node?Node::getRelativePosition(binding.node->transformNoLock().matrix(),Mat4::translation(world)):world;
        if(binding.replace) {
            const auto parent=binding.node->parentPtr();
            local=parent?Node::getRelativePosition(parent->transformNoLock().matrix(),Mat4::translation(world)):world;
            binding.node->recalcTransform=true;
            const auto part=std::dynamic_pointer_cast<ExPart>(binding.node);
            if(renameMapped)compat::ngRequireObject(part)->setName(
                ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(binding.layer.name)));
            if(retranslateMapped)binding.node->localTransform.translation=
                compat::ngRequireObject(binding.node)->lockToRootValue()?world:local;
            part->textures.at(0)->dispose();part->textures.at(0)=binding.texture;part->layerPath=binding.path;
            if constexpr(PSD)if(parent)parent->notifyChange(binding.node,nicxlive::core::nodes::NotifyReason::StructureChanged);
        } else {
            auto part=ext::nodes::incCreateExPart(binding.texture,binding.node,
                ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(binding.layer.name)));
            part->layerPath=binding.path;part->localTransform.translation=local;
            if constexpr(PSD)if(const auto parent=binding.node->parentPtr())parent->notifyChange(binding.node,nicxlive::core::nodes::NotifyReason::StructureChanged);
        }
    }
    puppet->root->transformChanged();updatePart<PSD>(puppet->root);puppet->populateTextureSlots();
}
}
void ngMergePSDDefault(String path,bool renameMapped,bool retranslateMapped) {
    auto puppet=project::incActivePuppet();
    const auto parts=puppet->findNodesType<ExPart>(puppet->root);
    io::psd::RetainedFile file(path);
    compat::ngRunWithScopeExit([&] {
        auto document=io::psd::parseDocument(file);
        merge<true>(document,renameMapped,retranslateMapped,puppet,parts);
    },[&] { file.close(); });
}
void ngMergeKRADefault(String path,bool renameMapped,bool retranslateMapped) {
    auto puppet=project::incActivePuppet();
    const auto parts=puppet->findNodesType<ExPart>(puppet->root);
    auto document=io::kra::parseDocument(path);
    merge<false>(document,renameMapped,retranslateMapped,puppet,parts);
}
}
