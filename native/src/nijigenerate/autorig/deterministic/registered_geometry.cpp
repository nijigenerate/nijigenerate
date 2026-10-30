#include "detail/registered.hpp"
namespace nijigenerate::autorig::deterministic::registered::detail {
double quantile(compat::Slice<double> values,double t) {
    enforce(!values.empty(),"Missing semantic facial support",22);
    auto sorted=values.duplicate(); sort(sorted);
    auto a=sorted.duplicate();
    const double index=t*(a.size()-1); const auto i=static_cast<std::size_t>(index);
    const double lowerValue=indexed(a,i,24);
    const double lowerWeight=1-index+i;
    const double lower=lowerValue*lowerWeight;
    const double upperValue=indexed(a,std::min(i+1,a.size()-1),24);
    const double upperWeight=index-i;
    const double upper=upperValue*upperWeight;
    return lower+upper;
}
Point2 ChartFrame::affineTo(Point2 p) {
    p=sub(p,origin); const double d=matrix[0]*matrix[3]-matrix[1]*matrix[2];
    enforce(d>0,"Anatomical registration reverses orientation",33);
    return {(matrix[3]*p[0]-matrix[1]*p[1])/d,(-matrix[2]*p[0]+matrix[0]*p[1])/d};
}
Point2 ChartFrame::forward(Point2 p,std::array<double,4>& jac) {
    const auto n=centers.size(); Point2 result=p; jac={1.,0.,0.,1.};
    for(std::size_t i=0;i<n;++i) {
        const auto delta=sub(p,indexed(centers,i,38)); const double r2=dot(delta,delta), logarithm=std::log(dmax(r2,1e-30));
        for(std::size_t k=0;k<2;++k) {
            result[k]+=r2*logarithm*indexed(coefficients,i,41)[k];
            for(std::size_t j=0;j<2;++j) jac[k*2+j]+=2*delta[j]*(logarithm+1)*indexed(coefficients,i,42)[k];
        }
    }
    for(std::size_t k=0;k<2;++k) {
        const double base=indexed(coefficients,n,46)[k];
        const double x=p[0]*indexed(coefficients,n+1,46)[k];
        const double partial=base+x;
        const double y=p[1]*indexed(coefficients,n+2,46)[k];
        result[k]+=partial+y;
        jac[k*2]+=indexed(coefficients,n+1,47)[k]; jac[k*2+1]+=indexed(coefficients,n+2,47)[k];
    }
    return result;
}
Point2 ChartFrame::toFrame(Point2 root) {
    const auto target=affineTo(root); auto p=target;
    for(int iteration=0;iteration<80;++iteration) {
        std::array<double,4> jac; const auto error=sub(forward(p,jac),target);
        if(dmax(std::abs(error[0]),std::abs(error[1]))<1e-10) return p;
        const double det=jac[0]*jac[3]-jac[1]*jac[2];
        enforce(det>.01,"Semantic chart inverse leaves its invertible domain",57);
        Point2 step{(jac[3]*error[0]-jac[1]*error[1])/det,(-jac[2]*error[0]+jac[0]*error[1])/det};
        double scale=1; bool accepted=false;
        for(int backtrack=0;backtrack<30;++backtrack) {
            Point2 candidate{p[0]-step[0]*scale,p[1]-step[1]*scale};
            const auto next=sub(forward(candidate,jac),target);
            if(norm(next)<norm(error) && jac[0]*jac[3]-jac[1]*jac[2]>.01) { p=candidate;accepted=true;break; }
            scale*=.5;
        }
        enforce(accepted,"Semantic inverse line search did not reduce its residual",68);
    }
    throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(
        "Semantic correspondence inverse did not converge"),registeredSourceFile(),70);
}
Point2 ChartFrame::fromFrame(Point2 q) {
    std::array<double,4> jac; q=forward(q,jac);
    return {origin[0]+matrix[0]*q[0]+matrix[1]*q[1],origin[1]+matrix[2]*q[0]+matrix[3]*q[1]};
}
void ChartFrame::install(Json spec,const Landmarks& landmarks) {
    centers=ngRigPoints(at(spec,"canonical")); const auto n=centers.size(); auto observed=centers.duplicate();
    const auto labels=at(spec,"labels").sourceArrayValue();
    for(std::size_t i=0;i<labels.size();++i) {
        const auto label=indexed(labels,i,79).as_string();
        enforceLazy(landmarks.contains(label.view()),
            [&] { return joinEnforceMessage("Missing semantic landmark: ", label.view()); },80);
        indexed(observed,i,81)=affineTo(aa(landmarks,label.view(),81));
    }
    for(auto triangle:at(spec,"triangles").sourceArrayValue()) {
        auto ids=triangle.sourceArrayValue();
        const auto a=ngRigUnsigned(indexed(ids,0,84)),b=ngRigUnsigned(indexed(ids,1,85)),c=ngRigUnsigned(indexed(ids,2,85));
        const auto area=[&](compat::Slice<Point2> ps) {
            const auto pb=indexed(ps,b,87),pa=indexed(ps,a,87);
            const auto u=sub(pb,pa);
            const auto pc=indexed(ps,c,87),paAgain=indexed(ps,a,87);
            const auto v=sub(pc,paAgain);
            return u[0]*v[1]-u[1]*v[0];
        };
        const auto ratio=[&] {
            const auto observedArea=area(observed);
            const auto canonicalArea=area(centers);
            return observedArea/canonicalArea;
        };
        enforce(std::isfinite(ratio()) && ratio()>1e-4,
            "Folded semantic correspondence; no model-specific fallback",89);
    }
    auto system=ngRigZeroMatrix(n+3,n+3),right=ngRigZeroMatrix(n+3,2);
    for(std::size_t i=0;i<n;++i) {
        for(std::size_t j=0;j<n;++j) {
            const auto pi=indexed(centers,i,94),pj=indexed(centers,j,94);
            const auto d=sub(pi,pj); const double r2=dot(d,d);
            indexed(indexed(system,i,95),j,95)=r2*std::log(dmax(r2,1e-30));
        }
        const std::array values{1.,indexed(centers,i,96)[0],indexed(centers,i,96)[1]};
        for(std::size_t k=0;k<3;++k)
            indexed(indexed(system,i,96),n+k,96)=indexed(indexed(system,n+k,96),i,96)=values[k];
        const auto actual=indexed(observed,i,97),canonical=indexed(centers,i,97);
        const auto delta=sub(actual,canonical);
        indexed(right,i,97)=compat::Slice<double>{delta[0],delta[1]};
    }
    for(auto row:ngRigSolve(system,right)) coefficients.append(Point2{indexed(row,0,99),indexed(row,1,99)});
    double extent=0;for(auto p:centers)extent=dmax(extent,dmax(std::abs(p[0]),std::abs(p[1])));
    for(int y=0;y<41;++y)for(int x=0;x<41;++x) {
        std::array<double,4> jac;forward({-extent+2*extent*x/40,-extent+2*extent*y/40},jac);
        enforce(jac[0]*jac[3]-jac[1]*jac[2]>.05,"Noninvertible smooth semantic correspondence",103);
    }
}
double sample(compat::Slice<double> xs,compat::Slice<double> ys,compat::Slice<double> values,Point2 p) {
    enforce(xs.size()>=2 && ys.size()>=2 && values.size()==xs.size()*ys.size(),"Invalid registered depth field",109);
    p={clamp(p[0],indexed(xs,0,110),indexed(xs,xs.size()-1,110)),
        clamp(p[1],indexed(ys,0,110),indexed(ys,ys.size()-1,110))};
    std::size_t i=0,j=0;
    while(i+2<xs.size() && p[0]>=indexed(xs,i+1,111))++i;
    while(j+2<ys.size() && p[1]>=indexed(ys,j+1,112))++j;
    const double u=(p[0]-indexed(xs,i,113))/(indexed(xs,i+1,113)-indexed(xs,i,113));
    const double v=(p[1]-indexed(ys,j,113))/(indexed(ys,j+1,113)-indexed(ys,j,113));
    const double lowerWeight=1-v;
    const double lowerLeftWeight=1-u;
    const double lowerLeftValue=indexed(values,j*xs.size()+i,114);
    const double lowerLeft=lowerLeftWeight*lowerLeftValue;
    const double lowerRightValue=indexed(values,j*xs.size()+i+1,114);
    const double lowerRight=u*lowerRightValue;
    const double lowerSum=lowerLeft+lowerRight;
    const double lower=lowerWeight*lowerSum;
    const double upperLeftWeight=1-u;
    const double upperLeftValue=indexed(values,(j+1)*xs.size()+i,115);
    const double upperLeft=upperLeftWeight*upperLeftValue;
    const double upperRightValue=indexed(values,(j+1)*xs.size()+i+1,115);
    const double upperRight=u*upperRightValue;
    const double upperSum=upperLeft+upperRight;
    const double upper=v*upperSum;
    return lower+upper;
}
compat::Slice<double> anchoredAxis(double lo,double hi,std::size_t count,compat::Slice<double> anchors) {
    compat::Slice<double> axis{lo,hi};axis.append(anchors);for(auto& p:axis)p=std::rint(p*1e4)/1e4;
    sort(axis);compat::Slice<double> unique;
    for(auto p:axis)if(unique.empty() || p!=indexed(unique,unique.size()-1,126))unique.append(p);
    count=std::max(count,unique.size());
    while(unique.size()<count) {
        std::size_t k=0;
        for(std::size_t i=1;i<unique.size()-1;++i)
            if(indexed(unique,i+1,129)-indexed(unique,i,129)>indexed(unique,k+1,129)-indexed(unique,k,129))k=i;
        const std::array args{core::translation::ngFormatArgument((indexed(unique,k,132)+indexed(unique,k+1,132))*.5)};
        auto formatted=core::translation::ngFormatTyped("%.4f",args);
        const double middle=ngRigNumber(autorig::json::ngParseAutoRigJson(compat::RetainedString(std::move(formatted))));
        if(middle==indexed(unique,k,133) || middle==indexed(unique,k+1,133))break;
        unique.append(middle);sort(unique);
    }
    return unique;
}
core::settings::SettingsString componentRole(Json domain) {
    const auto owner=at(domain,"owner").as_string(),label=at(domain,"chart").as_string();
    const auto lower=[](compat::RetainedStringSlice value){return io::imageexport::detail::lowerExtension(value);};
    if(owner.view()=="head") {
        if(label.view()=="face")return literalKey("face");
        if(label.view()=="hair:front")return literalKey("hair_front");
        if(label.view()=="hair:back")return literalKey("hair_back");
        if(label.view().starts_with("hair:")) {
            const auto suffix=lower(label.slice(5,label.size()));
            auto role=joinEnforceMessage("hair_side/",suffix.view());
            return core::settings::SettingsString(std::move(role));
        }
        if(label.view().starts_with("headwear") || label.view().starts_with("ear:"))return literalKey("headwear");
    }
    if(owner.view()=="torso") {
        const RegisteredStringAA<core::settings::SettingsString> roles{
            {literalKey("body"),literalKey("torso")},{literalKey("neck"),literalKey("torso")},
            {literalKey("topwear_front"),literalKey("topwear/front")},
            {literalKey("topwear_waist"),literalKey("topwear/waist")},
            {literalKey("skirt"),literalKey("skirt/front")},{literalKey("apron"),literalKey("apron")},
            {literalKey("tail"),literalKey("tail")},{literalKey("attachment:chest"),literalKey("torso")},
            {literalKey("attachment:pelvis"),literalKey("skirt/front")},
            {literalKey("skirt_back"),literalKey("skirt/back")}};
        if(auto i=roles.find(label.view());i!=roles.end())return i->second;
        if(label.view().starts_with("skirt_back:")) {
            const auto suffix=lower(label.slice(11,label.size()));
            auto role=joinEnforceMessage("train/",suffix.view());
            return core::settings::SettingsString(std::move(role));
        }
    }
    if(owner.view().starts_with("arm:")) {
        const std::string_view prefix=label.view()=="sleeve" || label.view()=="shoulder"?"sleeve/":"arm/";
        auto role=joinEnforceMessage(prefix,owner.view().substr(4));
        return core::settings::SettingsString(std::move(role));
    }
    if(owner.view().starts_with("leg:")) {
        auto role=joinEnforceMessage("leg/",owner.view().substr(4));
        return core::settings::SettingsString(std::move(role));
    }
    const auto id=at(domain,"id").as_string();
    constexpr std::string_view prefix="No common template component for ";
    const core::settings::SettingsString message(joinEnforceMessage(prefix,id.view()));
    throw compat::ThrowableException(message.retainedString(),registeredSourceFile(),190);
}
}
namespace nijigenerate::autorig::deterministic::registered {
using namespace detail;
compat::Slice<double> ngRigSampleRegisteredDepth(compat::Slice<double> xs,compat::Slice<double> ys,
        compat::Slice<double> values,compat::Slice<Point2> points) {
    compat::Slice<double> result;for(auto p:points)result.append(sample(xs,ys,values,p));return result;
}
Landmarks ngRigFacialLandmarks(Json observation) {
    detail::RegisteredStringAA<compat::Slice<Point2>> features;compat::Slice<double> faceXs;
    for(auto m:at(observation,"materials").sourceArrayValue())if(!at(m,"static").as_bool()) {
        if(at(m,"role").as_string().view()=="face")for(auto p:ngRigPoints(at(m,"cloud")))faceXs.append(p[0]);
        const auto feature=ngRigString(m,text("feature"),text(""));
        if(!feature.empty())features[feature].append(ngRigPoints(ngRigGet(m,text("landmark_cloud"),at(m,"cloud"))));
    }
    const auto featurePoints = [&](const char* key) -> compat::Slice<Point2> {
        const auto found = features.find(key);
        return found == features.end() ? compat::Slice<Point2>{} : found->second;
    };
    const double center=quantile(faceXs,.5);Landmarks result;std::array<Point2,2> eyeCenters;
    for(std::size_t index=0;index<2;++index) {
        const auto side=index==0?literalKey("r"):literalKey("l");compat::Slice<Point2> eye;
        for(auto p:featurePoints("sclera"))if((p[0]<center)==(index==0))eye.append(p);
        compat::Slice<double> xs;for(auto p:eye)xs.append(p[0]);
        const double lo=quantile(xs,.01),hi=quantile(xs,.99);std::array<Point2,2> ends;
        for(std::size_t i=0;i<2;++i) {
            const double x=i==0?lo:hi;compat::Slice<double> ys;
            for(auto p:eye)if(std::abs(p[0]-x)<=dmax(2.,(hi-lo)*.015)+1e-9)ys.append(p[1]);
            ends[i]={x,quantile(ys,.5)};
        }
        result[joinEnforceMessage(joinEnforceMessage("eye_",side.view()),"_outer")]=ends[index==0?0:1];
        result[joinEnforceMessage(joinEnforceMessage("eye_",side.view()),"_inner")]=ends[index==0?1:0];
        eyeCenters[index]={(ends[0][0]+ends[1][0])/2,(ends[0][1]+ends[1][1])/2};
    }
    compat::Slice<double> xs,ys;for(auto p:featurePoints("nose")){xs.append(p[0]);ys.append(p[1]);}
    result[literalKey("nose")]={quantile(xs,.5),quantile(ys,.5)};
    const auto mouthEntry = features.find("mouth");
    auto mouth = mouthEntry == features.end() ? featurePoints("mouth_outline") : mouthEntry->second;
    auto u=sub(eyeCenters[1],eyeCenters[0]);const auto length=norm(u);for(auto& v:u)v/=length;Point2 v{-u[1],u[0]};
    compat::Slice<double> along,cross;for(auto p:mouth){along.append(dot(p,u));cross.append(dot(p,v));}
    const double lo=quantile(along,.01),hi=quantile(along,.99),mid=quantile(cross,.5);
    result[literalKey("mouth_r")]={lo*u[0]+mid*v[0],lo*u[1]+mid*v[1]};
    result[literalKey("mouth_l")]={hi*u[0]+mid*v[0],hi*u[1]+mid*v[1]};
    result[literalKey("mouth")]={(lo+hi)/2*u[0]+mid*v[0],(lo+hi)/2*u[1]+mid*v[1]};return result;
}
}
