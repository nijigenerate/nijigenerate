module nijigenerate.autorig.deterministic.observation;

import nijigenerate.autorig.deterministic.contracts;
import std.exception : enforce;
import std.math : abs, sqrt;
import std.json : JSONValue;
import std.algorithm : min, max;
import nijigenerate.autorig.framework : AutoRigTaskContext;
import nijigenerate.autorig.deterministic.linear : ngRigLeastSquares;

/** Zhang-Suen thinning and endpoint-to-junction traces, using only owned source alpha. */
JSONValue ngRigEyelashTopology(JSONValue material, Point2 origin, Point2 tangent,
    AutoRigTaskContext context = null) {
    auto dimensions = ngRigNumbers(material["texture_size"]);
    size_t width = cast(size_t)dimensions[0], height = cast(size_t)dimensions[1];
    enforce(width>0 && height>0 && width<=size_t.max/height,"Invalid eyelash alpha dimensions");
    auto mask = new bool[width*height];
    auto runs = material["alpha_runs_32"].array;
    for (size_t i; i<runs.length; i+=2) {
        size_t start = cast(size_t)ngRigUnsigned(runs[i]), length = cast(size_t)ngRigUnsigned(runs[i+1]);
        enforce(start<=mask.length && length<=mask.length-start,"Invalid eyelash alpha run");
        mask[start .. start+length] = true;
    }
    auto radius = alphaDistance(mask,width,height,true,context), skeleton = mask.dup;
    bool pixel(ptrdiff_t x, ptrdiff_t y) {
        return x>=0 && y>=0 && x<width && y<height && skeleton[cast(size_t)y*width+cast(size_t)x];
    }
    bool changed = true; size_t iterations;
    while (changed) {
        ngRigCheckpoint(context); changed = false; ++iterations;
        foreach (phase; 0 .. 2) {
            size_t[] deleted;
            foreach (y; 0 .. height) foreach (x; 0 .. width) {
                if (!skeleton[y*width+x]) continue;
                ptrdiff_t px = cast(ptrdiff_t)x, py = cast(ptrdiff_t)y;
                bool[8] neighbors = [pixel(px,py-1),pixel(px+1,py-1),pixel(px+1,py),pixel(px+1,py+1),
                    pixel(px,py+1),pixel(px-1,py+1),pixel(px-1,py),pixel(px-1,py-1)];
                int count, transitions;
                foreach (i,n; neighbors) { count += n; transitions += !n && neighbors[(i+1)%8]; }
                if (count<2 || count>6 || transitions!=1) continue;
                bool first = phase == 0 ? neighbors[0] && neighbors[2] && neighbors[4] :
                    neighbors[0] && neighbors[2] && neighbors[6];
                bool second = phase == 0 ? neighbors[2] && neighbors[4] && neighbors[6] :
                    neighbors[0] && neighbors[4] && neighbors[6];
                if (!first && !second) deleted ~= y*width+x;
            }
            foreach (id; deleted) skeleton[id] = false;
            changed = changed || deleted.length>0;
        }
    }
    auto mapping = material["source_root_mapping"];
    auto vertices = ngRigPoints(mapping["vertices"]), uv = ngRigPoints(mapping["uv"]);
    double[][] design, values;
    foreach (i,p; uv) { design ~= [p[0],p[1],1.]; values ~= vertices[i][].dup; }
    auto affine = ngRigLeastSquares(design,values);
    Point2 world(size_t id) {
        double u = ((id%width)+.5)/width, v = ((id/width)+.5)/height;
        return [u*affine[0][0]+v*affine[1][0]+affine[2][0],
            u*affine[0][1]+v*affine[1][1]+affine[2][1]];
    }
    size_t[] neighbors(size_t id) {
        size_t[] result; ptrdiff_t x = cast(ptrdiff_t)(id%width), y = cast(ptrdiff_t)(id/width);
        foreach (dy; -1 .. 2) foreach (dx; -1 .. 2) {
            if ((!dx && !dy) || !pixel(x+dx,y+dy)) continue;
            // Avoid diagonal shortcuts across an existing orthogonal connection.
            if (dx && dy && (pixel(x+dx,y) || pixel(x,y+dy))) continue;
            result ~= cast(size_t)(y+dy)*width+cast(size_t)(x+dx);
        }
        return result;
    }
    size_t endpoints, junctions; JSONValue[] branches; bool left, right;
    foreach (id; 0 .. skeleton.length) if (skeleton[id]) {
        if (id%65536 == 0) ngRigCheckpoint(context);
        auto links = neighbors(id);
        if (links.length>=3) ++junctions;
        if (links.length != 1) continue;
        ++endpoints;
        size_t previous = id, current = links[0]; double length = 0;
        auto start = world(id);
        while (true) {
            auto a = world(previous), b = world(current);
            length += sqrt((a[0]-b[0])^^2+(a[1]-b[1])^^2);
            auto adjacent = neighbors(current);
            if (adjacent.length != 2) break;
            auto next = adjacent[0] == previous ? adjacent[1] : adjacent[0];
            previous = current; current = next;
        }
        auto end = world(current);
        double tx = (start[0]-end[0])*tangent[0]+(start[1]-end[1])*tangent[1];
        double ny = -(start[0]-end[0])*tangent[1]+(start[1]-end[1])*tangent[0];
        auto sx = world(current%width+1<width ? current+1 : current), sy = world(current+width<skeleton.length ? current+width : current);
        double pixelScale = max(sqrt((sx[0]-end[0])^^2+(sx[1]-end[1])^^2),
            sqrt((sy[0]-end[0])^^2+(sy[1]-end[1])^^2));
        bool lash = neighbors(current).length>=3 && abs(ny)>abs(tx) && length>2*radius[current]*pixelScale;
        double coordinate = (end[0]-origin[0])*tangent[0]+(end[1]-origin[1])*tangent[1];
        if (lash) { if (coordinate<0) left = true; else right = true; }
        branches ~= JSONValue(["endpoint":JSONValue(start[]),"attachment":JSONValue(end[]),
            "length":JSONValue(length),"lash_branch":JSONValue(lash),"attachment_tangent":JSONValue(coordinate)]);
    }
    return JSONValue(["thinning_iterations":JSONValue(iterations),"endpoints":JSONValue(endpoints),
        "junctions":JSONValue(junctions),"branches":JSONValue(branches),"outer_left":JSONValue(left),
        "outer_right":JSONValue(right),"medial_resolved":JSONValue(left != right),"inner_left":JSONValue(right && !left)]);
}

