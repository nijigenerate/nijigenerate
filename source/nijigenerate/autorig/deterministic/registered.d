module nijigenerate.autorig.deterministic.registered;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.linear;
import nijigenerate.autorig.deterministic.observation : ngRigAlphaCoverage;
import nijigenerate.autorig.deterministic.templates : ngRigHumanoidPrior;
import std.json : JSONValue, JSONType;
import nijigenerate.autorig.json : parseJSON = ngParseAutoRigJson;
import std.algorithm : min, max, sort, clamp;
import std.array : array;
import std.exception : enforce;
import std.math : sqrt, log, abs, rint, isFinite;
import std.string : startsWith, toLower;
import std.format : format;
import std.digest.sha : sha256Of;
import std.digest : toHexString;

private double norm(Point2 p) { return sqrt(p[0]*p[0]+p[1]*p[1]); }
private Point2 sub(Point2 a, Point2 b) { return [a[0]-b[0],a[1]-b[1]]; }
private double dot(Point2 a, Point2 b) { return a[0]*b[0]+a[1]*b[1]; }
private double quantile(double[] values, double t) {
    enforce(values.length>0,"Missing semantic facial support");
    auto a=values.dup.sort.array; double index=t*(a.length-1); auto i=cast(size_t)index;
    return a[i]*(1-index+i)+a[min(i+1,a.length-1)]*(index-i);
}

private struct ChartFrame {
    Point2 origin;
    double[4] matrix;
    Point2[] centers, coefficients;
    Point2 affineTo(Point2 p) {
        p=sub(p,origin); double d=matrix[0]*matrix[3]-matrix[1]*matrix[2];
        enforce(d>0,"Anatomical registration reverses orientation");
        return [(matrix[3]*p[0]-matrix[1]*p[1])/d,(-matrix[2]*p[0]+matrix[0]*p[1])/d];
    }
    Point2 forward(Point2 p, out double[4] jac) {
        auto n=centers.length; Point2 result=p; jac=[1.,0.,0.,1.];
        foreach (i,c; centers) {
            auto delta=sub(p,c); double r2=dot(delta,delta), logarithm=log(max(r2,1e-30));
            foreach (k; 0..2) {
                result[k]+=r2*logarithm*coefficients[i][k];
                foreach (j; 0..2) jac[k*2+j]+=2*delta[j]*(logarithm+1)*coefficients[i][k];
            }
        }
        foreach (k; 0..2) {
            result[k]+=coefficients[n][k]+p[0]*coefficients[n+1][k]+p[1]*coefficients[n+2][k];
            jac[k*2]+=coefficients[n+1][k]; jac[k*2+1]+=coefficients[n+2][k];
        }
        return result;
    }
    Point2 toFrame(Point2 root) {
        auto target=affineTo(root), p=target;
        foreach (iteration; 0..80) {
            double[4] jac; auto error=sub(forward(p,jac),target);
            if (max(abs(error[0]),abs(error[1]))<1e-10) return p;
            double det=jac[0]*jac[3]-jac[1]*jac[2];
            enforce(det>.01,"Semantic chart inverse leaves its invertible domain");
            Point2 step=[(jac[3]*error[0]-jac[1]*error[1])/det,(-jac[2]*error[0]+jac[0]*error[1])/det];
            double scale=1; bool accepted;
            foreach (backtrack; 0..30) {
                Point2 candidate=[p[0]-step[0]*scale,p[1]-step[1]*scale];
                auto next=sub(forward(candidate,jac),target);
                if (norm(next)<norm(error) && jac[0]*jac[3]-jac[1]*jac[2]>.01) {
                    p=candidate; accepted=true; break;
                }
                scale*=.5;
            }
            enforce(accepted,"Semantic inverse line search did not reduce its residual");
        }
        throw new Exception("Semantic correspondence inverse did not converge");
    }
    Point2 fromFrame(Point2 q) {
        double[4] jac; q=forward(q,jac);
        return [origin[0]+matrix[0]*q[0]+matrix[1]*q[1],origin[1]+matrix[2]*q[0]+matrix[3]*q[1]];
    }
    void install(JSONValue spec, Point2[string] landmarks) {
        centers=ngRigPoints(spec["canonical"]); auto n=centers.length;
        auto observed=centers.dup;
        foreach (i,label; spec["labels"].array) {
            enforce((label.str in landmarks)!is null,"Missing semantic landmark: "~label.str);
            observed[i]=affineTo(landmarks[label.str]);
        }
        foreach (triangle; spec["triangles"].array) {
            auto ids=triangle.array; auto a=cast(size_t)ngRigUnsigned(ids[0]);
            auto b=cast(size_t)ngRigUnsigned(ids[1]); auto c=cast(size_t)ngRigUnsigned(ids[2]);
            double area(Point2[] ps) {
                auto u=sub(ps[b],ps[a]),v=sub(ps[c],ps[a]); return u[0]*v[1]-u[1]*v[0];
            }
            enforce(isFinite(area(observed)/area(centers)) && area(observed)/area(centers)>1e-4,
                "Folded semantic correspondence; no model-specific fallback");
        }
        auto system=ngRigZeroMatrix(n+3,n+3), right=ngRigZeroMatrix(n+3,2);
        foreach (i; 0..n) {
            foreach (j; 0..n) { auto d=sub(centers[i],centers[j]); auto r2=dot(d,d);
                system[i][j]=r2*log(max(r2,1e-30)); }
            foreach (k,v; [1.,centers[i][0],centers[i][1]]) system[i][n+k]=system[n+k][i]=v;
            auto delta=sub(observed[i],centers[i]); right[i]=delta[].dup;
        }
        foreach (row; ngRigSolve(system,right)) coefficients~=[row[0],row[1]];
        double extent=0; foreach (p; centers) extent=max(extent,max(abs(p[0]),abs(p[1])));
        foreach (y; 0..41) foreach (x; 0..41) {
            double[4] jac; forward([-extent+2*extent*x/40,-extent+2*extent*y/40],jac);
            enforce(jac[0]*jac[3]-jac[1]*jac[2]>.05,"Noninvertible smooth semantic correspondence");
        }
    }
}

