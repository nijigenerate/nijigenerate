module nijigenerate.autorig.deterministic.geometry;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.linear;
import std.algorithm : sort, min, max, clamp;
import std.exception : enforce;
import std.math : abs, log, sin, cos, PI, isFinite;
import std.json : JSONValue;

private double cross(Point2 a, Point2 b) { return a[0]*b[1]-a[1]*b[0]; }
private Point2 sub(Point2 a, Point2 b) { return [a[0]-b[0],a[1]-b[1]]; }
private double squared(Point2 a) { return a[0]*a[0]+a[1]*a[1]; }
private double kernel(double r) { return r > 0 ? .5*r*log(r) : 0; }

/** Compare saved numeric meshes independently of JSON's signed and floating tags. */
bool ngRigSameTextureMapping(JSONValue a, JSONValue b) {
    foreach (key; ["vertices","uv","triangles"]) {
        if (a[key].array.length != b[key].array.length) return false;
        foreach (i,row; a[key].array) {
            auto actual = ngRigNumbers(row), expected = ngRigNumbers(b[key][i]);
            if (actual.length != expected.length) return false;
            double tolerance = key == "triangles" ? 0 : key == "uv" ? 1e-7 : 1e-5;
            foreach (j,value; actual) if (abs(value-expected[j])>tolerance) return false;
        }
    }
    return true;
}

/** Exact closest point on a triangle, including its edges and degenerate triangles. */
double[3] ngRigClosestTriangleWeights(Point2 p, Point2 a, Point2 b, Point2 c) {
    auto ab = sub(b,a), ac = sub(c,a), ap = sub(p,a);
    double determinant = cross(ab,ac);
    if (determinant != 0) {
        double v = cross(ap,ac)/determinant, w = cross(ab,ap)/determinant;
        if (v>=0 && w>=0 && v+w<=1) return [1-v-w,v,w];
    }
    Point2[3] vertices = [a,b,c]; double[3] best = [1.,0.,0.];
    double closest = squared(sub(p,a));
    foreach (i; 0 .. 3) {
        size_t j = (i+1)%3; auto edge = sub(vertices[j],vertices[i]);
        double length = squared(edge), t = length == 0 ? 0 :
            clamp((sub(p,vertices[i])[0]*edge[0]+sub(p,vertices[i])[1]*edge[1])/length,0.,1.);
        Point2 q = [vertices[i][0]+t*edge[0],vertices[i][1]+t*edge[1]];
        double distance = squared(sub(p,q));
        if (distance<closest) { closest = distance; best[] = 0; best[i] = 1-t; best[j] = t; }
    }
    return best;
}

double ngRigTriangleMinimumRatio(Point2[] rest, JSONValue triangles, double[] offsets) {
    enforce(offsets.length == rest.length*2,"Triangle validation offset count mismatch");
    double minimum = double.infinity;
    foreach (triangle; triangles.array) {
        auto values = ngRigNumbers(triangle); enforce(values.length == 3,"Invalid validation triangle");
        size_t[3] indices;
        Point2[3] base, posed;
        foreach (i; 0 .. 3) {
            enforce(values[i]>=0 && values[i]<rest.length && values[i] == cast(size_t)values[i],"Invalid triangle index");
            indices[i] = cast(size_t)values[i]; base[i] = rest[indices[i]];
            posed[i] = [base[i][0]+offsets[indices[i]*2],base[i][1]+offsets[indices[i]*2+1]];
            enforce(isFinite(posed[i][0]) && isFinite(posed[i][1]),"Nonfinite triangle pose");
        }
        double area = cross(sub(base[1],base[0]),sub(base[2],base[0]));
        if (area == 0) continue;
        minimum = min(minimum,cross(sub(posed[1],posed[0]),sub(posed[2],posed[0]))/area);
    }
    return minimum;
}