private double[] distanceLine(double[] values) {
    auto distance = new double[values.length], boundary = new double[values.length+1];
    auto envelope = new size_t[values.length];
    size_t last; boundary[0] = -double.infinity; boundary[1] = double.infinity;
    foreach (q; 1 .. values.length) {
        double intersection;
        while (true) {
            auto v = envelope[last];
            intersection = (values[q]+cast(double)q*q-values[v]-cast(double)v*v)/(2.*(q-v));
            if (intersection>boundary[last] || last == 0) break;
            --last;
        }
        ++last; envelope[last] = q; boundary[last] = intersection; boundary[last+1] = double.infinity;
    }
    last = 0;
    foreach (q; 0 .. values.length) {
        while (boundary[last+1]<q) ++last;
        double delta = cast(double)q-envelope[last]; distance[q] = delta*delta+values[envelope[last]];
    }
    return distance;
}

private double[] alphaDistance(bool[] mask, size_t width, size_t height, bool foreground,
    AutoRigTaskContext context) {
    auto result = new double[mask.length];
    foreach (y; 0 .. height) {
        ngRigCheckpoint(context);
        auto line = new double[width];
        foreach (x; 0 .. width) line[x] = mask[y*width+x] == foreground ? 1e20 : 0.;
        result[y*width .. (y+1)*width] = distanceLine(line);
    }
    foreach (x; 0 .. width) {
        if (x%64 == 0) ngRigCheckpoint(context);
        auto line = new double[height];
        foreach (y; 0 .. height) line[y] = result[y*width+x];
        line = distanceLine(line);
        foreach (y; 0 .. height) result[y*width+x] = sqrt(line[y]);
    }
    return result;
}