private double sample(double[] xs, double[] ys, double[] values, Point2 p) {
    enforce(xs.length>=2 && ys.length>=2 && values.length==xs.length*ys.length,"Invalid registered depth field");
    p=[clamp(p[0],xs[0],xs[$-1]),clamp(p[1],ys[0],ys[$-1])];
    size_t i=0,j=0; while (i+2<xs.length && p[0]>=xs[i+1]) ++i;
    while (j+2<ys.length && p[1]>=ys[j+1]) ++j;
    double u=(p[0]-xs[i])/(xs[i+1]-xs[i]),v=(p[1]-ys[j])/(ys[j+1]-ys[j]);
    return (1-v)*((1-u)*values[j*xs.length+i]+u*values[j*xs.length+i+1])+
        v*((1-u)*values[(j+1)*xs.length+i]+u*values[(j+1)*xs.length+i+1]);
}

double[] ngRigSampleRegisteredDepth(double[] xs, double[] ys, double[] values, Point2[] points) {
    double[] result;
    foreach (p; points) result~=sample(xs,ys,values,p);
    return result;
}

private double[] anchoredAxis(double lo, double hi, size_t count, double[] anchors) {
    double[] axis=[lo,hi]; axis~=anchors; foreach (ref p; axis) p=rint(p*1e4)/1e4;
    axis.sort; double[] unique; foreach (p; axis) if (!unique.length || p!=unique[$-1]) unique~=p;
    count=max(count,unique.length);
    while (unique.length<count) {
        size_t k=0; foreach (i; 1..unique.length-1) if (unique[i+1]-unique[i]>unique[k+1]-unique[k]) k=i;
        // Python's scalar round uses decimal rounding of the binary input;
        // multiplying by 10000 first changes half-way decisions.
        auto middle=ngRigNumber(parseJSON(format("%.4f",(unique[k]+unique[k+1])*.5)));
        if (middle==unique[k] || middle==unique[k+1]) break;
        unique~=middle; unique.sort;
    }
    return unique;
}

