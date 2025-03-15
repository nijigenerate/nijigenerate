module nijigenerate.core.cv.distancetransform;
/****************************************************
 * Felzenszwalb–Huttenlocher法による正確な EDT (Euclidean Distance Transform)
 * を、mir.rc.array を用いて連続領域上に確保する形で実装
 *
 * 前提:
 *   - binaryImage は mir.rc.array により作成された 3 次元配列
 *     （2 次元イメージの場合、アクセスは binaryImage[y, x, 0] を使用）
 *   - vec2u 型は inmath モジュールに存在する（座標は uint 型）
 *
 * 出力:
 *   - dist: mir.rc.array!float（2 次元配列、各画素の Euclidean 距離）
 *   - nearest: mir.rc.array!(vec2u)（2 次元配列、各画素の最近傍背景画素の座標）
 *
 * 本実装は、Felzenszwalb–Huttenlocher法に基づく2段階 EDT 計算を行います。
 ****************************************************/

import std.stdio;
import std.array;
import std.math;
import std.typecons; // Tuple
import mir.rc.array; // rcarray
import inmath;      // vec2u（例: inmath.vec2u）

enum floatINF = float.max; // INF として float.max を使用

// dt1d: 1 次元の Felzenszwalb–Huttenlocher EDT を計算する関数
// 入力: f (長さ n の float 配列、背景なら 0、対象なら INF)
// 戻り値: タプル (d, ind) で、
//    d[q] = min_{j} { (q - j)² + f[j] },
//    ind[q] = argmin_{j} { (q - j)² + f[j] }
Tuple!(float[], int[]) dt1d(const(float)[] f)
{
    int n = cast(int)f.length;  // f.length は ulong のためキャスト
    float[] d = new float[](n);
    int[] ind = new int[](n);
    int[] v = new int[](n);       // 候補となるインデックス
    float[] z = new float[](n + 1); // 分割点
    
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

/// distanceTransformEDT: Felzenszwalb–Huttenlocher法による EDT の計算
/// binaryImage: mir.rc.array により作成された 3 次元配列 (アクセスは binaryImage[y, x, 0])
/// 出力:
///   dist: mir.rc.array!float（2 次元配列、各画素の Euclidean 距離）
///   nearest: mir.rc.array!(vec2u)（2 次元配列、各画素の最近傍背景画素の座標）
void distanceTransformEDT(T)(in T binaryImage, out rcarray!float dist, out rcarray!(vec2u) nearest)
{
    int height = cast(int)binaryImage.extent[0];
    int width  = cast(int)binaryImage.extent[1];

    // f: 各画素の初期値 (背景なら 0、対象なら INF)
    auto f = rcarray!float(height, width);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            f[y, x] = (binaryImage[y, x, 0] == 0) ? 0.0f : floatINF;
        }
    }

    // 1段階目: 各行ごとに 1D EDT を計算
    auto g = rcarray!float(height, width);          // g[y, x] = min_{j} { (x - j)² + f[y, j] }
    auto rowNearest = rcarray!int(height, width);     // rowNearest[y, x] = argmin_{j} { (x - j)² + f[y, j] }
    for (int y = 0; y < height; y++) {
        auto row = f.slice(y); // 1 次元の行配列
        auto result = dt1d(row);
        for (int x = 0; x < width; x++) {
            g[y, x] = result.first[x];
            rowNearest[y, x] = result.second[x];
        }
    }

    // 2段階目: 各列ごとに 1D EDT を計算
    auto dt = rcarray!float(height, width);          // dt[y, x] = min_{i} { (y - i)² + g[i, x] }
    auto nearestArr = rcarray!(vec2u)(height, width);  // 最近傍背景画素の座標出力用
    for (int x = 0; x < width; x++) {
        float[] col = new float[](height);
        for (int y = 0; y < height; y++) {
            col[y] = g[y, x];
        }
        auto result = dt1d(col);
        float[] dcol = result.first;
        int[] colInd = result.second;
        for (int y = 0; y < height; y++) {
            dt[y, x] = dcol[y];
            int r = colInd[y];
            // 最近傍背景画素の座標は (rowNearest[r, x], r)
            nearestArr[y, x] = vec2u(rowNearest[r, x], r);
        }
    }

    // 最終出力: 各画素の Euclidean 距離 = sqrt(dt[y, x])
    auto outDist = rcarray!float(height, width);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            outDist[y, x] = sqrt(dt[y, x]);
        }
    }
    dist = outDist;
    nearest = nearestArr;
}

unittest {
    /*************************************
     * サンプル: mir.rc.array を用いた二値画像生成
     * 画像サイズ: 5 x 8, チャンネル数: 1
     * アクセスは binaryImage[y, x, 0] を使用
     *
     * この例では、対象ピクセルは 1、背景は 0 として EDT を計算します。
     *************************************/
    auto binaryImage = rcarray!int([
        [[0],[0],[0],[0],[0],[0],[0],[0]],
        [[0],[1],[1],[1],[0],[1],[1],[0]],
        [[0],[1],[0],[1],[0],[1],[1],[0]],
        [[0],[1],[1],[1],[0],[0],[0],[0]],
        [[0],[0],[0],[0],[0],[0],[0],[0]]
    ]);

    rcarray!float dist;
    rcarray!(vec2u) nearest;
    distanceTransformEDT(binaryImage, dist, nearest);

    // アサーション: 背景画素は距離 0、対象画素は正の距離
    assert(dist[0, 0] == 0.0f, "背景画素の距離が 0 でない");
    for (int y = 0; y < dist.extent[0]; y++) {
        for (int x = 0; x < dist.extent[1]; x++) {
            if (binaryImage[y, x, 0] != 0)
                assert(dist[y, x] > 0.0f, "対象画素の距離が 0 以下");
        }
    }

    writeln("Euclidean Distance Transform (EDT):");
    for (int y = 0; y < dist.extent[0]; y++) {
        for (int x = 0; x < dist.extent[1]; x++) {
            write(dist[y, x]:5:2, "\t");
        }
        writeln();
    }

    writeln("\nNearest Background Coordinates:");
    for (int y = 0; y < nearest.extent[0]; y++) {
        for (int x = 0; x < nearest.extent[1]; x++) {
            auto pt = nearest[y, x];
            write("(", pt.x, ",", pt.y, ")\t");
        }
        writeln();
    }
}