/** Imported alpha contour with exact Euclidean signed-distance normals in model coordinates. */
JSONValue ngRigAlphaContour(JSONValue material, AutoRigTaskContext context = null, bool nominalPixelFrame = false) {
    auto dimensions = ngRigNumbers(material["texture_size"]);
    size_t width = cast(size_t)dimensions[0], height = cast(size_t)dimensions[1];
    enforce(width>1 && height>1 && width<=size_t.max/height,"Invalid contour texture dimensions");
    auto mask = new bool[width*height]; size_t opaque;
    auto runs = material["alpha_runs_32"].array;
    for (size_t i = 0; i<runs.length; i+=2) {
        size_t start = cast(size_t)ngRigUnsigned(runs[i]), count = cast(size_t)ngRigUnsigned(runs[i+1]);
        enforce(start<=mask.length && count<=mask.length-start,"Invalid alpha support run");
        mask[start .. start+count] = true; opaque += count;
    }
    auto outside = alphaDistance(mask,width,height,false,context);
    auto inside = alphaDistance(mask,width,height,true,context);
    foreach (i; 0 .. outside.length) outside[i] -= inside[i];
    auto mapping = material["source_root_mapping"];
    auto vertices = ngRigPoints(mapping["vertices"]), uv = ngRigPoints(mapping["uv"]);
    double[][] design, world;
    foreach (i,p; uv) { design ~= [p[0]*width,p[1]*height,1.]; world ~= vertices[i][]; }
    auto affine = ngRigLeastSquares(design,world);
    if (nominalPixelFrame) {
        auto bounds = ngRigNumbers(material["bounds"]);
        affine = [[(bounds[2]-bounds[0])/(width-1),0.],
            [0.,(bounds[3]-bounds[1])/(height-1)],[bounds[0],bounds[1]]];
    }
    double determinant = affine[0][0]*affine[1][1]-affine[0][1]*affine[1][0];
    enforce(abs(determinant)>1e-12,"Singular contour frame");
    Point2[] points, normals;
    foreach (y; 0 .. height) foreach (x; 0 .. width) {
        size_t pixel = y*width+x;
        if (!mask[pixel]) continue;
        if (x>0 && y>0 && x+1<width && y+1<height && mask[pixel-1] && mask[pixel+1] &&
            mask[pixel-width] && mask[pixel+width]) continue;
        double dx = x == 0 ? outside[pixel+1]-outside[pixel] : x+1 == width ?
            outside[pixel]-outside[pixel-1] : (outside[pixel+1]-outside[pixel-1])/2;
        double dy = y == 0 ? outside[pixel+width]-outside[pixel] : y+1 == height ?
            outside[pixel]-outside[pixel-width] : (outside[pixel+width]-outside[pixel-width])/2;
        Point2 normal = [(dx*affine[1][1]-dy*affine[0][1])/determinant,
            (dy*affine[0][0]-dx*affine[1][0])/determinant];
        double length = max(1e-12,sqrt(normal[0]^^2+normal[1]^^2)); normal[] /= length;
        Point2 p = [(x+.5)*affine[0][0]+(y+.5)*affine[1][0]+affine[2][0],
            (x+.5)*affine[0][1]+(y+.5)*affine[1][1]+affine[2][1]];
        points ~= p; normals ~= normal;
    }
    double xx = affine[0][0]^^2+affine[0][1]^^2, yy = affine[1][0]^^2+affine[1][1]^^2;
    double xy = affine[0][0]*affine[1][0]+affine[0][1]*affine[1][1];
    double step = sqrt(max(0.,(xx+yy-sqrt((xx-yy)^^2+4*xy*xy))/2));
    enforce(step>1e-8,"Degenerate source pixel frame");
    return JSONValue(["points":ngRigPointsJson(points),"normals":ngRigPointsJson(normals),
        "pixel_step":JSONValue(step),"opaque_pixels":JSONValue(opaque)]);
}

