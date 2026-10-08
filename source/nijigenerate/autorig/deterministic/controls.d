module nijigenerate.autorig.deterministic.controls;

import nijigenerate.autorig.deterministic.contracts;
import std.json : JSONValue;
import std.algorithm : sort, min, max, clamp;
import std.array : array;
import std.exception : enforce;
import std.math : sqrt, abs, sin, cos, round, rint, isFinite;
import nijigenerate.autorig.solver.quadratic;
import nijigenerate.autorig.framework : AutoRigTaskContext;
import nijigenerate.autorig.deterministic.linear : ngRigLeastSquares;
import nijigenerate.autorig.deterministic.templates : ngRigHumanoidPrior;
import nijigenerate.autorig.deterministic.geometry : ngRigTriangleMinimumRatio;
import nijigenerate.autorig.deterministic.observation : ngRigAlphaCoverage, ngRigAlphaContour, ngRigEyelashTopology;
import nijigenerate.autorig.deterministic.registered : ngRigFacialLandmarks;
import std.string : toLower, replace, startsWith, endsWith;
import std.regex : regex, matchFirst;

private double percentile(double[] input, double fraction) {
    enforce(input.length>0, "Empty facial support"); auto values = input.dup.sort.array;
    double index = fraction*(values.length-1); size_t i = cast(size_t)index;
    return values[i]*(1-(index-i))+values[min(i+1,values.length-1)]*(index-i);
}

private struct Frame {
    Point2 origin = [0.,0.], tangent = [0.,0.], normal = [0.,0.];
    double width = 0;
    Point2 local(Point2 p) {
        auto dx = p[0]-origin[0], dy = p[1]-origin[1];
        return [dx*tangent[0]+dy*tangent[1],dx*normal[0]+dy*normal[1]];
    }
}

private Frame frame(Point2[] support) {
    double[] xs, ys; foreach (p; support) { xs ~= p[0]; ys ~= p[1]; }
    Point2 origin = [percentile(xs,.5),percentile(ys,.5)];
    double xx = 0, xy = 0, yy = 0;
    foreach (p; support) { double dx = p[0]-origin[0], dy = p[1]-origin[1]; xx += dx*dx; xy += dx*dy; yy += dy*dy; }
    double eigen = (xx+yy+sqrt((xx-yy)*(xx-yy)+4*xy*xy))/2;
    Point2 tangent = abs(xy)>1e-12 ? [eigen-yy,xy] : xx>=yy ? [1.,0.] : [0.,1.];
    double length = sqrt(tangent[0]*tangent[0]+tangent[1]*tangent[1]); enforce(length>0, "Degenerate facial frame");
    tangent[] /= length; if (tangent[0]<0) tangent[] *= -1;
    Frame result; result.origin = origin; result.tangent = tangent; result.normal = [-tangent[1],tangent[0]];
    double[] stations; foreach (p; support) stations ~= result.local(p)[0];
    double low = percentile(stations,.01), high = percentile(stations,.99);
    result.width = high-low; enforce(result.width>0, "Degenerate facial width");
    result.origin[0] += (low+high)/2*tangent[0]; result.origin[1] += (low+high)/2*tangent[1];
    return result;
}

private Frame eyeFrame(Point2[] support) {
    double[] xs; foreach (point; support) xs ~= point[0];
    double low = percentile(xs,.01), high = percentile(xs,.99);
    Point2[2] endpoints;
    foreach (i,x; [low,high]) {
        double[] ys;
        foreach (point; support) if (abs(point[0]-x)<=max(2.,(high-low)*.015)+1e-9) ys ~= point[1];
        endpoints[i] = [x,percentile(ys,.5)];
    }
    Frame result;
    result.origin = [(endpoints[0][0]+endpoints[1][0])/2,(endpoints[0][1]+endpoints[1][1])/2];
    result.tangent = [endpoints[1][0]-endpoints[0][0],endpoints[1][1]-endpoints[0][1]];
    result.width = sqrt(result.tangent[0]*result.tangent[0]+result.tangent[1]*result.tangent[1]);
    enforce(result.width>1e-6,"Degenerate observed eye axis");
    result.tangent[] /= result.width; result.normal = [-result.tangent[1],result.tangent[0]];
    return result;
}

private struct Profile {
    double[] axis, upper, lower;
    double sample(double[] values, double x) {
        if (x<=axis[0]) return values[0];
        foreach (i; 1 .. axis.length) if (x<=axis[i]) {
            double t = (x-axis[i-1])/(axis[i]-axis[i-1]); return values[i-1]*(1-t)+values[i]*t;
        }
        return values[$-1];
    }
}

private Profile profile(Point2[] support, Frame frame) {
    Point2[] local; foreach (p; support) local ~= frame.local(p);
    Profile result;
    foreach (i; 0 .. 65) {
        double station = -frame.width/2+frame.width*i/64;
        double[] heights;
        foreach (p; local) if (abs(p[0]-station)<=frame.width/32) heights ~= p[1];
        if (!heights.length) {
            auto sorted = local.dup.sort!((a,b)=>abs(a[0]-station)<abs(b[0]-station)).array;
            foreach (p; sorted[0 .. max(1,sorted.length/100)]) heights ~= p[1];
        }
        result.axis ~= station; result.upper ~= percentile(heights,.02); result.lower ~= percentile(heights,.98);
    }
    return result;
}

private double[] contactProfile(Point2[] support, Frame frame, bool lowerEdge) {
    double[] values = new double[65]; bool[] valid = new bool[65];
    foreach (i; 0 .. values.length) {
        double station = -frame.width/2+frame.width*i/64;
        values[i] = lowerEdge ? -double.infinity : double.infinity;
        foreach (point; support) {
            auto p = frame.local(point);
            if (abs(p[0]-station)>frame.width/128) continue;
            values[i] = lowerEdge ? max(values[i],p[1]) : min(values[i],p[1]); valid[i] = true;
        }
    }
    size_t[] known; foreach (i,exists; valid) if (exists) known ~= i;
    enforce(known.length>0,"No painted boundary intersects the eye span");
    foreach (i,exists; valid) if (!exists) {
        if (i<known[0]) values[i] = values[known[0]];
        else if (i>known[$-1]) values[i] = values[known[$-1]];
        else foreach (j; 1 .. known.length) if (i<known[j]) {
            double t = cast(double)(i-known[j-1])/(known[j]-known[j-1]);
            values[i] = values[known[j-1]]*(1-t)+values[known[j]]*t; break;
        }
    }
    return values;
}

private double[] endpointCubic(double[] values) {
    double[][] design, residual;
    foreach (i,value; values) {
        double t = cast(double)i/(values.length-1);
        design ~= [t*(1-t),t*(1-t)*(2*t-1)];
        residual ~= [value-values[0]*(1-t)-values[$-1]*t];
    }
    auto coefficients = ngRigLeastSquares(design,residual);
    double[] result;
    foreach (i,row; design) {
        double t = cast(double)i/(values.length-1);
        result ~= values[0]*(1-t)+values[$-1]*t+row[0]*coefficients[0][0]+row[1]*coefficients[1][0];
    }
    return result;
}