/** Verify the immutable UV-to-root affine map against every vertex of a remeshed Part. */
double ngRigVerifySourceUV(JSONValue sourceRoot, JSONValue currentRoot, double tolerance = .001) {
    auto original = ngRigPoints(sourceRoot["vertices"]), uv = ngRigPoints(sourceRoot["uv"]);
    enforce(original.length == uv.length && uv.length >= 3,"Invalid source UV frame");
    double[][] design, values;
    foreach (i,p; uv) { design ~= [p[0],p[1],1.]; values ~= original[i][].dup; }
    auto fitted = ngRigLeastSquares(design,values);
    double maximum = 0;
    void check(Point2 p, Point2 tex) {
        foreach (axis; 0 .. 2) {
            double expected = tex[0]*fitted[0][axis]+tex[1]*fitted[1][axis]+fitted[2][axis];
            enforce(isFinite(p[axis]) && isFinite(expected),"Nonfinite source UV frame");
            maximum = max(maximum,abs(p[axis]-expected));
        }
    }
    foreach (i,p; original) check(p,uv[i]);
    auto current = ngRigPoints(currentRoot["vertices"]), texture = ngRigPoints(currentRoot["uv"]);
    enforce(current.length == texture.length && current.length >= 3,"Invalid remeshed UV frame");
    foreach (i,p; current) check(p,texture[i]);
    import std.conv : to;
    enforce(maximum <= tolerance,"Remeshing or hierarchy changed the source UV-to-root frame: " ~ maximum.to!string);
    return maximum;
}

/** Preserve the source UV-to-parent map through Part TRS, without changing native mesh arrays. */
JSONValue ngRigSourceUVRegistration(JSONValue source, JSONValue current) {
    double[][] affine(JSONValue mesh) {
        auto vertices = ngRigPoints(mesh["vertices"]), uv = ngRigPoints(mesh["uv"]);
        enforce(vertices.length == uv.length && vertices.length>=3,"Invalid UV registration mesh");
        double[][] design, values;
        foreach (i,p; vertices) { design ~= [p[0],p[1],1.]; values ~= [uv[i][0],uv[i][1]]; }
        auto fitted = ngRigLeastSquares(design,values);
        foreach (i,p; vertices) foreach (axis; 0 .. 2)
            enforce(abs(p[0]*fitted[0][axis]+p[1]*fitted[1][axis]+fitted[2][axis]-uv[i][axis])<=1e-6,
                "Source UVs require non-affine registration");
        return fitted;
    }
    auto a = affine(source), b = affine(current);
    double determinant = a[0][0]*a[1][1]-a[0][1]*a[1][0];
    enforce(abs(determinant)>1e-20,"Singular source UV mapping");
    double[2][2] inverse = [[a[1][1]/determinant,-a[0][1]/determinant],
        [-a[1][0]/determinant,a[0][0]/determinant]];
    double[2][2] linear;
    foreach (i; 0 .. 2) foreach (j; 0 .. 2)
        linear[i][j] = b[i][0]*inverse[0][j]+b[i][1]*inverse[1][j];
    enforce(abs(linear[0][1])<=1e-6 && abs(linear[1][0])<=1e-6 && linear[0][0]>0 && linear[1][1]>0,
        "UV registration needs an unsupported sheared Part frame");
    Point2 shift = [(b[2][0]-a[2][0])*inverse[0][0]+(b[2][1]-a[2][1])*inverse[1][0],
        (b[2][0]-a[2][0])*inverse[0][1]+(b[2][1]-a[2][1])*inverse[1][1]];
    return JSONValue(["scale":JSONValue([linear[0][0],linear[1][1]]),"shift":JSONValue(shift[]),
        "source_affine":JSONValue(a)]);
}

private void lines(double[] axis) {
    enforce(axis.length >= 2 && axis[0] == 0 && axis[$-1] == 1, "Grid axis must span [0,1]");
    foreach (i, value; axis) enforce(isFinite(value) && (!i || value > axis[i-1]), "Unordered grid axis");
}

Point2[] ngRigUVGrid(double[] xs, double[] ys) {
    lines(xs); lines(ys);
    Point2[] result;
    foreach (y; ys) foreach (x; xs) { Point2 p = [x,y]; result ~= p; }
    return result;
}

