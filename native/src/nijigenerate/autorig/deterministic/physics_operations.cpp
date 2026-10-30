#include "detail/physics.hpp"
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <bit>
namespace nijigenerate::autorig::deterministic::physics {
using namespace detail;
namespace {
template<class Array> decltype(auto) arrayAt(Array& values, std::size_t index, std::size_t sourceLine) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
        sourceLine);
    return (values[index]);
}
Slice<bool> boolSlice(Slice<bool> values, std::size_t lower, std::size_t upper, std::size_t sourceLine) {
    if (lower > upper || upper > values.size()) throw compat::ngStaticError<compat::ArraySliceError>(
        lower, upper, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
        sourceLine);
    return values.slice(lower, upper);
}
int integer(double value) { return compat::ngFloatToInt32(value); }
int dimension(std::size_t v){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(v));}
// Preserve the original int arithmetic before conversion to double or size_t.
static_assert(sizeof(int)==sizeof(std::uint32_t));
int wrapAdd32(int a,int b) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));
}
int wrapSub32(int a,int b) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));
}
bool touchesProtectedPixels(std::array<Point2,3> polygon,Slice<bool> mask,
        std::size_t width,std::size_t height) {
    std::array<std::array<int,2>,3> points;
    for(std::size_t i=0;i<3;++i) points[i]={integer(polygon[i][0]),integer(polygon[i][1])};
    const auto low=std::max(0,std::min(points[0][1],std::min(points[1][1],points[2][1])));
    const auto high=std::min(wrapSub32(dimension(height),1),
        std::max(points[0][1],std::max(points[1][1],points[2][1])));
    const auto endY=wrapAdd32(high,1);
    for(auto y=low;y<endY;y=wrapAdd32(y,1)) {
        Slice<double> intersections;
        for(std::size_t i=0;i<3;++i) {
            const auto a=points[i],b=points[(i+1)%3];
            if(a[1]==b[1]) {
                if(y==a[1]) {intersections.append(a[0]);intersections.append(b[0]);}
            } else if(y>=std::min(a[1],b[1])&&y<=std::max(a[1],b[1])) {
                intersections.append(a[0]+static_cast<double>(wrapSub32(y,a[1]))*
                    wrapSub32(b[0],a[0])/wrapSub32(b[1],a[1]));
            }
        }
        if(intersections.empty()) continue;
        controls_double_sort::sort({intersections.data(),intersections.size()});
        const auto x0=std::max(0,integer(std::ceil(intersections[0]-.5)));
        const auto x1=std::min(wrapSub32(dimension(width),1),
            integer(std::floor(intersections[intersections.size()-1]+.5)));
        const auto endX=wrapAdd32(x1,1);
        for(auto x=x0;x<endX;x=wrapAdd32(x,1))
            if(arrayAt(mask,static_cast<std::size_t>(y)*width+x,518)) return true;
    }
    return false;
}
Point2 sampleHost(Json host,Point2 key,Point2 anchor,const PhysicsMeshes& meshes){
    auto id=ngRigUnsigned(host.sourceIndexValue("target"));auto mesh=physicsMapValue(meshes,id,524);Slice<Point2> positions,values;
    for(auto v:mesh.vertices)positions.append(ngRigPhysicsTransform(mesh.toModel,v));
    for(auto op:host.sourceIndexValue("operations").sourceArrayValue())if(ngRigUnsigned(op.sourceIndexValue("target"))==id){
        auto k=ngRigPoint(op.sourceIndexValue("key"));if(k[0]!=key[0]||k[1]!=key[1])continue;
        auto offsets=ngRigNumbers(op.sourceIndexValue("values"));
        for(std::size_t i=0;i<offsets.size();i+=2)
            values.append(ngRigPhysicsTransform(mesh.toModel,
                {arrayAt(offsets,i,530),arrayAt(offsets,i+1,530)},true));break;}
    enforce(values.size()==positions.size(),"Physics host key is missing",533);
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        auto ia=arrayAt(mesh.indices,i,535),ib=arrayAt(mesh.indices,i+1,535),ic=arrayAt(mesh.indices,i+2,535);
        auto a=arrayAt(positions,ia,536),b=sub(arrayAt(positions,ib,536),a),
            c=sub(arrayAt(positions,ic,536),a),q=sub(anchor,a);
        auto determinant=b[0]*c[1]-c[0]*b[1];if(std::abs(determinant)<1e-8)continue;
        double v=(q[0]*c[1]-c[0]*q[1])/determinant,w=(b[0]*q[1]-q[0]*b[1])/determinant;
        if(dmin(1-v-w,dmin(v,w))>=-1e-6)return {
            (1-v-w)*arrayAt(values,ia,540)[0]+v*arrayAt(values,ib,540)[0]+w*arrayAt(values,ic,540)[0],
            (1-v-w)*arrayAt(values,ia,541)[1]+v*arrayAt(values,ib,541)[1]+w*arrayAt(values,ic,541)[1]};
    }
    std::size_t closest=0;double distance=INFINITY;for(std::size_t i=0;i<positions.size();++i){auto d=norm(sub(positions[i],anchor));if(d<distance){distance=d;closest=i;}}return arrayAt(values,closest,545);
}
}
Json ngRigCompilePhysicsOperations(Json& structure,PhysicsAssets assets,PhysicsMeshes meshes,std::shared_ptr<framework::AutoRigTaskContext> task){
    auto policy=ngRigPhysicsPolicy();Array groups,checks;
    for(auto source:structure.sourceIndexValue("groups").sourceArrayValue()){
        auto spec=duplicate(source);Array operations,groupChecks,targets;
        for(auto target:source.sourceIndexValue("targets").sourceArrayValue()){
            ngRigCheckpoint(task);auto id=ngRigUnsigned(target);auto mesh=physicsMapValue(meshes,id,555);auto asset=physicsMapValue(assets,id,555);auto inverse=ngRigPhysicsInverse(mesh.toModel);Slice<Point2> p;
            for(auto v:mesh.vertices)p.append(ngRigPhysicsTransform(mesh.toModel,v));auto weights=ngRigPhysicsWeights(spec,p,policy);auto dense=duplicate(spec);
            if(!get(spec,"mixed_anatomy_cloth",false).as_bool()){set(dense,"limb",Json{});set(dense,"protected_pixels",Array{});}
            auto cloudWeights=ngRigPhysicsWeights(dense,asset.points,policy);auto fixedMask=physics_detail::newArray<bool>(asset.mask.size());std::size_t cloudIndex=0;
            for(std::size_t i=0;i<asset.mask.size();++i)if(asset.mask[i])
                fixedMask[i]=arrayAt(cloudWeights,cloudIndex++,565)<=1e-12;
            auto modelToPixel=asset.affineMapping?ngRigPhysicsInverse(asset.pixelToModel):PhysicsAffineTransform{1.,0.,0.,0.,1.,0.};auto fixedVertices=physics_detail::newArray<bool>(p.size());
            for(std::size_t i=0;i<mesh.indices.size();i+=3){std::array<Point2,3> q;
                for(std::size_t j=0;j<3;++j){auto vertex=arrayAt(mesh.indices,i+j,571);
                    if(asset.affineMapping)q[j]=ngRigPhysicsTransform(modelToPixel,arrayAt(p,vertex,572));
                    else{enforce(mesh.uv.size()==mesh.vertices.size(),"Physics UV count differs from mesh",574);
                        q[j]={arrayAt(mesh.uv,vertex,575)[0]*asset.width,arrayAt(mesh.uv,vertex,575)[1]*asset.height};}
                    q[j][0]-=.5;q[j][1]-=.5;}
                auto x0=std::max(0,integer(std::floor(dmin(q[0][0],dmin(q[1][0],q[2][0]))))),y0=std::max(0,integer(std::floor(dmin(q[0][1],dmin(q[1][1],q[2][1])))));
                auto x1=std::min(wrapSub32(dimension(asset.width),1),
                    integer(std::ceil(dmax(q[0][0],dmax(q[1][0],q[2][0])))));
                auto y1=std::min(wrapSub32(dimension(asset.height),1),
                    integer(std::ceil(dmax(q[0][1],dmax(q[1][1],q[2][1])))));
                if(x1<x0||y1<y0)continue;
                auto width=static_cast<std::size_t>(wrapAdd32(wrapSub32(x1,x0),1));
                auto height=static_cast<std::size_t>(wrapAdd32(wrapSub32(y1,y0),1));
                auto mask=physics_detail::newArray<bool>(width*height);
                for(std::size_t y=0;y<height;++y) {
                    auto destinationRow=boolSlice(mask,y*width,(y+1)*width,587);
                    const auto sourceStart=(y+y0)*asset.width+x0;
                    const auto sourceRow=boolSlice(fixedMask,sourceStart,sourceStart+width,588);
                    for(std::size_t x=0;x<width;++x)destinationRow[x]=sourceRow[x];
                }
                for(auto& point:q){point[0]-=x0;point[1]-=y0;}
                if(touchesProtectedPixels(q,mask,width,height))for(std::size_t j=0;j<3;++j)
                    arrayAt(fixedVertices,arrayAt(mesh.indices,i+j,591),591)=true;
            }
            Slice<Slice<Point2>> values;Slice<Point2> keys;
            for(double x:{-1.,0.,1.})for(double y:{-1.,0.,1.}){Point2 key{x,y};keys.append(key);
                auto delta=ngRigPhysicsProtectTriangles(ngRigPhysicsDisplacement(spec,p,key,policy),p,mesh.indices,dense,policy);
                for(std::size_t i=0;i<delta.size();++i)
                    if(arrayAt(weights,i,597)<=1e-12||arrayAt(fixedVertices,i,597))delta[i]={0.,0.};
                for(auto& value:delta)value=ngRigPhysicsTransform(inverse,value,true);values.append(delta);
            }
            double scale=1.;auto preserved=[&](double amplitude){for(auto value:values)if(!ngRigPhysicsNoInversion(mesh.vertices,mesh.indices,value,amplitude))return false;return true;};
            while(scale>=1./128&&!preserved(scale))scale*=.5;enforce(scale>=1./128,"Physics field cannot preserve existing mesh topology",607);
            std::size_t moving=0,fixed=0;double maximum=0;for(auto value:arrayAt(values,0,609))moving+=std::abs(value[0]*scale)>1e-6||std::abs(value[1]*scale)>1e-6;for(auto weight:weights)fixed+=weight==0;
            if(!moving){
                Object retainedInventory;
                for(auto& row:structure.sourceIndexReference("inventory",retainedInventory).as_array())
                    if(ngRigUnsigned(row.sourceIndexValue("target"))==id){
                        set(row,"decision",id==ngRigUnsigned(source.sourceIndexValue("target"))?"unresolved":"exclude");
                        set(row,"reason","Existing mesh has no resolved free vertices; retain fully fixed region");
                    }
                continue;
            }
            targets.append(target);
            for(std::size_t i=0;i<values.size();++i){Slice<double> flat;Slice<Point2> quantized;
                for(auto v:values[i]){maximum=dmax(maximum,norm(v)*scale);Point2 q{std::rint(v[0]*scale*1e6)/1e6,std::rint(v[1]*scale*1e6)/1e6};quantized.append(q);flat.append(q[0]);flat.append(q[1]);}
                enforce(ngRigPhysicsNoInversion(mesh.vertices,mesh.indices,quantized),
                    "Quantized physics key inverts a face",626);
                operations.append(object({
                    {jsonLiteral("target"),target},
                    {jsonLiteral("key"),point(arrayAt(keys,i,627))},
                    {jsonLiteral("values"),array(flat)}}));
            }
            groupChecks.append(object({
                {jsonLiteral("target"),target},
                {jsonLiteral("moving_vertices"),moving},
                {jsonLiteral("fixed_vertices"),fixed},
                {jsonLiteral("amplitude_topology_scale"),scale},
                {jsonLiteral("triangles_preserved"),true},
                {jsonLiteral("neutral_zero"),true},
                {jsonLiteral("maximum_displacement"),maximum}}));
        }
        bool primary=false;for(auto target:targets)primary|=ngRigUnsigned(target)==ngRigUnsigned(source.sourceIndexValue("target"));if(!primary)continue;
        set(spec,"operations",operations);set(spec,"targets",targets);set(spec,"checks",groupChecks);
        groups.append(spec);checks.appendRange(groupChecks);
    }
    PhysicsStringMap<std::size_t> byId;
    for(std::size_t i=0;i<groups.size();++i)
        physicsMapSlot(byId,groups[i].sourceIndexValue("id").as_string())=i;
    for(auto child:groups){auto support=stringGet(child,"support_group");auto index=byId.find(support);if(index==byId.end())continue;auto host=arrayAt(groups,index->second,642);
        const auto childTargets=child.sourceIndexValue("targets").sourceArrayValue();
        for(auto target:childTargets){bool exists=false;for(auto id:host.sourceIndexValue("targets").sourceArrayValue())exists|=ngRigUnsigned(id)==ngRigUnsigned(target);if(exists)continue;
            auto mesh=physicsMapValue(meshes,ngRigUnsigned(target),646);auto inverse=ngRigPhysicsInverse(mesh.toModel);Array carried;bool moving=false;
            for(double x:{-1.,0.,1.})for(double y:{-1.,0.,1.}){Point2 key{x,y};auto delta=sampleHost(host,key,ngRigPoint(child.sourceIndexValue("support_anchor")),meshes),local=ngRigPhysicsTransform(inverse,delta,true);Slice<double> values;
                for(auto v:mesh.vertices)for(auto component:local){auto q=std::rint(component*1e6)/1e6;values.append(q);moving|=q!=0;}
                carried.append(object({
                    {jsonLiteral("target"),target},
                    {jsonLiteral("key"),point(key)},
                    {jsonLiteral("values"),array(values)},
                    {jsonLiteral("carried"),true}}));
            }
            if(!moving)continue;
            Object retainedOperations;
            host.sourceIndexReference("operations",retainedOperations).as_array().appendRange(carried);
            Object retainedTargets;
            host.sourceIndexReference("targets",retainedTargets).as_array().append(target);
        }
    }
    return object({
        {jsonLiteral("policy"),policy},
        {jsonLiteral("groups"),groups},
        {jsonLiteral("checks"),checks}});
}
}