/** Preserve signed native triangle areas by projecting only posed Y through OSQP. */
double[] ngRigProjectControlOrientation(Point2[] rest, JSONValue triangles, double[] offsets,
    AutoRigTaskContext context = null) {
    enforce(offsets.length == rest.length*2,"Local control vertex count mismatch");
    struct Constraint { size_t[3] indices; double[3] coefficients; }
    Constraint[] constraints;
    double minimum = double.infinity;
    foreach (triangle; triangles.array) {
        auto values = ngRigNumbers(triangle);
        enforce(values.length == 3,"Invalid native control triangle");
        size_t[3] index;
        foreach (i,value; values) {
            enforce(value>=0 && value<rest.length && value==cast(size_t)value,"Control triangle index out of range");
            index[i] = cast(size_t)value;
        }
        auto a = rest[index[0]], b = rest[index[1]], c = rest[index[2]];
        double before = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        if (before == 0) continue;
        double xa = a[0]+offsets[index[0]*2], xb = b[0]+offsets[index[1]*2], xc = c[0]+offsets[index[2]*2];
        double[3] coefficients = [(xc-xb)/before,(xa-xc)/before,(xb-xa)/before];
        double ratio = 0;
        foreach (i; 0 .. 3) ratio += coefficients[i]*(rest[index[i]][1]+offsets[index[i]*2+1]);
        minimum = min(minimum,ratio);
        constraints ~= Constraint(index,coefficients);
    }
    if (minimum>=.002) return offsets;
    QuadraticProblem problem;
    problem.variables = cast(int)rest.length; problem.constraints = cast(int)constraints.length;
    foreach (i,p; rest) {
        problem.objective ~= SparseEntry(cast(int)i,cast(int)i,1);
        problem.linear ~= -(p[1]+offsets[i*2+1]);
    }
    foreach (row,constraint; constraints) {
        foreach (i; 0 .. 3) problem.matrix ~= SparseEntry(cast(int)row,
            cast(int)constraint.indices[i],constraint.coefficients[i]);
        problem.lower ~= .004; problem.upper ~= double.infinity;
    }
    ngRigCheckpoint(context);
    auto result = ngSolveQuadratic(problem,SolverOptions(30000,1e-8,30,true),
        { return context !is null && context.isCanceled(); });
    ngRigCheckpoint(context);
    enforce(result.solved() && result.constraintViolation<1e-6,"Native control orientation projection failed");
    auto projected = offsets.dup;
    foreach (i,p; rest) projected[i*2+1] = result.solution[i]-p[1];
    return projected;
}