JSONValue ngRigValidateGrid(Point2[] grid, size_t columns, double minimumRatio = 1e-8,
    Point2[] rest = null) {
    enforce(columns >= 2 && grid.length % columns == 0 && grid.length / columns >= 2,
        "Invalid grid dimensions");
    enforce(isFinite(minimumRatio) && minimumRatio >= 0, "Invalid minimum area ratio");
    enforce(rest is null || rest.length == grid.length,"Rest grid dimensions differ from posed grid");
    Point2 low = grid[0], high = grid[0];
    foreach (p; grid) foreach (axis; 0 .. 2) {
        enforce(isFinite(p[axis]), "Nonfinite grid point");
        low[axis] = min(low[axis],p[axis]); high[axis] = max(high[axis],p[axis]);
    }
    size_t rows = grid.length/columns, cells = (rows-1)*(columns-1);
    double reference = (high[0]-low[0])*(high[1]-low[1])/cells;
    double smallest = double.infinity, smallestRatio = double.infinity;
    foreach (y; 0 .. rows-1) foreach (x; 0 .. columns-1) {
        auto p00 = grid[y*columns+x], p10 = grid[y*columns+x+1];
        auto p01 = grid[(y+1)*columns+x], p11 = grid[(y+1)*columns+x+1];
        double[4] corners = [cross(sub(p10,p00),sub(p01,p00)), cross(sub(p10,p00),sub(p11,p10)),
            cross(sub(p11,p01),sub(p01,p00)), cross(sub(p11,p01),sub(p11,p10))];
        double cellReference = reference;
        if (rest !is null) {
            auto origin = rest[y*columns+x];
            cellReference = cross(sub(rest[y*columns+x+1],origin),sub(rest[(y+1)*columns+x],origin));
            enforce(cellReference>0,"Folded or degenerate rest grid");
        }
        auto threshold=max(double.min_normal,cellReference*minimumRatio);
        foreach (value; corners) {
            enforce(value > threshold, "Folded or degenerate rig grid");
            smallest = min(smallest,value); smallestRatio=min(smallestRatio,value/cellReference);
        }
    }
    return JSONValue(["cells":JSONValue(cells), "min_jacobian":JSONValue(smallest),
        "reference_cell_area":JSONValue(reference), "min_area_ratio":JSONValue(smallestRatio),
        "local_orientation_preserved":JSONValue(true), "global_injectivity_tested":JSONValue(false)]);
}

/** Exact bilinear sampling on the declared, possibly nonuniform axes. */
Point2[] ngRigSampleGrid(Point2[] grid, double[] xs, double[] ys, Point2[] query) {
    lines(xs); lines(ys);
    enforce(grid.length == xs.length*ys.length, "Grid dimensions differ from axes");
    Point2[] result;
    foreach (p; query) {
        enforce(p[0]>=0 && p[0]<=1 && p[1]>=0 && p[1]<=1, "Query is outside material UV");
        size_t x = 0, y = 0;
        while (x+2 < xs.length && p[0] > xs[x+1]) ++x;
        while (y+2 < ys.length && p[1] > ys[y+1]) ++y;
        double u = (p[0]-xs[x])/(xs[x+1]-xs[x]), v = (p[1]-ys[y])/(ys[y+1]-ys[y]);
        Point2 sample = [0.,0.];
        foreach (axis; 0 .. 2) sample[axis] =
            (1-v)*((1-u)*grid[y*xs.length+x][axis]+u*grid[y*xs.length+x+1][axis])+
            v*((1-u)*grid[(y+1)*xs.length+x][axis]+u*grid[(y+1)*xs.length+x+1][axis]);
        result ~= sample;
    }
    return result;
}