Point2[string] ngRigFacialLandmarks(JSONValue observation) {
    Point2[][string] features;
    double[] faceXs;
    foreach (m; observation["materials"].array) if (!m["static"].boolean) {
        if (m["role"].str=="face") foreach (p; ngRigPoints(m["cloud"])) faceXs~=p[0];
        auto feature=ngRigString(m,"feature","");
        if (feature.length) features[feature]~=ngRigPoints(ngRigGet(m,"landmark_cloud",m["cloud"]));
    }
    auto center=quantile(faceXs,.5); Point2[string] result; Point2[2] eyeCenters;
    foreach (index,side; ["r","l"]) {
        Point2[] eye; foreach (p; features.get("sclera",null)) if ((p[0]<center)==(index==0)) eye~=p;
        double[] xs; foreach (p; eye) xs~=p[0];
        auto lo=quantile(xs,.01),hi=quantile(xs,.99); Point2[2] ends;
        foreach (i,x; [lo,hi]) {
            double[] ys; foreach (p; eye) if (abs(p[0]-x)<=max(2.,(hi-lo)*.015)+1e-9) ys~=p[1];
            ends[i]=[x,quantile(ys,.5)];
        }
        result["eye_"~side~"_outer"]=ends[index==0 ? 0 : 1];
        result["eye_"~side~"_inner"]=ends[index==0 ? 1 : 0];
        eyeCenters[index]=[(ends[0][0]+ends[1][0])/2,(ends[0][1]+ends[1][1])/2];
    }
    double[] xs,ys; foreach (p; features.get("nose",null)) { xs~=p[0]; ys~=p[1]; }
    result["nose"]=[quantile(xs,.5),quantile(ys,.5)];
    auto mouth=features.get("mouth",features.get("mouth_outline",null));
    auto u=sub(eyeCenters[1],eyeCenters[0]); u[]/=norm(u); Point2 v=[-u[1],u[0]];
    double[] along,cross; foreach (p; mouth) { along~=dot(p,u); cross~=dot(p,v); }
    double lo=quantile(along,.01),hi=quantile(along,.99),mid=quantile(cross,.5);
    result["mouth_r"]=[lo*u[0]+mid*v[0],lo*u[1]+mid*v[1]];
    result["mouth_l"]=[hi*u[0]+mid*v[0],hi*u[1]+mid*v[1]];
    result["mouth"]=[(lo+hi)/2*u[0]+mid*v[0],(lo+hi)/2*u[1]+mid*v[1]];
    return result;
}

private string componentRole(JSONValue domain) {
    auto owner=domain["owner"].str,label=domain["chart"].str;
    if (owner=="head") {
        if (label=="face") return "face";
        if (label=="hair:front") return "hair_front";
        if (label=="hair:back") return "hair_back";
        if (label.startsWith("hair:")) return "hair_side/"~label[5..$].toLower;
        if (label.startsWith("headwear") || label.startsWith("ear:")) return "headwear";
    }
    if (owner=="torso") {
        string[string] roles=["body":"torso","neck":"torso","topwear_front":"topwear/front",
            "topwear_waist":"topwear/waist","skirt":"skirt/front","apron":"apron","tail":"tail",
            "attachment:chest":"torso","attachment:pelvis":"skirt/front","skirt_back":"skirt/back"];
        if (auto role=label in roles) return *role;
        if (label.startsWith("skirt_back:")) return "train/"~label[11..$].toLower;
    }
    if (owner.startsWith("arm:")) return (label=="sleeve" || label=="shoulder" ? "sleeve/" : "arm/")~owner[4..$];
    if (owner.startsWith("leg:")) return "leg/"~owner[4..$];
    throw new Exception("No common template component for "~domain["id"].str);
}