/** Group facial parts in measured feature-local frames, independent of suffix convention. */
JSONValue ngRigCompileControls(JSONValue state, AutoRigTaskContext context = null) {
    auto materials = state["materials"].array, targets = state["targets"].array;
    JSONValue[ulong] materialById;
    foreach (material; materials) materialById[ngRigUnsigned(material["uuid"])] = material;
    string feature(JSONValue material) {
        if (auto classified = "feature" in material.object) {
            auto kind = classified.str;
            return kind.startsWith("mouth") ? "mouth" : kind;
        }
        auto name = material["name"].str.toLower.replace("-","_").replace(":","_");
        string[string] patterns = ["sclera":"(sclera|eye_white)","iris":"(iris|pupil)",
            "upper":"(lash_upper|upper_lash|upper_lid)","lower":"(lash_lower|lower_lash|lower_lid)",
            "corner":"(eye_corner|canthus)","fold":"(lid_fold|eyelid_fold)","brow":"(brow|eyebrow)",
            "mouth":"(mouth|lip|tongue|teeth)"];
        string[] order = ["sclera","iris","upper","lower","corner","fold","brow","mouth"];
        foreach (kind; order) if (!matchFirst(name,regex(patterns[kind])).empty) return kind;
        return "";
    }
    Point2[] supports(string kind, string side, bool landmarks = false) {
        Point2[] points;
        foreach (target; targets) {
            auto material = materialById[ngRigUnsigned(target["part"])];
            if (feature(material) == kind && (side == "" || target["side"].str == side)) {
                auto landmark = "landmark_cloud" in material.object;
                points ~= ngRigPoints(!landmarks || landmark is null ? material["cloud"] : *landmark);
            }
        }
        return points;
    }
    Frame[string] eyes;
    foreach (side; ["R","L"]) {
        auto white = supports("sclera",side,true);
        if (white.length) eyes[side] = eyeFrame(white);
    }
    bool[ulong] sharedBrows;
    if (eyes.length == 2) foreach (target; targets) {
        auto material = materialById[ngRigUnsigned(target["part"])];
        if (feature(material) != "brow") continue;
        size_t right, left; auto cloud = ngRigPoints(material["cloud"]);
        foreach (p; cloud) {
            double distance(Frame e) { return (p[0]-e.origin[0])^^2+(p[1]-e.origin[1])^^2; }
            if (distance(eyes["R"])<=distance(eyes["L"])) ++right; else ++left;
        }
        if (right>=cloud.length*.05 && left>=cloud.length*.05) sharedBrows[ngRigUnsigned(target["part"])] = true;
    }
    JSONValue[] mechanisms, masks;
    void mechanism(string name, double[] xs, double[] ys, string[] features, string side,
        Frame frame, Point2 delegate(string,Point2,double,double,JSONValue) displacement) {
        JSONValue[] operations;
        foreach (target; targets) {
            auto material = materialById[ngRigUnsigned(target["part"])]; auto kind = feature(material);
            bool selected; foreach (candidate; features) if (candidate == kind) selected = true;
            bool sharedBrow = kind == "brow" && (ngRigUnsigned(target["part"]) in sharedBrows) !is null;
            if (!selected || side.length && target["side"].str != side && !sharedBrow) continue;
            auto positions = ngRigPoints(target["world"]);
            auto matrix = ngRigNumbers(target["root_to_local_direction"]);
            enforce(matrix.length == 4, "Missing native facial frame transform");
            JSONValue[] keys;
            foreach (y, vy; ys) foreach (x, vx; xs) {
                ngRigCheckpoint(context);
                double[] offsets;
                foreach (p; positions) {
                    auto d = displacement(kind,frame.local(p),vx,vy,material);
                    if (sharedBrow) {
                        auto other = eyes[side == "R" ? "L" : "R"].origin;
                        Point2 axis = [frame.origin[0]-other[0],frame.origin[1]-other[1]];
                        double distance = sqrt(axis[0]*axis[0]+axis[1]*axis[1]); axis[] /= distance;
                        Point2 midpoint = [(frame.origin[0]+other[0])/2,(frame.origin[1]+other[1])/2];
                        double weight = clamp(.5+2*((p[0]-midpoint[0])*axis[0]+(p[1]-midpoint[1])*axis[1])/distance,0.,1.);
                        d[] *= weight;
                    }
                    Point2 world = [d[0]*frame.tangent[0]+d[1]*frame.normal[0],
                        d[0]*frame.tangent[1]+d[1]*frame.normal[1]];
                    offsets ~= matrix[0]*world[0]+matrix[1]*world[1];
                    offsets ~= matrix[2]*world[0]+matrix[3]*world[1];
                }
                if (!name.endsWith("::Blink")) offsets = ngRigProjectControlOrientation(
                    ngRigPoints(target["mapping"]["vertices"]),target["mapping"]["triangles"],offsets,context);
                foreach (ref offset; offsets) offset = round(offset*100000)/100000;
                if (!name.endsWith("::Blink")) enforce(ngRigTriangleMinimumRatio(
                    ngRigPoints(target["mapping"]["vertices"]),target["mapping"]["triangles"],offsets)>=.002,
                    "Rounded local control has a folded triangle");
                if (vx == 0 && vy == 0) foreach (offset; offsets)
                    enforce(abs(offset)<=1e-8,"Local control changed the neutral pose");
                keys ~= JSONValue(["key":JSONValue([x,y]),"offsets":JSONValue(offsets)]);
            }
            operations ~= JSONValue(["part":target["part"],"keys":JSONValue(keys)]);
        }
        if (operations.length) mechanisms ~= JSONValue(["name":JSONValue(name),"axisX":JSONValue(xs),
            "axisY":JSONValue(ys),"operations":JSONValue(operations)]);
    }
    if (ngRigString(state,"kind","humanoid") == "local") {
        Point2[] support;
        foreach (target; targets) support ~= ngRigPoints(materialById[ngRigUnsigned(target["part"])]["cloud"]);
        auto localFrame = frame(support);
        Point2 mean = [0.,0.];
        foreach (p; support) { mean[0] += p[0]/support.length; mean[1] += p[1]/support.length; }
        localFrame.origin = mean;
        double minimum = double.infinity, maximum = -double.infinity;
        foreach (p; support) { auto x = localFrame.local(p)[0]; minimum = min(minimum,x); maximum = max(maximum,x); }
        localFrame.width = max(1e-8,maximum-minimum);
        string[] features;
        foreach (target; targets) features ~= feature(materialById[ngRigUnsigned(target["part"])]);
        mechanism("Local::Bend",[-1.,-.5,0.,.5,1.],[-1.,0.,1.],features,"",localFrame,
            (kind,p,x,y,material) {
                double curvature = x*.55/localFrame.width;
                Point2 q = p;
                if (abs(curvature)>1e-12) {
                    q[0] = (1/curvature-p[1])*sin(p[0]*curvature);
                    q[1] = 1/curvature-(1/curvature-p[1])*cos(p[0]*curvature);
                }
                Point2 delta = [q[0]-p[0],q[1]-p[1]+y*.1*p[0]]; return delta;
            });
    }
    foreach (side; ["R","L"]) {
        auto white = supports("sclera",side); if (!white.length) continue;
        auto eye = eyes[side]; auto aperture = profile(white,eye);
        auto upper = supports("upper",side), lower = supports("lower",side);
        auto upperContact = aperture.upper.dup, lowerContact = aperture.lower.dup;
        ulong whiteOwner;
        foreach (target; targets) if (target["side"].str == side &&
            feature(materialById[ngRigUnsigned(target["part"])]) == "sclera") { whiteOwner = ngRigUnsigned(target["part"]); break; }
        auto downClose = contactProfile(ngRigPoints(materialById[whiteOwner]["cloud"]),eye,true);
        double[] flatClose, expressionOffset, smileClose;
        foreach (i,down; downClose) {
            double t = cast(double)i/(downClose.length-1);
            double flat = downClose[0]*(1-t)+downClose[$-1]*t;
            flatClose ~= flat; expressionOffset ~= .3*(down-flat); smileClose ~= flat-.3*(down-flat);
        }
        smileClose = endpointCubic(smileClose);
        bool hasReference;
        Point2 reference;
        if (eyes.length == 2) { reference = eyes[side == "R" ? "L" : "R"].origin; hasReference = true; }
        else {
            auto mouthSupport = supports("mouth","");
            if (mouthSupport.length) { reference = frame(mouthSupport).origin; hasReference = true; }
        }
        if (hasReference) {
            bool innerLeft = eye.local(reference)[0]<0;
            foreach (i,ref value; smileClose) {
                double t = cast(double)i/(smileClose.length-1);
                value += eye.width*.08*(innerLeft ? 1-t : t)^^3;
            }
        }
        bool upperContainsLower;
        ulong upperOwner; size_t greatest;
        foreach (target; targets) if (target["side"].str == side &&
            feature(materialById[ngRigUnsigned(target["part"])]) == "upper") {
            auto cloud = ngRigPoints(materialById[ngRigUnsigned(target["part"])]["cloud"]);
            size_t score;
            foreach (p; cloud) if (abs(eye.local(p)[0])<=eye.width/2) ++score;
            if (score>greatest) { greatest = score; upperOwner = ngRigUnsigned(target["part"]); }
        }
        if (upperOwner) {
            Point2[] above, below; double low = double.infinity, high = -double.infinity;
            foreach (point; ngRigPoints(materialById[upperOwner]["cloud"])) {
                auto p = eye.local(point);
                double middle = (aperture.sample(aperture.upper,p[0])+aperture.sample(aperture.lower,p[0]))/2;
                if (p[1]<=middle) above ~= point;
                else if (abs(p[0])<=eye.width/2) { below ~= point; low = min(low,p[0]); high = max(high,p[0]); }
            }
            upperContact = contactProfile(above,eye,true);
            upperContainsLower = below.length>1 && high-low>eye.width*.25;
            if (upperContainsLower) lowerContact = contactProfile(below,eye,false);
        }
        JSONValue topology = JSONValue(cast(JSONValue[string])null);
        if (upperOwner && ("alpha_runs_32" in materialById[upperOwner].object) !is null) {
            topology = ngRigEyelashTopology(materialById[upperOwner],eye.origin,eye.tangent,context);
            if (!hasReference && topology["medial_resolved"].boolean) {
                bool innerLeft = topology["inner_left"].boolean;
                foreach (i,ref value; smileClose) {
                    double t = cast(double)i/(smileClose.length-1);
                    value += eye.width*.08*(innerLeft ? 1-t : t)^^3;
                }
            }
        }
        if (lower.length) lowerContact = contactProfile(lower,eye,false);
        double[] heights; foreach (i; 0 .. aperture.axis.length) heights ~= aperture.lower[i]-aperture.upper[i];
        double height = percentile(heights,.5);
        mechanism("Eye::" ~ side ~ "::Blink",[0.,.25,.5,.75,1.],[-1.,0.,1.],
            ["sclera","upper","lower","corner","fold"],side,eye,
            (kind,p,blink,expression,material) {
                double t = (p[0]+eye.width/2)/eye.width;
                double chord = flatClose[0]*(1-t)+flatClose[$-1]*t;
                double seam = expression>0 ? aperture.sample(flatClose,p[0])+
                    expression*(aperture.sample(smileClose,p[0])-aperture.sample(flatClose,p[0])) :
                    aperture.sample(flatClose,p[0])-expression*aperture.sample(expressionOffset,p[0]);
                double top = aperture.sample(upperContact,p[0]);
                double bottom = aperture.sample(lowerContact,p[0]);
                double delta = kind == "sclera" ? blink*(chord+(1-blink)*(seam-chord)-p[1]) :
                    kind == "lower" ? blink*(seam-bottom) :
                    kind == "fold" ? blink*(seam-top) : blink*(seam-top-clamp(p[1]-top,0.,max(0.,bottom-top)));
                Point2 d = [0.,delta]; return d;
            });
        if (mechanisms.length && mechanisms[$-1]["name"].str == "Eye::" ~ side ~ "::Blink")
            mechanisms[$-1]["contact_curves"] = JSONValue(["tangent":JSONValue(aperture.axis),
                "frame_origin":JSONValue(eye.origin[]),"frame_tangent":JSONValue(eye.tangent[]),
                "upper_lower_edge":JSONValue(upperContact),"lower_upper_edge":JSONValue(lowerContact),
                "white_boundary_owner":JSONValue(whiteOwner),"upper_owner":JSONValue(upperOwner),
                "upper_part_contains_lower_band":JSONValue(upperContainsLower),"neutral_target":JSONValue(flatClose),
                "open_sclera_lower_reference":JSONValue(downClose),"smile_target":JSONValue(smileClose),
                "expression_range":JSONValue(.3),"eyelash_topology":topology]);
        mechanism("Eye::" ~ side ~ "::X-Y",[-1.,-.5,0.,.5,1.],[-1.,0.,1.],["iris"],side,eye,
            (kind,p,x,y,material) { Point2 d = [x*eye.width*.18,y*height*.18]; return d; });
        mechanism("Eyebrow::" ~ side,[-1.,-.5,0.,.5,1.],[-1.,0.,1.],["brow"],side,eye,
            (kind,p,x,y,material) { Point2 d = [0.,x*eye.width*.08+y*p[0]*.15]; return d; });
        ulong owner;
        foreach (target; targets) if (target["side"].str == side && feature(materialById[ngRigUnsigned(target["part"])]) == "sclera") {
            owner = ngRigUnsigned(target["part"]); break;
        }
        foreach (target; targets) if (target["side"].str == side && feature(materialById[ngRigUnsigned(target["part"])]) == "iris")
            masks ~= JSONValue(["part":target["part"],"source":JSONValue(owner)]);
    }
    auto mouth = supports("mouth","");
    if (mouth.length) {
        Point2[] base;
        foreach (target; targets) {
            auto material = materialById[ngRigUnsigned(target["part"])];
            if (ngRigString(material,"feature","") == "mouth") base ~= ngRigPoints(material["cloud"]);
        }
        if (!base.length) foreach (target; targets) {
            auto material = materialById[ngRigUnsigned(target["part"])];
            if (ngRigString(material,"feature","") == "mouth_outline") base ~= ngRigPoints(material["cloud"]);
        }
        if (!base.length) base = mouth;
        Frame mouthFrame; mouthFrame.tangent = [1.,0.];
        if (eyes.length == 2) {
            auto r = eyes["R"].origin, l = eyes["L"].origin;
            mouthFrame.tangent = [l[0]-r[0],l[1]-r[1]];
            double length = sqrt(mouthFrame.tangent[0]^^2+mouthFrame.tangent[1]^^2);
            mouthFrame.tangent[] /= length;
        }
        mouthFrame.normal = [-mouthFrame.tangent[1],mouthFrame.tangent[0]];
        double[] stations, y;
        foreach (p; base) { auto q = mouthFrame.local(p); stations ~= q[0]; y ~= q[1]; }
        double low = percentile(stations,.01), high = percentile(stations,.99), middle = percentile(y,.5);
        mouthFrame.width = high-low;
        mouthFrame.origin = [(low+high)/2*mouthFrame.tangent[0]+middle*mouthFrame.normal[0],
            (low+high)/2*mouthFrame.tangent[1]+middle*mouthFrame.normal[1]];
        double height = max(percentile(y,1)-percentile(y,0),mouthFrame.width*.01);
        mechanism("Mouth::Open",[-1.,-.5,0.,.5,1.],[-1.,0.,1.],["mouth"],"",mouthFrame,
            (kind,p,x,expression,material) {
                auto name = material["name"].str.toLower;
                bool rigid = !matchFirst(name,regex("(tongue|teeth)")).empty;
                double basis = p[1];
                if (rigid && x>=0) {
                    double[] heights; foreach (point; ngRigPoints(material["cloud"])) heights ~= mouthFrame.local(point)[1];
                    basis = percentile(heights,.5);
                }
                double factor = x<0 ? 1+x*.985 : 1+x*mouthFrame.width*.20/height;
                double t = clamp(p[0]/(mouthFrame.width/2),-1.,1.);
                Point2 d = [0.,(factor-1)*basis+expression*mouthFrame.width*.035*(1-t*t)]; return d;
            });
        if (mechanisms.length && mechanisms[$-1]["name"].str == "Mouth::Open")
            mechanisms[$-1]["source_frame"] = JSONValue(["origin":JSONValue(mouthFrame.origin[]),
                "tangent":JSONValue(mouthFrame.tangent[]),"width":JSONValue(mouthFrame.width)]);
    }
    JSONValue[] drawOrder, overlappingBackings;
    double[ulong] desiredZ;
    foreach (target; targets) desiredZ[ngRigUnsigned(target["part"])] =
        ngRigNumber(ngRigGet(target,"absolute_zsort",JSONValue(0.)));
    foreach (target; targets) {
        ulong faceId = ngRigUnsigned(target["part"]);
        auto face = materialById[faceId];
        if (ngRigString(face,"role","") != "face") continue;
        auto compositeScope = ngRigGet(target,"composite_scope",JSONValue(cast(JSONValue[])null));
        ulong[] supported;
        double highest = -double.infinity;
        foreach (candidate; targets) {
            auto material = materialById[ngRigUnsigned(candidate["part"])];
            if (!feature(material).length || ngRigGet(candidate,"composite_scope",JSONValue(cast(JSONValue[])null)) != compositeScope) continue;
            auto strong = "draw_order_cloud" in material.object;
            if (strong is null) continue;
            if (ngRigAlphaCoverage(face,ngRigPoints(*strong))>.5) {
                supported ~= ngRigUnsigned(candidate["part"]); highest = max(highest,desiredZ[ngRigUnsigned(candidate["part"])]);
            }
        }
        if (!supported.length) continue;
        double next = max(desiredZ[faceId],highest+.25);
        void change(JSONValue node, double value, string reason) {
            ulong id = ngRigUnsigned(node["part"]);
            double original = ngRigNumber(ngRigGet(node,"absolute_zsort",JSONValue(0.)));
            double relative = ngRigNumber(ngRigGet(node,"relative_zsort",JSONValue(0.)));
            drawOrder ~= JSONValue(["part":node["part"],"absolute_zsort":JSONValue(value),
                "relative_zsort":JSONValue(value-original+relative),"reason":JSONValue(reason)]);
            desiredZ[id] = value;
        }
        if (next != desiredZ[faceId]) change(target,next,"skin behind independent facial mechanisms");
        auto faceCloud = "draw_order_cloud" in face.object;
        if (faceCloud is null) continue;
        foreach (candidate; targets) {
            auto ear = materialById[ngRigUnsigned(candidate["part"])];
            if (ngRigString(ear,"role","") != "ear" ||
                ngRigGet(candidate,"composite_scope",JSONValue(cast(JSONValue[])null)) != compositeScope) continue;
            double coverage = ngRigAlphaCoverage(ear,ngRigPoints(*faceCloud));
            if (coverage<=.5) continue;
            overlappingBackings ~= JSONValue(["ear":candidate["part"],"face":target["part"],
                "face_alpha_coverage":JSONValue(coverage)]);
            double value = max(desiredZ[ngRigUnsigned(candidate["part"])],desiredZ[faceId]+.25);
            if (value != desiredZ[ngRigUnsigned(candidate["part"])]) change(candidate,value,
                "ear artwork covering skin stays behind the separate skin");
        }
    }
    return JSONValue(["schema_version":JSONValue("rig-controls-d/1"),"mechanisms":JSONValue(mechanisms),
        "draw_order":JSONValue(drawOrder),"overlapping_ear_backings":JSONValue(overlappingBackings),
        "masks":JSONValue(masks),"source_sha256":state["source_sha256"]]);
}

