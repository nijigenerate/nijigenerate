#include "detail/physics.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
namespace nijigenerate::autorig::deterministic::physics {
using namespace detail;
RigPhysicsAsset ngRigPhysicsAsset(Json material,std::shared_ptr<framework::AutoRigTaskContext> task) {
    const auto arrayAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
            sourceLine);
        return (values[index]);
    };
    const auto dimensions=ngRigNumbers(material.sourceIndexValue("texture_size"));
    auto width=static_cast<std::size_t>(arrayAt(dimensions,0,422)),
        height=static_cast<std::size_t>(arrayAt(dimensions,1,422));
    enforce(width && height && width<=SIZE_MAX/height,"Invalid physics alpha dimensions",423);
    RigPhysicsAsset result;result.width=width;result.height=height;result.mask=physics_detail::newArray<bool>(width*height);
    auto runs=material.sourceIndexValue("alpha_runs_32").sourceArrayValue();
    for(std::size_t i=0;i<runs.size();i+=2){
        auto start=static_cast<std::size_t>(ngRigUnsigned(arrayAt(runs,i,427))),
            count=static_cast<std::size_t>(ngRigUnsigned(arrayAt(runs,i+1,427)));
        enforce(start<=result.mask.size() && count<=result.mask.size()-start,"Invalid physics alpha runs",428);
        for(std::size_t j=start;j<start+count;++j)result.mask[j]=true;
    }
    auto mapping=material.sourceIndexValue("source_root_mapping");
    auto vertices=ngRigPoints(mapping.sourceIndexValue("vertices")),uv=ngRigPoints(mapping.sourceIndexValue("uv"));
    linear::RigMatrix design,values;
    for(std::size_t i=0;i<uv.size();++i){auto p=uv[i];
        auto designRow=physics_detail::newArray<double>(3);
        designRow[0]=p[0]*width;
        designRow[1]=p[1]*height;
        designRow[2]=1.;
        design.append(designRow);
        const auto& vertex=arrayAt(vertices,i,434);
        auto duplicatedVertex = [&vertex] {
            try {
                return linear::RigRow(vertex.begin(),vertex.end());
            } catch (const std::bad_alloc&) {
                physics_detail::duplicationFailed();
            } catch (const std::length_error&) {
                physics_detail::duplicationFailed();
            }
        }();
        values.append(duplicatedVertex);
    }
    auto fitted=linear::ngRigLeastSquares(design,values);
    result.pixelToModel={arrayAt(arrayAt(fitted,0,436),0,436),arrayAt(arrayAt(fitted,1,436),0,436),
        arrayAt(arrayAt(fitted,2,436),0,436),arrayAt(arrayAt(fitted,0,436),1,436),
        arrayAt(arrayAt(fitted,1,436),1,436),arrayAt(arrayAt(fitted,2,436),1,436)};
    result.affineMapping=true;
    for(std::size_t i=0;i<uv.size();++i){auto p=uv[i];auto mapped=ngRigPhysicsTransform(result.pixelToModel,{p[0]*width,p[1]*height});
        for(std::size_t axis=0;axis<2;++axis)result.affineMapping&=
            std::abs(mapped[axis]-arrayAt(vertices,i,441)[axis])<=
                1e-8*dmax(1.,std::abs(arrayAt(vertices,i,441)[axis]));}
    struct Triangle {std::array<std::size_t,3> ids{};Point2 a,b,c;double determinant=NAN;};Slice<Triangle> triangles;
    for(auto face:mapping.sourceIndexValue("triangles").sourceArrayValue()){auto ids=ngRigNumbers(face);enforce(ids.size()==3,"Invalid physics UV triangle",446);Triangle triangle;
        for(std::size_t i=0;i<ids.size();++i){auto id=ids[i];enforce(id>=0 && id<vertices.size() && id==static_cast<std::size_t>(id),"Physics UV index outside mesh",449);triangle.ids[i]=static_cast<std::size_t>(id);}
        triangle.a=arrayAt(uv,triangle.ids[0],452);
        triangle.b=sub(arrayAt(uv,triangle.ids[1],452),triangle.a);
        triangle.c=sub(arrayAt(uv,triangle.ids[2],453),triangle.a);
        triangle.determinant=triangle.b[0]*triangle.c[1]-triangle.b[1]*triangle.c[0];if(std::abs(triangle.determinant)>=1e-14)triangles.append(triangle);
    }
    enforce(!triangles.empty(),"Physics material has no valid UV triangles",457);
    auto divisions=std::min<std::size_t>(static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(triangles.size())))),128);
    auto columns=std::min(width,divisions),rows=std::min(height,divisions);
    auto cells=physics_detail::newArray<Slice<std::size_t>>(columns*rows);
    const auto cell=[](double coordinate,std::size_t count){
        return static_cast<std::size_t>(clamp(std::floor(coordinate*count),0.,static_cast<double>(count-1)));
    };
    for(std::size_t index=0;index<triangles.size();++index){auto triangle=triangles[index];
        Point2 b{triangle.a[0]+triangle.b[0],triangle.a[1]+triangle.b[1]},c{triangle.a[0]+triangle.c[0],triangle.a[1]+triangle.c[1]};
        double padding=4e-8*dmax(dmax(dmax(1.,std::abs(triangle.b[0])),std::abs(triangle.b[1])),
            dmax(std::abs(triangle.c[0]),std::abs(triangle.c[1])));
        auto x0=cell(dmin(dmin(triangle.a[0],b[0]),c[0])-padding,columns),x1=cell(dmax(dmax(triangle.a[0],b[0]),c[0])+padding,columns);
        auto y0=cell(dmin(dmin(triangle.a[1],b[1]),c[1])-padding,rows),y1=cell(dmax(dmax(triangle.a[1],b[1]),c[1])+padding,rows);
        for(auto y=y0;y<y1+1;++y)for(auto x=x0;x<x1+1;++x)arrayAt(cells,y*columns+x,474).append(index);
    }
    for(std::size_t y=0;y<height;++y){if(y%64==0)ngRigCheckpoint(task);
        for(std::size_t x=0;x<width;++x)if(result.mask[y*width+x]){Point2 pixel{(x+.5)/width,(y+.5)/height};bool covered=false;
            for(auto index:arrayAt(cells,cell(pixel[1],rows)*columns+cell(pixel[0],columns),480)){
                auto triangle=arrayAt(triangles,index,481);auto q=sub(pixel,triangle.a);
                auto v=(q[0]*triangle.c[1]-q[1]*triangle.c[0])/triangle.determinant,w=(triangle.b[0]*q[1]-triangle.b[1]*q[0])/triangle.determinant;
                if(v < -1e-8 || w < -1e-8 || v+w>1+1e-8)continue;Point2 point;
                for(std::size_t axis=0;axis<2;++axis)point[axis]=
                    (1-v-w)*arrayAt(vertices,triangle.ids[0],487)[axis]+
                    v*arrayAt(vertices,triangle.ids[1],488)[axis]+w*arrayAt(vertices,triangle.ids[2],488)[axis];
                result.points.append(result.affineMapping?ngRigPhysicsTransform(result.pixelToModel,{x+.5,y+.5}):point);covered=true;break;
            }result.mask[y*width+x]=covered;
        }
    }return result;
}
}