JSONValue ngRigPrepareShoulders(JSONValue state, JSONValue program, AutoRigTaskContext context = null) {
    JSONValue[] pairs;
    if (ngRigString(program,"kind","humanoid") != "humanoid") return JSONValue(pairs);
    JSONValue body, bodyContour; size_t greatest;
    foreach (material; state["materials"].array) if (!material["static"].boolean && material["role"].str == "torso") {
        auto support = "alpha_runs_32" in material.object;
        if (support is null) continue;
        size_t count; auto runs = support.array;
        for (size_t i = 1; i<runs.length; i+=2) count += cast(size_t)ngRigUnsigned(runs[i]);
        if (count>greatest) { greatest = count; body = material; }
    }
    if (!greatest) return JSONValue(pairs);
    bodyContour = ngRigAlphaContour(body,context,true);
    // Shoulder eligibility precedes skeleton fitting in the original pipeline.
    // Use observed evidence rather than the optimized scaffold's torso axis.
    auto evidence = state["evidence"];
    Point2 landmark(string name) {
        auto p = ngRigPoint(evidence["landmarks"][name]["xy"]);
        auto matrix = evidence["source_to_model"].array;
        auto x = ngRigNumbers(matrix[0]), y = ngRigNumbers(matrix[1]);
        return [x[0]*p[0]+x[1]*p[1]+x[2],y[0]*p[0]+y[1]*p[1]+y[2]];
    }
    auto neck = landmark("neck_base"), chest = landmark("chest");
    Point2 up = [neck[0]-chest[0],neck[1]-chest[1]];
    double norm = sqrt(up[0]^^2+up[1]^^2); enforce(norm>0,"Unresolved shoulder up axis"); up[] /= norm;
    Point2 tangent = [-up[1],up[0]];
    string[ulong] sides;
    foreach (carrier; program["carriers"].array) sides[ngRigUnsigned(carrier["part"])] = carrier["side"].str;
    foreach (material; state["materials"].array) {
        if (material["static"].boolean || material["role"].str != "arm" || !("alpha_runs_32" in material.object)) continue;
        auto side = sides[ngRigUnsigned(material["uuid"])];
        if (side != "L" && side != "R") continue;
        auto origin = landmark("shoulder." ~ side), elbow = landmark("elbow." ~ side);
        Point2 axis = [elbow[0]-origin[0],elbow[1]-origin[1]];
        double length = sqrt(axis[0]^^2+axis[1]^^2); enforce(length>0,"Degenerate upper arm"); axis[] /= length;
        Point2[] region(JSONValue contour) {
            Point2[] result;
            auto points = ngRigPoints(contour["points"]), normals = ngRigPoints(contour["normals"]);
            foreach (i,p; points) {
                Point2 delta = [p[0]-origin[0],p[1]-origin[1]];
                if (sqrt(delta[0]^^2+delta[1]^^2)<.30*length && delta[0]*axis[0]+delta[1]*axis[1]<.12*length &&
                    normals[i][0]*up[0]+normals[i][1]*up[1]>.5) result ~= p;
            }
            return result;
        }
        auto proximal = region(ngRigAlphaContour(material,context,true)), torso = region(bodyContour);
        size_t matched; double low = double.infinity, high = -double.infinity;
        foreach (p; proximal) {
            double nearest = double.infinity;
            foreach (q; torso) nearest = min(nearest,(p[0]-q[0])^^2+(p[1]-q[1])^^2);
            if (nearest<(.07*length)^^2) {
                ++matched; double station = p[0]*tangent[0]+p[1]*tangent[1];
                low = min(low,station); high = max(high,station);
            }
        }
        double fraction = proximal.length ? cast(double)matched/proximal.length : 0.;
        double span = matched ? high-low : 0.;
        pairs ~= JSONValue(["source":body["uuid"],"target":material["uuid"],"side":JSONValue(side),
            "matching":JSONValue(fraction>=.6 && span>=.18*length),"matched_fraction":JSONValue(fraction),
            "matched_span":JSONValue(span),"upper_arm_length":JSONValue(length),"origin":JSONValue(origin[]),
            "up":JSONValue(up[]),"tangent":JSONValue(tangent[]),"arm_axis":JSONValue(axis[])]);
    }
    return JSONValue(pairs);
}