private double curveSample(double[] axis, double[] values, double x) {
    enforce(axis.length == values.length && axis.length>0,"Invalid measured section curve");
    if (x<=axis[0]) return values[0];
    foreach (i; 1 .. axis.length) if (x<=axis[i]) {
        double t = (x-axis[i-1])/(axis[i]-axis[i-1]); return values[i-1]*(1-t)+values[i]*t;
    }
    return values[$-1];
}

private double smoothStep(double x) { x = clamp(x,0.,1.); return x*x*(3-2*x); }

/** Shared longitudinal Hermite field keeps the ankle fixed under body roll. */
JSONValue ngRigCompileFixedFootCorrections(JSONValue state, JSONValue program,
    AutoRigTaskContext context = null) {
    JSONValue[] operations;
    if (ngRigString(state,"kind","humanoid") != "humanoid")
        return JSONValue(["operations":JSONValue(operations)]);
    auto landmarks = program["scaffold"]["landmarks"];
    auto pelvis = ngRigPoint(landmarks["pelvis"]);
    double magnitude = ngRigNumber(ngRigHumanoidPrior()["drivers"]["body_roll_degrees"]);
    bool[ulong] seen;
    foreach (target; state["targets"].array) {
        if (!target["owner"].str.startsWith("leg:") || (ngRigUnsigned(target["grid"]) in seen)) continue;
        seen[ngRigUnsigned(target["grid"])] = true;
        auto side = target["side"].str;
        Point2[3] joints = [ngRigPoint(landmarks["hip." ~ side]),
            ngRigPoint(landmarks["knee." ~ side]),ngRigPoint(landmarks["ankle." ~ side])];
        Point2 axis = [joints[2][0]-joints[0][0],joints[2][1]-joints[0][1]];
        double length = sqrt(axis[0]^^2+axis[1]^^2); enforce(length>0,"Degenerate fixed-foot axis"); axis[] /= length;
        double[3] stations;
        foreach (i,p; joints) stations[i] = (p[0]-joints[0][0])*axis[0]+(p[1]-joints[0][1])*axis[1];
        enforce(stations[0]<stations[1] && stations[1]<stations[2],"Nonmonotone fixed-foot stations");
        auto xs = ngRigNumbers(target["xs"]), ys = ngRigNumbers(target["ys"]);
        auto matrix = ngRigNumbers(target["parent_to_root"]);
        double determinant = matrix[0]*matrix[4]-matrix[1]*matrix[3];
        enforce(abs(determinant)>1e-12,"Singular fixed-foot carrier frame");
        foreach (index,value; [-1.,-.5,0.,.5,1.]) {
            ngRigCheckpoint(context);
            import std.math : PI;
            double angle = magnitude*value*PI/180;
            Point2[3] delta;
            foreach (i,p; joints) {
                double x = p[0]-pelvis[0], y = p[1]-pelvis[1];
                delta[i] = [cos(angle)*x-sin(angle)*y+pelvis[0]-p[0],
                    sin(angle)*x+cos(angle)*y+pelvis[1]-p[1]];
            }
            delta[2] = [0.,0.];
            Point2[] posed, rest; double[] offsets;
            foreach (y; ys) foreach (x; xs) {
                rest ~= [x,y];
                Point2 world = [matrix[0]*x+matrix[1]*y+matrix[2],matrix[3]*x+matrix[4]*y+matrix[5]];
                double station = (world[0]-joints[0][0])*axis[0]+(world[1]-joints[0][1])*axis[1];
                size_t segment = station<=stations[1] ? 0 : 1;
                double weight = smoothStep((station-stations[segment])/(stations[segment+1]-stations[segment]));
                Point2 d = [delta[segment][0]*(1-weight)+delta[segment+1][0]*weight,
                    delta[segment][1]*(1-weight)+delta[segment+1][1]*weight];
                if (target["bones"].array.length == 1 && target["bones"][0].str == "Foot." ~ side) d = [0.,0.];
                Point2 local = [(matrix[4]*d[0]-matrix[1]*d[1])/determinant,
                    (-matrix[3]*d[0]+matrix[0]*d[1])/determinant];
                foreach (ref v; local) v = round(v*1e4)/1e4;
                offsets ~= local[0]; offsets ~= local[1];
                Point2 q = [x+local[0],y+local[1]]; posed ~= q;
            }
            import nijigenerate.autorig.deterministic.geometry : ngRigValidateGrid;
            auto validation = ngRigValidateGrid(posed,xs.length,.05,rest);
            operations ~= JSONValue(["grid":target["grid"],"parameter":JSONValue("Body::Roll"),
                "key":JSONValue([index,0]),"values":JSONValue(offsets),"validation":validation]);
        }
    }
    auto report = JSONValue(["operations":JSONValue(operations),
        "method":JSONValue("shared longitudinal cubic Hermite with fixed ankle"),
        "program_sha256":program["content_sha256"]]);
    report["content_sha256"] = JSONValue(ngRigDigest(report)); return report;
}

