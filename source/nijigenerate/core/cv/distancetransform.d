module nijigenerate.core.cv.distancetransform;

import std.stdio;
import std.array;
import std.math;
import std.format;
import std.typecons; // Tuple
import mir.rc.array : RCArray, rcarray;
import mir.ndslice;

enum floatINF = float.max;

/// 1D distance transform function using parabolic envelope.
Tuple!(float[], int[]) dt1d(const(float)[] f)
{
    int n = cast(int)f.length;
    float[] d = new float[](n);
    int[] ind = new int[](n);
    int[] v = new int[](n);
    float[] z = new float[](n + 1);

    z[0] = -float.infinity;
    z[1] = float.infinity;
    int k = 0;

    for (int q = 1; q < n; q++) {
        float s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * (q - v[k]));
        while (s <= z[k]) {
            k--;
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * (q - v[k]));
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = float.infinity;
    }

    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < q)
            k++;
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
        ind[q] = v[k];
    }
    return tuple(d, ind);
}

/// Generic helper: Convert a dynamic array to an RCArray given dimensions.
/// This implementation explicitly copies elements using the RCArray.ptr property.
RCArray!T convertToRCArray(T)(T[] arr, int dim0, int dim1)
{
    auto A = rcarray!T(dim0, dim1);
    size_t total = arr.length;
    foreach(i; 0 .. total) {
        A.ptr[i] = arr[i];
    }
    return A;
}
RCArray!T convertToRCArray(T)(T[] arr, int dim0, int dim1, int dim2)
{
    auto A = rcarray!T(dim0, dim1, dim2);
    size_t total = arr.length;
    foreach(i; 0 .. total) {
        A.ptr[i] = arr[i];
    }
    return A;
}
/// Compute the Euclidean Distance Transform on a binary image.
/// The input 'binaryImage' is a 2D mir.ndslice (channel last),
/// where a pixel is background if its first channel is 0, and foreground otherwise.
/// The outputs are:
///   - 'dist' (Euclidean distance) as RCArray!float,
///   - 'nearest' (nearest background coordinates) as RCArray!int.
/// Nearest coordinates are stored as a 3-dimensional RCArray!int with dimensions [height, width, 2],
/// so that you can access the element at [y, x] to get a 2-element slice [column, row].
void distanceTransform(T)(in T binaryImage, out Slice!(float*, 2, mir_slice_kind.contiguous) dist, out Slice!(int*, 3, mir_slice_kind.contiguous) nearest)
{
    int height = cast(int)binaryImage.shape[0];
    int width  = cast(int)binaryImage.shape[1];
    int total = height * width;

    // Use dynamic arrays for intermediate processing.
    float[] farr = new float[](total);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // binaryImage[y, x] is assumed to be scalar.
            farr[y * width + x] = (binaryImage[y, x] == 0) ? 0.0f : floatINF;
        }
    }

    // Process each row.
    float[] garr = new float[](total);
    int[] rowNearest = new int[](total);
    for (int y = 0; y < height; y++) {
        float[] row = new float[](width);
        for (int x = 0; x < width; x++) {
            row[x] = farr[y * width + x];
        }
        auto result = dt1d(row);
        auto drow = result.tupleof[0];
        auto indrow = result.tupleof[1];
        for (int x = 0; x < width; x++) {
            garr[y * width + x] = drow[x];
            rowNearest[y * width + x] = indrow[x];
        }
    }

    // Process each column.
    float[] dtarr = new float[](total);
    int[] nearestArr = new int[](total * 2); // Flat array: two ints per pixel.
    for (int x = 0; x < width; x++) {
        float[] col = new float[](height);
        for (int y = 0; y < height; y++) {
            col[y] = garr[y * width + x];
        }
        auto result = dt1d(col);
        auto dcol = result.tupleof[0];
        auto colInd = result.tupleof[1];
        for (int y = 0; y < height; y++) {
            dtarr[y * width + x] = dcol[y];
            int r = colInd[y];
            int idx = (y * width + x) * 2;
            // For pixel (y, x), set nearest coordinate:
            // Column index from rowNearest of row r and column x, row index = r.
            nearestArr[idx]     = rowNearest[r * width + x];
            nearestArr[idx + 1] = r;
        }
    }

    // Final pass: compute square roots.
    float[] outArr = new float[](total);
    for (int i = 0; i < total; i++) {
        outArr[i] = sqrt(dtarr[i]);
    }

    // Convert dynamic arrays to RCArray.
    int err;
    dist = outArr.sliced.reshape([height, width], err); //convertToRCArray!float(outArr, height, width);
    // Convert nearestArr to RCArray!int with initial dimensions [height, width*2].
    nearest = nearestArr.sliced.reshape([height, width, 2], err);
}

unittest {
    // Create a sample binary image using mir.rc.array.
    auto binaryImage = rcarray!int([
        [[0],[0],[0],[0],[0],[0],[0],[0]],
        [[0],[1],[1],[1],[0],[1],[1],[0]],
        [[0],[1],[0],[1],[0],[1],[1],[0]],
        [[0],[1],[1],[1],[0],[0],[0],[0]],
        [[0],[0],[0],[0],[0],[0],[0],[0]]
    ]);
    int height = cast(int)binaryImage.shape[0];
    int width  = cast(int)binaryImage.shape[1];

    RCArray!float dist;
    RCArray!int nearest;
    distanceTransform(binaryImage, dist, nearest);

    // Check that background pixel (0,0) has distance 0.
    assert(dist[0, 0] == 0.0f, "背景画素の距離が 0 でない");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (binaryImage[y, x] != 0)
                assert(dist[y, x] > 0.0f, "対象画素の距離が 0 以下");
        }
    }

    writeln("Euclidean Distance Transform (EDT):");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            writef("%5.2f\t", dist[y, x]);
        }
        writeln();
    }

    writeln("\nNearest Background Coordinates (col, row):");
    // Access nearest as a slice with dimensions [height, width, 2]
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto pt = nearest[y, x]; // Should yield int[2]
            write("(", pt[0], ",", pt[1], ")\t");
        }
        writeln();
    }
}