/** Store alpha support as owned row-major runs; no image or editor object escapes the snapshot. */
JSONValue ngRigAlphaRuns(const(ubyte)[] rgba, ubyte threshold) {
    size_t[] runs;
    size_t start; bool inside;
    foreach (pixel; 0 .. rgba.length/4) {
        bool opaque = rgba[pixel*4+3]>threshold;
        if (opaque && !inside) { start = pixel; inside = true; }
        if (!opaque && inside) { runs ~= start; runs ~= pixel-start; inside = false; }
    }
    if (inside) { runs ~= start; runs ~= rgba.length/4-start; }
    return JSONValue(runs);
}

/** Exact source alpha lookup through the imported model's immutable native UV triangles. */
double ngRigAlphaCoverage(JSONValue material, Point2[] points, string support = "alpha_runs_128",
    bool[]* occupancy = null) {
    auto encoded = support in material.object;
    if (encoded is null || !points.length) return 0;
    auto runs = encoded.array;
    auto mapping = material["source_root_mapping"];
    auto vertices = ngRigPoints(mapping["vertices"]), uvs = ngRigPoints(mapping["uv"]);
    auto dimensions = ngRigNumbers(material["texture_size"]);
    size_t width = cast(size_t)dimensions[0], height = cast(size_t)dimensions[1], hits;
    if (occupancy !is null) *occupancy = new bool[points.length];
    foreach (pointIndex,point; points) foreach (triangle; mapping["triangles"].array) {
        auto index = ngRigNumbers(triangle);
        auto ia = cast(size_t)index[0], ib = cast(size_t)index[1], ic = cast(size_t)index[2];
        auto a = vertices[ia], b = vertices[ib], c = vertices[ic];
        double determinant = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        if (abs(determinant)<1e-14) continue;
        double s = ((point[0]-a[0])*(c[1]-a[1])-(point[1]-a[1])*(c[0]-a[0]))/determinant;
        double t = ((b[0]-a[0])*(point[1]-a[1])-(b[1]-a[1])*(point[0]-a[0]))/determinant;
        if (s < -1e-8 || t < -1e-8 || s+t>1+1e-8) continue;
        Point2 uv = [uvs[ia][0]*(1-s-t)+uvs[ib][0]*s+uvs[ic][0]*t,
            uvs[ia][1]*(1-s-t)+uvs[ib][1]*s+uvs[ic][1]*t];
        if (uv[0]>=0 && uv[0]<1 && uv[1]>=0 && uv[1]<1) {
            size_t pixel = cast(size_t)(uv[1]*height)*width+cast(size_t)(uv[0]*width);
            size_t low = 0, high = runs.length/2;
            while (low<high) {
                size_t mid = (low+high)/2;
                if (ngRigUnsigned(runs[mid*2])<=pixel) low = mid+1; else high = mid;
            }
            if (low) {
                size_t start = cast(size_t)ngRigUnsigned(runs[(low-1)*2]);
                size_t length = cast(size_t)ngRigUnsigned(runs[(low-1)*2+1]);
                if (pixel-start<length) {
                    ++hits;
                    if (occupancy !is null) (*occupancy)[pointIndex] = true;
                }
            }
        }
        break;
    }
    return cast(double)hits/points.length;
}

/** Keep the main 8-connected eye-white patch for landmark measurements only. */
ubyte[] ngRigLargestAlphaComponent(const(ubyte)[] rgba, size_t width, size_t height,
    AutoRigTaskContext context = null) {
    enforce(width>0 && height>0 && width<=size_t.max/height/4 && rgba.length == width*height*4,
        "Invalid imported texture dimensions");
    auto visited = new bool[width*height];
    size_t[] largest;
    foreach (start; 0 .. visited.length) {
        if (start % 65536 == 0) ngRigCheckpoint(context);
        if (visited[start] || rgba[start*4+3]<=32) continue;
        size_t[] queue = [start]; visited[start] = true;
        foreach (cursor; 0 .. visited.length) {
            if (cursor>=queue.length) break;
            if (cursor % 65536 == 0) ngRigCheckpoint(context);
            auto pixel = queue[cursor], x = pixel%width, y = pixel/width;
            foreach (ny; (y>0 ? y-1 : 0) .. min(height,y+2))
                foreach (nx; (x>0 ? x-1 : 0) .. min(width,x+2)) {
                    auto neighbor = ny*width+nx;
                    if (!visited[neighbor] && rgba[neighbor*4+3]>32) {
                        visited[neighbor] = true; queue ~= neighbor;
                    }
                }
        }
        if (queue.length>largest.length) largest = queue;
    }
    auto result = new ubyte[rgba.length];
    foreach (pixel; largest) result[pixel*4+3] = rgba[pixel*4+3];
    return result;
}