private struct SkinProfile {
    Frame frame;
    double[] y, left, right;
    double temple = 0, eye = 0, mouth = 0;
    double[2] eyeSpan, mouthSpan;
    double midline(double level) {
        return curveSample([eye,mouth,y[$-1]],[(eyeSpan[0]+eyeSpan[1])/2,
            (mouthSpan[0]+mouthSpan[1])/2,(left[$-1]+right[$-1])/2],level);
    }
}

private double[3] barycentric(Point2 point, Point2 a, Point2 b, Point2 c) {
    double determinant = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
    if (determinant == 0) return [double.nan,double.nan,double.nan];
    double u = ((point[0]-a[0])*(c[1]-a[1])-(point[1]-a[1])*(c[0]-a[0]))/determinant;
    double v = ((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))/determinant;
    return [1-u-v,u,v];
}

private bool[] cheekSupport(Point2[] world, JSONValue triangles, SkinProfile profile, int side) {
    Point2[] points; bool[] allowed;
    foreach (p; world) {
        auto q = profile.frame.local(p); points ~= q;
        allowed ~= side*(q[0]-profile.midline(q[1]))>0 && q[1]>=profile.temple && q[1]<=profile.y[$-1];
    }
    auto near = allowed.dup;
    foreach (triangle; triangles.array) {
        auto indices = ngRigNumbers(triangle); bool outside;
        foreach (index; indices) if (!near[cast(size_t)index]) outside = true;
        foreach (level; [profile.eye,profile.mouth,profile.y[$-1]]) foreach (edge; [[0,1],[1,2],[2,0]]) {
            auto p = points[cast(size_t)indices[edge[0]]], q = points[cast(size_t)indices[edge[1]]];
            if (level<min(p[1],q[1]) || level>max(p[1],q[1])) continue;
            double t = abs(q[1]-p[1])>1e-12 ? (level-p[1])/(q[1]-p[1]) : 0;
            if (side*(p[0]+t*(q[0]-p[0])-profile.midline(level))<=0) outside = true;
        }
        if (outside) foreach (index; indices) allowed[cast(size_t)index] = false;
    }
    return allowed;
}