/** Apply the original single anatomical template, without runtime Python or new libraries. */
JSONValue ngRigRegisterProgram(JSONValue program, JSONValue observation) {
    enum source=import("autorig/reference-humanoid.registered.json");
    enforce(sha256Of(source).toHexString=="908B217553C870EA81B81902893AAB43B8D487871814127F048338989E865D71",
        "Embedded anatomical template differs from the original registered resource");
    auto common=parseJSON(source);
    enforce(common["template_count"].integer==1 && !common["character_selection"].boolean,
        "Expected one common anatomical template");
    JSONValue[string] bones; foreach (b; program["scaffold"]["bones"].array) bones[b["id"].str]=b;
    Point2 get(string name, string end="head") { auto p=ngRigNumbers(bones[name][end]); return [p[0],p[1]]; }
    auto pelvis=get("Pelvis"),neck=get("Neck"),vertical=sub(pelvis,neck);
    auto torso=norm(vertical); auto down=vertical; down[]/=torso; Point2 across=[down[1],-down[0]];
    auto width=abs(dot(sub(get("UpperArm.L"),get("UpperArm.R")),across));
    double[4] faceBounds=[double.infinity,double.infinity,-double.infinity,-double.infinity];
    JSONValue[ulong] materials;
    foreach (m; observation["materials"].array) {
        materials[ngRigUnsigned(m["uuid"])]=m;
        if (!m["static"].boolean && m["role"].str=="face") {
            auto b=ngRigNumbers(m["bounds"]);
            faceBounds[0]=min(faceBounds[0],b[0]); faceBounds[1]=min(faceBounds[1],b[1]);
            faceBounds[2]=max(faceBounds[2],b[2]); faceBounds[3]=max(faceBounds[3],b[3]);
        }
    }
    ChartFrame[string] frames;
    frames["body"]=ChartFrame(pelvis,[across[0]*width,vertical[0],across[1]*width,vertical[1]]);
    frames["head"]=ChartFrame([(faceBounds[0]+faceBounds[2])/2,(faceBounds[1]+faceBounds[3])/2],
        [faceBounds[2]-faceBounds[0],0.,0.,faceBounds[3]-faceBounds[1]]);
    Point2[string][string] landmarks;
    landmarks["head"]=ngRigFacialLandmarks(observation);
    foreach (name; ["Pelvis","Spine","Chest","Neck"]) landmarks["body"][name.toLower]=get(name);
    foreach (side; ["L","R"]) {
        auto suffix=side.toLower;
        landmarks["body"]["shoulder_"~suffix]=get("UpperArm."~side);
        landmarks["body"]["hip_"~suffix]=get("Thigh."~side);
        foreach (family; ["arm","leg"]) {
            auto proximal=family=="arm" ? "UpperArm" : "Thigh";
            auto joint=family=="arm" ? "Forearm" : "Shin";
            auto distal=family=="arm" ? "Hand" : "Foot";
            auto start=get(proximal~"."~side),axis=sub(get(distal~"."~side),start);
            auto direction=axis; direction[]/=norm(axis); Point2 right=[direction[1],-direction[0]];
            auto region=family~"/"~suffix;
            frames[region]=ChartFrame(start,[right[0]*torso,axis[0],right[1]*torso,axis[1]]);
            landmarks[region]=["root":start,"joint":get(joint~"."~side),"end":get(distal~"."~side),
                "tip":get(distal~"."~side,"tail")];
        }
    }
    auto hipL=get("Thigh.L"),hipR=get("Thigh.R");
    landmarks["body"]["pelvis"]=[(hipL[0]+hipR[0])/2,(hipL[1]+hipR[1])/2];
    foreach (region,spec; common["semantic_charts"].object) frames[region].install(spec,landmarks[region]);
    foreach (name,ref bone; bones) bone["pose_origin_z"]=JSONValue(ngRigNumber(common["bone_z"][name])*torso);
    bones["Chest"]["pose_origin_z"]=JSONValue(min(ngRigNumber(bones["Clavicle.L"]["pose_origin_z"]),
        ngRigNumber(bones["Clavicle.R"]["pose_origin_z"])));
    foreach (spec; common["support_bones"].array) {
        auto f=frames[spec["frame"].str]; auto head=f.fromFrame(ngRigPoint(spec["head"]));
        auto tail=f.fromFrame(ngRigPoint(spec["tail"])); auto z=ngRigNumbers(spec["rest_z"]);
        double pose=ngRigNumber(spec["pose_origin_z"])*torso;
        if (spec["head_anchor"].type!=JSONType.null_) {
            auto anchor=get(spec["head_anchor"].str),delta=sub(anchor,head); head=anchor;
            tail=[tail[0]+delta[0],tail[1]+delta[1]];
            pose=ngRigNumber(bones[spec["head_anchor"].str]["pose_origin_z"]);
        }
        bones[spec["id"].str]=JSONValue(["id":spec["id"],"head":JSONValue([head[0],head[1],z[0]*torso]),
            "tail":JSONValue([tail[0],tail[1],z[1]*torso]),"rest_roll":spec["rest_roll"],"pose_origin_z":JSONValue(pose)]);
    }
    JSONValue[] ordered;
    foreach (row; common["support_skeleton"].array) {
        auto bone=bones[row["bone"].str]; foreach (key; ["parent","lock_to_root","allow_parent_to_targets"]) bone[key]=row[key];
        ordered~=bone;
    }
    program["scaffold"]["bones"]=JSONValue(ordered);
    JSONValue[] drivers;
    foreach (curve; common["bone_curves"].array) {
        enforce(curve["interpolation"].str=="Linear","Unsupported common template interpolation");
        auto xs=ngRigNumbers(curve["axes"][0]),ys=ngRigNumbers(curve["axes"][1]); JSONValue[] values;
        auto unit=curve["units"].str;
        enforce(unit=="torso_length" || unit=="radians","Unsupported common bone curve unit");
        foreach (i,x; xs) foreach (j,y; ys) values~=JSONValue(["key":JSONValue([x,y]),
            "value":JSONValue(ngRigNumber(curve["values"][i][j])*(unit=="torso_length" ? torso : 1.))]);
        drivers~=JSONValue(["parameter":curve["parameter"],"bone":curve["bone"],"binding":curve["property"],"values":JSONValue(values)]);
    }
    program["native_drivers"]=JSONValue(drivers);
    double[4] extent=[double.infinity,double.infinity,-double.infinity,-double.infinity];
    JSONValue[string] registered;
    foreach (domain; program["domains"].array) {
        auto carrier=program["carriers"][0];
        foreach (c; program["carriers"].array) if (c["domain_id"].str==domain["id"].str) { carrier=c; break; }
        bool bilateral=domain["owner"].str=="arm:both" || domain["owner"].str=="leg:both";
        auto family=domain["owner"].str.startsWith("arm:") ? "arm" : "leg";
        auto role=bilateral ? family~"/r" : componentRole(domain);
        enforce((role in common["components"].object)!is null,"Missing common template surface: "~role);
        auto component=common["components"][role];
        enforce(component["orientation_accepted"].boolean && component["deformations"].object.length==0,
            "Invalid common template surface authority");
        auto f=frames[component["frame"].str]; Point2 origin=bilateral ? [0.,0.] : f.origin;
        double[4] bounds=[double.infinity,double.infinity,-double.infinity,-double.infinity];
        foreach (id; domain["parts"].array) {
            auto b=ngRigNumbers(materials[ngRigUnsigned(id)]["bounds"]);
            bounds[0]=min(bounds[0],b[0]-origin[0]); bounds[1]=min(bounds[1],b[1]-origin[1]);
            bounds[2]=max(bounds[2],b[2]-origin[0]); bounds[3]=max(bounds[3],b[3]-origin[1]);
        }
        auto padding=ngRigNumber(program["scaffold"]["body_height"])*
            ngRigNumber(ngRigHumanoidPrior()["bounds_padding_body_height"]);
        bounds[0]-=padding; bounds[1]-=padding; bounds[2]+=padding; bounds[3]+=padding;
        size_t nx=0,ny=0; foreach (resolution; component["source_resolution"].array) {
            nx=max(nx,cast(size_t)ngRigUnsigned(resolution[0])); ny=max(ny,cast(size_t)ngRigUnsigned(resolution[1]));
        }
        if (bilateral) {
            foreach (resolution; common["components"][family~"/l"]["source_resolution"].array) {
                nx=max(nx,cast(size_t)ngRigUnsigned(resolution[0])); ny=max(ny,cast(size_t)ngRigUnsigned(resolution[1]));
            }
            nx*=2;
        }
        double[] ax,ay;
        foreach (label; bilateral ? cast(JSONValue[])null : component["vertex_landmarks"].array) {
            auto p=sub(landmarks[component["frame"].str][label.str],origin);
            if (p[0]>=bounds[0] && p[0]<=bounds[2] && p[1]>=bounds[1] && p[1]<=bounds[3]) { ax~=p[0]; ay~=p[1]; }
        }
        auto xs=anchoredAxis(bounds[0],bounds[2],nx,ax),ys=anchoredAxis(bounds[1],bounds[3],ny,ay);
        auto cx=ngRigNumbers(component["axis_x"]),cy=ngRigNumbers(component["axis_y"]);
        auto relief=ngRigNumbers(component["depth"]["relief"]); double[] depths; Point2[] points;
        auto unit=component["depth"]["units"].str=="face width" ? faceBounds[2]-faceBounds[0] : torso;
        foreach (y; ys) foreach (x; xs) {
            Point2 p=[x+origin[0],y+origin[1]]; points~=p;
            double depth=0,total=0;
            foreach (side; bilateral ? ["r","l"] : [""]) {
                auto current=side.length ? common["components"][family~"/"~side] : component;
                auto field=frames[current["frame"].str]; double weight=1;
                if (bilateral) {
                    auto suffix=side=="r" ? "R" : "L";
                    auto names=family=="arm" ? ["UpperArm","Forearm","Hand"] : ["Thigh","Shin","Foot"];
                    Point2[] line=[get(names[0]~"."~suffix),get(names[1]~"."~suffix),
                        get(names[2]~"."~suffix),get(names[2]~"."~suffix,"tail")];
                    double distance=double.infinity;
                    foreach (i; 0..3) {
                        auto axis=sub(line[i+1],line[i]); auto t=clamp(dot(sub(p,line[i]),axis)/dot(axis,axis),0.,1.);
                        distance=min(distance,norm([p[0]-line[i][0]-t*axis[0],p[1]-line[i][1]-t*axis[1]]));
                    }
                    weight=1/(max(distance,torso*.002)^^4);
                }
                auto currentUnit=current["depth"]["units"].str=="face width" ? faceBounds[2]-faceBounds[0] : torso;
                depth+=weight*((ngRigNumber(current["depth"]["plane_offset"])+sample(
                    ngRigNumbers(current["axis_x"]),ngRigNumbers(current["axis_y"]),
                    ngRigNumbers(current["depth"]["relief"]),field.toFrame(p)))*currentUnit+
                    ngRigNumber(common["bone_z"][current["depth"]["origin"].str])*torso);
                total+=weight;
            }
            depths~=depth/total;
        }
        carrier["xs"]=JSONValue(xs); carrier["ys"]=JSONValue(ys); carrier["points"]=ngRigPointsJson(points);
        carrier["parent_to_root"]=JSONValue([1.,0.,origin[0],0.,1.,origin[1]]);
        carrier["bounds"]=JSONValue([bounds[0]+origin[0],bounds[1]+origin[1],bounds[2]+origin[0],bounds[3]+origin[1]]);
        carrier["depth_model_units"]=JSONValue(depths);
        carrier["reference_component"]=JSONValue(bilateral ? family~"/both" : role);
        if (auto binding="bone_binding" in component.object) if (!bilateral) {
            carrier["bones"]=(*binding)["bones"]; carrier["bone_influence_rule"]=(*binding)["influence_rule"];
        }
        auto b=ngRigNumbers(carrier["bounds"]); foreach (i; 0..2) { extent[i]=min(extent[i],b[i]); extent[i+2]=max(extent[i+2],b[i+2]); }
        registered[domain["id"].str]=carrier;
    }
    JSONValue*[string] byRole;
    foreach (domain; program["domains"].array) {
        auto carrier=&registered[domain["id"].str];
        byRole[(*carrier)["reference_component"].str]=carrier;
    }
    if (auto body="torso" in byRole) foreach (side; ["L","R"]) {
        auto arm="arm/"~side.toLower in byRole; if (arm is null) continue;
        auto shoulder=get("UpperArm."~side),axis=sub(get("UpperArm."~side,"tail"),shoulder);
        double fieldAt(JSONValue c, Point2 p) {
            auto m=ngRigNumbers(c["parent_to_root"]); p=sub(p,[m[2],m[5]]);
            return sample(ngRigNumbers(c["xs"]),ngRigNumbers(c["ys"]),ngRigNumbers(c["depth_model_units"]),p);
        }
        auto bodyZ=fieldAt(**body,shoulder),armZ=fieldAt(**arm,shoulder);
        double[] weights; foreach (p; ngRigPoints((**arm)["points"])) {
            auto t=clamp(dot(sub(p,shoulder),axis)/dot(axis,axis),0.,1.); weights~=(1-t)^^2*(1+2*t);
        }
        auto matrix=ngRigNumbers((**arm)["parent_to_root"]);
        auto xs=ngRigNumbers((**arm)["xs"]),ys=ngRigNumbers((**arm)["ys"]);
        auto anchor=sample(xs,ys,weights,sub(shoulder,[matrix[2],matrix[5]]));
        enforce(anchor>0,"Arm shoulder is outside its depth field");
        auto z=ngRigNumbers((**arm)["depth_model_units"]);
        foreach (i,ref value; z) value+=(bodyZ-armZ)*weights[i]/anchor;
        (**arm)["depth_model_units"]=JSONValue(z);
    }
    string[ulong] domainsByPart;
    foreach (c; program["carriers"].array) domainsByPart[ngRigUnsigned(c["part"])]=c["domain_id"].str;
    foreach (hair; observation["materials"].array) if (!hair["static"].boolean &&
        (hair["role"].str=="hair_front" || hair["role"].str=="hair_side")) {
        foreach (skin; observation["materials"].array) if (!skin["static"].boolean && skin["role"].str=="face") {
            Point2[] overlap; auto cloud=ngRigPoints(hair["cloud"]); bool[] occupied;
            ngRigAlphaCoverage(skin,cloud,"alpha_runs_32",&occupied);
            foreach (i,p; cloud) if (occupied[i]) overlap~=p;
            if (!overlap.length) continue;
            auto front=domainsByPart[ngRigUnsigned(hair["uuid"])],back=domainsByPart[ngRigUnsigned(skin["uuid"])];
            auto fc=registered[front],bc=registered[back]; auto fm=ngRigNumbers(fc["parent_to_root"]),bm=ngRigNumbers(bc["parent_to_root"]);
            auto fz=ngRigNumbers(fc["depth_model_units"]),bz=ngRigNumbers(bc["depth_model_units"]);
            auto fx=ngRigNumbers(fc["xs"]),fy=ngRigNumbers(fc["ys"]),bx=ngRigNumbers(bc["xs"]),by=ngRigNumbers(bc["ys"]);
            double gap=-double.infinity;
            auto stride=(overlap.length+255)/256;
            for (size_t i=0;i<overlap.length;i+=stride) {
                auto p=overlap[i]; gap=max(gap,sample(bx,by,bz,sub(p,[bm[2],bm[5]]))-sample(fx,fy,fz,sub(p,[fm[2],fm[5]])));
            }
            auto shift=max(0.,gap+(faceBounds[2]-faceBounds[0])*.001);
            foreach (ref z; fz) z+=shift;
            registered[front]["depth_model_units"]=JSONValue(fz);
        }
    }
    double scale=max(1.,max(extent[2]-extent[0],extent[3]-extent[1])*.42/2.9);
    foreach (ref carrier; program["carriers"].array) {
        auto old=carrier; carrier=JSONValue(registered[carrier["domain_id"].str].object.dup);
        foreach (key; ["part","path","role","unit","side"]) carrier[key]=old[key];
        auto depths=ngRigNumbers(carrier["depth_model_units"]); foreach (ref z; depths) z=rint(z/scale*1e6)/1e6;
        carrier["depth"]=JSONValue(depths);
    }
    program["native_depth_scale"]=JSONValue(scale);
    program["reference_template"]=JSONValue(["sha256":common["content_sha256"],"count":JSONValue(1),
        "reference_count":common["reference_count"],"torso_length":JSONValue(torso),"motion_policy":common["motion_policy"]]);
    return program;
}