/** TPS displacement with deterministic landmark order and IDW for collinear observations. */
Point2[] ngRigFitGuide(Point2[] query, double[4] bounds, Point2[] source, Point2[] target) {
    enforce(bounds[2]>bounds[0] && bounds[3]>bounds[1] && source.length == target.length,
        "Invalid guide bounds or landmarks");
    size_t[] order;
    foreach (i; 0 .. source.length) order ~= i;
    order.sort!((a,b) => source[a][0]<source[b][0] ||
        (source[a][0]==source[b][0] && source[a][1]<source[b][1]));
    Point2[] controls, delta;
    Point2 base(Point2 uv) { return [bounds[0]+uv[0]*(bounds[2]-bounds[0]),
        bounds[1]+uv[1]*(bounds[3]-bounds[1])]; }
    foreach (i; order) {
        auto p = source[i];
        enforce(p[0]>=0 && p[0]<=1 && p[1]>=0 && p[1]<=1, "Landmark outside material UV");
        auto d = sub(target[i],base(p));
        if (controls.length && controls[$-1] == p) {
            enforce(delta[$-1] == d, "Conflicting landmark targets"); continue;
        }
        controls ~= p; delta ~= d;
    }
    bool affineRank = false;
    if (controls.length >= 3) foreach (i; 2 .. controls.length)
        if (abs(cross(sub(controls[1],controls[0]),sub(controls[i],controls[0]))) > 1e-12) affineRank = true;
    double[][] coefficients;
    size_t n = controls.length;
    if (affineRank) {
        auto matrix = ngRigZeroMatrix(n+3,n+3), right = ngRigZeroMatrix(n+3,2);
        foreach (i; 0 .. n) {
            foreach (j; 0 .. n) matrix[i][j] = kernel(squared(sub(controls[i],controls[j])));
            double[3] polynomial = [1.,controls[i][0],controls[i][1]];
            foreach (j; 0 .. 3) matrix[i][n+j] = matrix[n+j][i] = polynomial[j];
            right[i][] = delta[i][];
        }
        coefficients = ngRigSolve(matrix,right);
    }
    Point2[] result;
    foreach (p; query) {
        enforce(p[0]>=0 && p[0]<=1 && p[1]>=0 && p[1]<=1, "Query outside material UV");
        auto value = base(p);
        if (affineRank) foreach (axis; 0 .. 2) {
            value[axis] += coefficients[n][axis]+p[0]*coefficients[n+1][axis]+p[1]*coefficients[n+2][axis];
            foreach (i; 0 .. n) value[axis] += kernel(squared(sub(p,controls[i])))*coefficients[i][axis];
        } else if (n) {
            double closest = double.infinity;
            foreach (q; controls) closest = min(closest,squared(sub(p,q)));
            double total = 0; Point2 weighted = [0.,0.];
            foreach (i, q; controls) {
                double distance = squared(sub(p,q));
                if (distance == 0) { weighted = delta[i]; total = 1; break; }
                double weight = closest/distance; total += weight;
                foreach (axis; 0 .. 2) weighted[axis] += weight*delta[i][axis];
            }
            foreach (axis; 0 .. 2) value[axis] += weighted[axis]/total;
        }
        foreach (coordinate; value) enforce(isFinite(coordinate), "Nonfinite fitted guide");
        result ~= value;
    }
    return result;
}

/** Right-handed Rz(roll) Ry(yaw) Rx(pitch), with degree-valued angles. */
Point3[] ngRigRotate(Point3[] points, Point3 pivot, Point3 angles) {
    foreach (angle; angles) enforce(isFinite(angle), "Nonfinite rig angle");
    double yaw = angles[0]*PI/180, pitch = angles[1]*PI/180, roll = angles[2]*PI/180;
    Point3[] result;
    foreach (point; points) {
        if (angles == [0.,0.,0.]) { result ~= point; continue; }
        Point3 p;
        foreach (i; 0 .. 3) p[i] = point[i]-pivot[i];
        double y = cos(pitch)*p[1]-sin(pitch)*p[2], z = sin(pitch)*p[1]+cos(pitch)*p[2];
        double x = cos(yaw)*p[0]+sin(yaw)*z;
        z = -sin(yaw)*p[0]+cos(yaw)*z;
        Point3 q = [cos(roll)*x-sin(roll)*y+pivot[0],sin(roll)*x+cos(roll)*y+pivot[1],z+pivot[2]];
        result ~= q;
    }
    return result;
}