private Point2 gridSample(Point2[] values, double[] xs, double[] ys, Point2 point) {
    enforce(values.length == xs.length*ys.length,"Invalid native face grid dimensions");
    point = [clamp(point[0],xs[0],xs[$-1]),clamp(point[1],ys[0],ys[$-1])];
    size_t x, y;
    while (x+2<xs.length && point[0]>xs[x+1]) ++x;
    while (y+2<ys.length && point[1]>ys[y+1]) ++y;
    double u = (point[0]-xs[x])/(xs[x+1]-xs[x]), v = (point[1]-ys[y])/(ys[y+1]-ys[y]);
    Point2 result;
    foreach (axis; 0 .. 2) result[axis] = (1-v)*((1-u)*values[y*xs.length+x][axis]+u*values[y*xs.length+x+1][axis])+
        v*((1-u)*values[(y+1)*xs.length+x][axis]+u*values[(y+1)*xs.length+x+1][axis]);
    return result;
}

/** Compile near-cheek Part residuals from immutable alpha and the saved native head bake. */
JSONValue ngRigCompileCheekCorrections(JSONValue state, JSONValue program, AutoRigTaskContext context = null) {
    JSONValue[] operations, observations;
    if (ngRigString(state,"kind","humanoid") != "humanoid") return JSONValue([
        "applicable":JSONValue(false),"operations":JSONValue(operations)]);
    auto keys = state["native_head_keys"].array;
    enforce(ngRigDigest(JSONValue(keys)) == state["depth_angle_program_sha256"].str,"Native head bake ownership mismatch");
    JSONValue[ulong] materials, targets;
    foreach (material; state["materials"].array) materials[ngRigUnsigned(material["uuid"])] = material;
    ulong skin; size_t greatest;
    foreach (target; state["targets"].array) {
        ulong id = ngRigUnsigned(target["part"]); targets[id] = target;
        if (materials[id]["role"].str != "face") continue;
        size_t count; auto runs = materials[id]["alpha_runs_32"].array;
        for (size_t i = 1; i<runs.length; i+=2) count += cast(size_t)ngRigUnsigned(runs[i]);
        if (count>greatest) { greatest = count; skin = id; }
    }
    enforce(greatest>0,"Cheek correction needs observed face skin");
    skin = ngRigUnsigned(program["hierarchy"]["face_origin"]);
    enforce((skin in targets) !is null && materials[skin]["role"].str == "face",
        "Cheek correction needs the hierarchy face origin");
    Frame[] eyes; Frame mouthFrame; bool hasMouth;
    foreach (mechanism; state["controls"]["mechanisms"].array) {
        if (mechanism["name"].str.endsWith("::Blink")) {
            auto curves = mechanism["contact_curves"]; Frame eye;
            eye.origin = ngRigPoint(curves["frame_origin"]); eye.tangent = ngRigPoint(curves["frame_tangent"]);
            eye.normal = [-eye.tangent[1],eye.tangent[0]];
            auto axis = ngRigNumbers(curves["tangent"]); eye.width = axis[$-1]-axis[0]; eyes ~= eye;
        }
        if (mechanism["name"].str == "Mouth::Open") {
            auto source = mechanism["source_frame"]; mouthFrame.origin = ngRigPoint(source["origin"]);
            mouthFrame.tangent = ngRigPoint(source["tangent"]); mouthFrame.width = ngRigNumber(source["width"]); hasMouth = true;
        }
    }
    enforce(eyes.length>0 && hasMouth,"Cheek correction needs observed eye and mouth anchors");
    SkinProfile profile;
    foreach (eye; eyes) { profile.frame.origin[0] += eye.origin[0]/eyes.length; profile.frame.origin[1] += eye.origin[1]/eyes.length; }
    profile.frame.tangent = eyes.length>=2 ? [eyes[$-1].origin[0]-eyes[0].origin[0],eyes[$-1].origin[1]-eyes[0].origin[1]] : eyes[0].tangent;
    double length = sqrt(profile.frame.tangent[0]^^2+profile.frame.tangent[1]^^2);
    enforce(length>0,"Degenerate cheek feature frame"); profile.frame.tangent[] /= length;
    if (profile.frame.tangent[0]<0) profile.frame.tangent[] *= -1;
    profile.frame.normal = [-profile.frame.tangent[1],profile.frame.tangent[0]];
    auto contour = ngRigAlphaContour(materials[skin],context);
    auto cloud = ngRigPoints(contour["points"]);
    Point2[] local; double minimumY = double.infinity, maximumY = -double.infinity;
    foreach (p; cloud) { auto q = profile.frame.local(p); local ~= q; minimumY = min(minimumY,q[1]); maximumY = max(maximumY,q[1]); }
    double step = ngRigNumber(contour["pixel_step"]);
    size_t count = cast(size_t)round((maximumY-minimumY)/step)+1;
    auto left = new double[count], right = new double[count]; left[] = double.infinity; right[] = -double.infinity;
    foreach (p; local) {
        size_t bin = min(count-1,cast(size_t)round((p[1]-minimumY)/step));
        left[bin] = min(left[bin],p[0]); right[bin] = max(right[bin],p[0]);
    }
    foreach (i; 0 .. count) if (right[i]>left[i]) {
        profile.y ~= minimumY+i*step; profile.left ~= left[i]; profile.right ~= right[i];
    }
    enforce(profile.y.length>=2,"Unresolved face skin sections");
    profile.eyeSpan = [double.infinity,-double.infinity]; profile.mouthSpan = [double.infinity,-double.infinity];
    foreach (eye; eyes) foreach (sign; [-1,1]) {
        Point2 p = [eye.origin[0]+sign*eye.tangent[0]*eye.width/2,eye.origin[1]+sign*eye.tangent[1]*eye.width/2];
        auto q = profile.frame.local(p); profile.eye += q[1]/(eyes.length*2);
        profile.eyeSpan[0] = min(profile.eyeSpan[0],q[0]); profile.eyeSpan[1] = max(profile.eyeSpan[1],q[0]);
    }
    foreach (sign; [-1,1]) {
        Point2 p = [mouthFrame.origin[0]+sign*mouthFrame.tangent[0]*mouthFrame.width/2,
            mouthFrame.origin[1]+sign*mouthFrame.tangent[1]*mouthFrame.width/2];
        auto q = profile.frame.local(p); profile.mouth += q[1]/2;
        profile.mouthSpan[0] = min(profile.mouthSpan[0],q[0]); profile.mouthSpan[1] = max(profile.mouthSpan[1],q[0]);
    }
    profile.temple = double.infinity;
    Point2[] anchors = [ngRigPoint(targets[skin]["origin"])] ;
    foreach (anchor; ngRigFacialLandmarks(state)) anchors ~= anchor;
    foreach (id,material; materials) {
        auto feature = ngRigString(material,"feature","");
        if ((id in targets) is null) continue;
        if (feature == "brow" || feature == "corner" || feature == "fold" || feature == "iris" ||
            feature == "lower" || feature == "sclera" || feature == "upper" || feature == "mouth")
            anchors ~= ngRigPoint(targets[id]["origin"]);
        if (feature == "brow" || feature == "upper") foreach (p; ngRigPoints(material["cloud"]))
            profile.temple = min(profile.temple,profile.frame.local(p)[1]);
    }
    enforce(profile.temple<profile.eye && profile.eye<profile.mouth && profile.mouth<profile.y[$-1],
        "Observed temple, eye, mouth and chin order is unresolved");
    auto skinTarget = targets[skin]; auto skinWorld = ngRigPoints(skinTarget["world"]);
    auto skinLocal = ngRigPoints(skinTarget["mapping"]["vertices"]);
    auto skinTriangles = skinTarget["mapping"]["triangles"];
    auto pinned = new bool[skinWorld.length];
    foreach (triangle; skinTriangles.array) {
        auto indices = ngRigNumbers(triangle);
        auto a = skinLocal[cast(size_t)indices[0]], b = skinLocal[cast(size_t)indices[1]], c = skinLocal[cast(size_t)indices[2]];
        if (abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))<1e-10) continue;
        foreach (anchor; anchors) {
            auto weights = barycentric(anchor,skinWorld[cast(size_t)indices[0]],skinWorld[cast(size_t)indices[1]],skinWorld[cast(size_t)indices[2]]);
            if (weights[0]>=-1e-7 && weights[1]>=-1e-7 && weights[2]>=-1e-7)
                foreach (index; indices) pinned[cast(size_t)index] = true;
        }
    }
    auto matrix = ngRigNumbers(skinTarget["parent_to_root"]);
    double determinant = matrix[0]*matrix[4]-matrix[1]*matrix[3];
    Point2 gridLocal(Point2 p) { return [((p[0]-matrix[2])*matrix[4]-(p[1]-matrix[5])*matrix[1])/determinant,
        (matrix[0]*(p[1]-matrix[5])-matrix[3]*(p[0]-matrix[2]))/determinant]; }
    auto xs = ngRigNumbers(skinTarget["xs"]), ys = ngRigNumbers(skinTarget["ys"]);
    Point2[] depths; foreach (depth; ngRigNumbers(skinTarget["depth"])) { Point2 p = [depth,0.]; depths ~= p; }
    double depthScale = ngRigNumber(program["native_depth_scale"]);
    foreach (key; keys) {
        if (key["grid"] != skinTarget["grid"]) continue;
        ngRigCheckpoint(context);
        auto projection = ngRigNumbers(key["projection"]); enforce(projection.length == 8,"Missing native XYZ projection");
        Point3 depthAxis = [projection[2]*projection[5]-projection[4]*projection[3],
            projection[4]*projection[1]-projection[0]*projection[5],projection[0]*projection[3]-projection[2]*projection[1]];
        double norm = sqrt(depthAxis[0]^^2+depthAxis[1]^^2+depthAxis[2]^^2); enforce(norm>1e-12,"Degenerate viewing-depth axis");
        double slope = (depthAxis[0]*profile.frame.tangent[0]+depthAxis[1]*profile.frame.tangent[1])/norm;
        int side = abs(slope)<=1e-6 ? 0 : slope>0 ? 1 : -1;
        auto values = ngRigNumbers(key["offsets"]); Point2[] offsets;
        for (size_t i = 0; i<values.length; i+=2) { Point2 p = [values[i],values[i+1]]; offsets ~= p; }
        Point2 projected(Point2 section) {
            Point2 world = [profile.frame.origin[0]+section[0]*profile.frame.tangent[0]+section[1]*profile.frame.normal[0],
                profile.frame.origin[1]+section[0]*profile.frame.tangent[1]+section[1]*profile.frame.normal[1]];
            auto delta = gridSample(offsets,xs,ys,gridLocal(world));
            return [world[0]+matrix[0]*delta[0]+matrix[1]*delta[1],world[1]+matrix[3]*delta[0]+matrix[4]*delta[1]];
        }
        auto allowedSkin = cheekSupport(skinWorld,skinTriangles,profile,side);
        Point2[] skinField;
        foreach (i,world; skinWorld) {
            Point2 result = [0.,0.];
            auto p = profile.frame.local(world);
            double lo = curveSample(profile.y,profile.left,p[1]), hi = curveSample(profile.y,profile.right,p[1]);
            double vertical = smoothStep((p[1]-profile.temple)/(profile.eye-profile.temple))*
                smoothStep((profile.y[$-1]-p[1])/(profile.y[$-1]-profile.mouth));
            if (side && allowedSkin[i] && !pinned[i] && hi>lo && vertical>0) {
                Point2 a = [lo,p[1]], b = [hi,p[1]]; auto pa = projected(a), pb = projected(b);
                double[] sectionDepth;
                foreach (station; 0 .. 129) {
                    double t = station/128.; Point2 localSection = [lo+(hi-lo)*t,p[1]];
                    Point2 wp = [profile.frame.origin[0]+localSection[0]*profile.frame.tangent[0]+p[1]*profile.frame.normal[0],
                        profile.frame.origin[1]+localSection[0]*profile.frame.tangent[1]+p[1]*profile.frame.normal[1]];
                    sectionDepth ~= gridSample(depths,xs,ys,gridLocal(wp))[0]*depthScale;
                }
                double numerator = 0, denominator = 0;
                foreach (station,z; sectionDepth) {
                    double t = station/128., ellipse = sqrt(max(0.,1-(2*t-1)^^2));
                    numerator += (z-sectionDepth[0]*(1-t)-sectionDepth[$-1]*t)*ellipse; denominator += ellipse*ellipse;
                }
                double radius = max(0.,numerator/denominator);
                Point2 u = [(pb[0]-pa[0])/2,(pb[1]-pa[1])/2], v = [radius*projection[4],radius*projection[5]];
                double len = sqrt(u[0]^^2+u[1]^^2);
                if (len>1e-8 && radius>0) {
                    Point2 direction = [u[0]/len,u[1]/len]; double du = len, dv = v[0]*direction[0]+v[1]*direction[1];
                    double divisor = max(1e-8,sqrt(du*du+dv*dv));
                    Point2 edge = side<0 ? pa : pb;
                    double inner = side<0 ? max(lo,curveSample([profile.eye,profile.mouth],profile.eyeSpan[0 .. 1] ~ profile.mouthSpan[0 .. 1],p[1])) :
                        min(hi,curveSample([profile.eye,profile.mouth],profile.eyeSpan[1 .. 2] ~ profile.mouthSpan[1 .. 2],p[1]));
                    double weight = side<0 ? smoothStep((inner-p[0])/max(1e-8,inner-lo)) : smoothStep((p[0]-inner)/max(1e-8,hi-inner));
                    foreach (axis; 0 .. 2) result[axis] = ((pa[axis]+pb[axis])/2+
                        side*(u[axis]*du+v[axis]*dv)/divisor-edge[axis])*weight*vertical;
                }
            }
            auto skinInverse = ngRigNumbers(skinTarget["root_to_local_direction"]);
            Point2 rounded = [rint((skinInverse[0]*result[0]+skinInverse[1]*result[1])*1e5)/1e5,
                rint((skinInverse[2]*result[0]+skinInverse[3]*result[1])*1e5)/1e5];
            double skinDeterminant = skinInverse[0]*skinInverse[3]-skinInverse[1]*skinInverse[2];
            Point2 roundedWorld = [(skinInverse[3]*rounded[0]-skinInverse[1]*rounded[1])/skinDeterminant,
                (skinInverse[0]*rounded[1]-skinInverse[2]*rounded[0])/skinDeterminant];
            skinField ~= roundedWorld;
        }
        foreach (id,target; targets) {
            if (materials[id]["role"].str != "face") continue;
            auto world = ngRigPoints(target["world"]); auto triangles = target["mapping"]["triangles"];
            auto allowed = cheekSupport(world,triangles,profile,side);
            auto inverse = ngRigNumbers(target["root_to_local_direction"]);
            double[] displacement; double maximum = 0;
            foreach (i,p; world) {
                Point2 field = [0.,0.];
                if (id == skin) field = skinField[i];
                else {
                    double nearest = double.infinity;
                    foreach (vertex,q; skinWorld) {
                        double distance = (p[0]-q[0])^^2+(p[1]-q[1])^^2;
                        if (distance<nearest) { nearest = distance; field = skinField[vertex]; }
                    }
                    foreach (triangle; skinTriangles.array) {
                        auto index = ngRigNumbers(triangle); size_t[3] ids = [cast(size_t)index[0],cast(size_t)index[1],cast(size_t)index[2]];
                        auto a = skinWorld[ids[0]], b = skinWorld[ids[1]], c = skinWorld[ids[2]];
                        if (abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))<1e-10) continue;
                        auto weights = barycentric(p,skinWorld[ids[0]],skinWorld[ids[1]],skinWorld[ids[2]]);
                        if (!(weights[0]>=-1e-7 && weights[1]>=-1e-7 && weights[2]>=-1e-7)) continue;
                        Point2 candidate = [0.,0.];
                        foreach (j; 0 .. 3) foreach (axis; 0 .. 2) {
                            candidate[axis] += skinField[ids[j]][axis]*weights[j];
                        }
                        field = candidate;
                        break;
                    }
                }
                Point2 localDelta = [inverse[0]*field[0]+inverse[1]*field[1],inverse[2]*field[0]+inverse[3]*field[1]];
                if (!allowed[i] || id == skin && pinned[i]) localDelta = [0.,0.];
                foreach (ref delta; localDelta) {
                    enforce(isFinite(delta),"Nonfinite cheek residual");
                    delta = rint(delta*1e5)/1e5; maximum = max(maximum,abs(delta)); displacement ~= delta;
                }
            }
            observations ~= JSONValue(["part":target["part"],"key":key["key"],"near_side":JSONValue(side),
                "near_depth_slope":JSONValue(slope),"maximum_local_residual":JSONValue(maximum)]);
            operations ~= JSONValue(["part":target["part"],"key":key["key"],"values":JSONValue(displacement),
                "allowed_vertices":JSONValue(allowed),"near_side":JSONValue(side)]);
        }
    }
    auto report = JSONValue(["applicable":JSONValue(true),"mechanism":JSONValue("alpha_near_cheek_section_envelope_v4"),
        "skin":JSONValue(skin),"operations":JSONValue(operations),"observations":JSONValue(observations),
        "skin_protected_vertices":JSONValue(pinned),
        "profile":JSONValue(["origin":JSONValue(profile.frame.origin[]),"tangent":JSONValue(profile.frame.tangent[]),
            "y":JSONValue(profile.y),"left":JSONValue(profile.left),"right":JSONValue(profile.right),
            "temple_y":JSONValue(profile.temple),"eye_y":JSONValue(profile.eye),"mouth_y":JSONValue(profile.mouth)]),
        "program_sha256":program["content_sha256"],"depth_angle_program_sha256":state["depth_angle_program_sha256"],
        "source_uv_program_sha256":state["source_uv_program_sha256"],"source_sha256":state["source_sha256"],
        "visually_reviewed":JSONValue(false)]);
    report["content_sha256"] = JSONValue(ngRigDigest(report));
    return report;
}