double ngRigAlphaPerimeterCoverage(const(ubyte)[] rgba, size_t width, size_t height) {
    enforce(width>0 && height>0 && width<=size_t.max/height/4 && rgba.length == width*height*4,
        "Invalid imported texture dimensions");
    size_t band = max(1,cast(size_t)(min(width,height)*.04)), opaque, total;
    foreach (y; 0 .. height) foreach (x; 0 .. width) {
        size_t copies = (y<band ? 1 : 0)+(y>=height-band ? 1 : 0)+
            (x<band ? 1 : 0)+(x>=width-band ? 1 : 0);
        total += copies;
        if (rgba[(y*width+x)*4+3]>32) opaque += copies;
    }
    return cast(double)opaque/total;
}

/** Resolve texture pixels through the imported mesh's declared UV triangles. */
Point2[] ngRigTextureSupport(ubyte[] rgba, size_t width, size_t height, JSONValue mesh,
    size_t maximumSamples = 40000, AutoRigTaskContext context = null) {
    enforce(width>0 && height>0 && width<=size_t.max/height/4 && rgba.length == width*height*4,
        "Invalid imported texture dimensions");
    auto vertices = ngRigPoints(mesh["vertices"]), uv = ngRigPoints(mesh["uv"]);
    auto triangles = mesh["triangles"].array;
    enforce(vertices.length == uv.length && triangles.length>0 && maximumSamples>0, "Invalid imported UV mesh");
    struct UVTriangle { size_t[3] indices; Point2 a, b, c; double determinant; }
    UVTriangle[] prepared;
    foreach (triangle; triangles) {
        auto values = ngRigNumbers(triangle);
        enforce(values.length == 3,"Invalid imported UV triangle");
        UVTriangle item;
        foreach (i, value; values) {
            enforce(value>=0 && value<vertices.length && value==cast(size_t)value,"UV index out of range");
            item.indices[i] = cast(size_t)value;
        }
        item.a = uv[item.indices[0]]; item.b = uv[item.indices[1]]; item.c = uv[item.indices[2]];
        item.determinant = (item.b[0]-item.a[0])*(item.c[1]-item.a[1])-
            (item.b[1]-item.a[1])*(item.c[0]-item.a[0]);
        if (abs(item.determinant)>=1e-14) prepared ~= item;
    }
    size_t count = 0;
    foreach (pixel; 0 .. width*height) {
        if (pixel % 65536 == 0) ngRigCheckpoint(context);
        if (rgba[pixel*4+3]>32) ++count;
    }
    size_t stride = max(1,(count+maximumSamples-1)/maximumSamples), ordinal = 0;
    Point2[] result;
    foreach (pixel; 0 .. width*height) {
        if (pixel % 65536 == 0) ngRigCheckpoint(context);
        if (rgba[pixel*4+3]<=32) continue;
        if (ordinal++ % stride) continue;
        Point2 p = [(pixel%width+.5)/width,(pixel/width+.5)/height];
        foreach (triangle; prepared) {
            auto index = triangle.indices;
            auto a = triangle.a, b = triangle.b, c = triangle.c;
            auto determinant = triangle.determinant;
            double u = ((p[0]-a[0])*(c[1]-a[1])-(p[1]-a[1])*(c[0]-a[0]))/determinant;
            double v = ((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))/determinant;
            if (u < -1e-8 || v < -1e-8 || u+v > 1+1e-8) continue;
            Point2 q = [(1-u-v)*vertices[index[0]][0]+u*vertices[index[1]][0]+v*vertices[index[2]][0],
                (1-u-v)*vertices[index[0]][1]+u*vertices[index[1]][1]+v*vertices[index[2]][1]];
            result ~= q; break;
        }
        // Pixels outside the imported drawing mesh are not visible material support.
    }
    return result;
}
