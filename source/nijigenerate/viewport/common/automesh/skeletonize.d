module nijigenerate.viewport.common.automesh.skeletonize;

import i18n;
import nijigenerate.viewport.common.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import inmath;
import dcv.core;
import dcv.imgproc;
import dcv.measure;
import mir.ndslice;
import mir.ndslice.slice;    // NDslice 用の slice 操作
import mir.ndslice.topology; // reshape() などのため
import mir.math.stat: mean;
import std.algorithm;
import std.stdio;
import std.math: abs;
import std.conv;
import std.array;

alias Point = vec2u; // vec2u は (uint x, uint y) を想定（必要に応じて調整）

/// SkeletonExtractor クラス
/// - 入力画像は ubyte[] 形式で与えられ、幅・高さから各画素は img[y][x] = img2[y*width + x] でアクセス可能とする。
/// - autoMesh() 内で、画像のバイナリ化→Zhang-Suen 細線化→連続パス抽出→RDP による制御点抽出を行い、
///   さらに制御点群を「Y 座標が最も少ない点を原点 (0,0) にする」よう平行移動します。
class SkeletonExtractor : AutoMeshProcessor {
private:
    float maskThreshold = 15;
    Point[] controlPoints; // RDP により得られた制御点群（後で平行移動済み）

public:
    override IncMesh autoMesh(Drawable target, IncMesh mesh,
                              bool mirrorHoriz = false, float axisHoriz = 0,
                              bool mirrorVert = false, float axisVert = 0)
    {
        Part part = cast(Part)target;
        if (!part)
            return mesh;

        Texture texture = part.textures[0];
        if (!texture)
            return mesh;

        ubyte[] data = texture.getTextureData();
        int width = texture.width;
        int height = texture.height;

        // Image 型は dcv.imgproc などのラッパーと仮定
        auto img = new Image(width, height, ImageFormat.IF_RGB_ALPHA);
        copy(data, img.data);
        
        // NDslice を用いて透明チャンネル（インデックス 3）を抽出
        // img.sliced[...] は NDslice を返すので、dup してミュータブルなビューを得る
        auto imbin = img.sliced[0 .. $, 0 .. $, 3].dup;
        // ※ imbin の shape は [height, width] となる

        // マスク化: 各画素を maskThreshold と比較し、値を 0 か 255 にする
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;
            }
        }

        // 細線化（NDslice のビュー imbin をそのまま変更）
        zhangSuenThinning(imbin);

        // DFS で連続パス抽出
        auto path = extractPath(imbin, width, height);

        // RDP による曲線単純化
        controlPoints = rdp(path, 1.0); // 例：epsilon = 1.0

        // 制御点群の座標調整（Y 座標が最も小さい点を原点に）
        adjustCoordinates();

        foreach (Point pt; controlPoints) {
            vec2 position = vec2(pt.x, pt.y);
            mesh.vertices ~= new MeshVertex(position);
        }

        return mesh.autoTriangulate();
    }

    override void configure() {
        // 必要に応じて実装
    }

    /// 抽出された制御点の取得
    Point[] getControlPoints() {
        return controlPoints;
    }

private:
    /////////////////////////////////////////////////////////////
    // 2. Zhang-Suen 細線化アルゴリズム（NDslice を用いる）
    void zhangSuenThinning(T)(T imbin)
    {
        bool changed = true;
        long loop = 0;
        while (changed) {
            long marked = 0;
            long altered = 0;
            changed = false;
            writefln("loop: %d", loop ++);
            // サブイテレーション1用のマーカー（imbin と同じ形状の NDslice）
            auto markers = imbin.dup; // 値の複製。以降、0 を設定する場所がマーカーとする

            // サブイテレーション 1
            for (int i = 1; i < imbin.shape[0] - 1; i++) {
                for (int j = 1; j < imbin.shape[1] - 1; j++) {
                    if (!imbin[i, j])
                        continue;
                    int bp = countNeighbors(imbin, i, j);
                    if (bp < 2 || bp > 6)
                        continue;
                    int ap = countTransitions(imbin, i, j);
                    if (ap != 1)
                        continue;
                    if (imbin[i - 1, j] && imbin[i, j + 1] && imbin[i + 1, j])
                        continue;
                    if (imbin[i, j + 1] && imbin[i + 1, j] && imbin[i, j - 1])
                        continue;
                    // マーカーとして 0 を設定（削除候補）
                    markers[i, j] = 0;
                    changed = true;
                    marked ++;
                }
            }
            // マーカーに従い削除 (サブイテレーション 1)
            foreach (i; 0..markers.shape[0]) {
                foreach (j; 0..markers.shape[1]) {
                    if (markers[i, j] == 0) {
                        imbin[i, j] = 0;
                        altered ++;
                    }
                }
            }
            writefln(" sub-iter1: marked=%d, new=%d", marked, altered);
            // サブイテレーション 2 用のマーカー再生成
            marked = 0;
            altered = 0;
            markers = imbin.dup;
            // サブイテレーション 2
            for (int i = 1; i < imbin.shape[0] - 1; i++) {
                for (int j = 1; j < imbin.shape[1] - 1; j++) {
                    if (!imbin[i, j])
                        continue;
                    int bp = countNeighbors(imbin, i, j);
                    if (bp < 2 || bp > 6)
                        continue;
                    int ap = countTransitions(imbin, i, j);
                    if (ap != 1)
                        continue;
                    if (imbin[i - 1, j] && imbin[i, j + 1] && imbin[i, j - 1])
                        continue;
                    if (imbin[i - 1, j] && imbin[i + 1, j] && imbin[i, j - 1])
                        continue;
                    markers[i, j] = 0;
                    changed = true;
                    marked ++;
                }
            }
            // マーカーに従い削除 (サブイテレーション 2)
            foreach (i; 0..markers.shape[0]) {
                foreach (j; 0..markers.shape[1]) {
                    if (markers[i, j] == 0) {
                        imbin[i, j] = 0;
                        altered ++;
                    }
                }
            }
            writefln(" sub-iter2: marked=%d, new=%d", marked, altered);
        }
    }

    /// 8近傍（上下左右・斜め）の前景画素数を返す
    int countNeighbors(T)(T image, int i, int j)
    {
        int count = 0;
//        writefln("  countNeighbors");
        // ループで -1 から +1 のオフセット
        for (int di = -1; di <= 1; di++) {
            for (int dj = -1; dj <= 1; dj++) {
                if (di == 0 && dj == 0)
                    continue;
                if (image[i + di, j + dj])
                    count++;
            }
        }
        return count;
    }

    /// 8近傍（p2～p9）のうち、背景から前景へ変化する回数を返す
    int countTransitions(T)(T image, int i, int j)
    {
//        writefln("  countTransitions");
        ubyte[8] neighbors;
        neighbors[0] = image[i - 1, j];      // p2
        neighbors[1] = image[i - 1, j + 1];  // p3
        neighbors[2] = image[i, j + 1];      // p4
        neighbors[3] = image[i + 1, j + 1];  // p5
        neighbors[4] = image[i + 1, j];      // p6
        neighbors[5] = image[i + 1, j - 1];  // p7
        neighbors[6] = image[i, j - 1];      // p8
        neighbors[7] = image[i - 1, j - 1];  // p9

        int transitions = 0;
        foreach (k; 0 .. 8) {
            if (!neighbors[k] && neighbors[(k + 1) % 8])
                transitions++;
        }
        return transitions;
    }

    /////////////////////////////////////////////////////////////
    // 3. スケルトン上の連続パスを DFS で抽出する（visited も NDslice で管理）
    Point[] extractPath(T)(T skeleton, int width, int height)
    {
        writefln("extractPath");
        if (skeleton.length == 0) return [];
        Point[] endpoints;

        // visited 用の 1 次元配列を用意し、NDslice として reshape する
        ubyte[] visitedData = new ubyte[height * width];
        int err;
        auto visited = visitedData.sliced.reshape([height, width], err);

        // 端点探索（各ピクセルの 8 近傍の前景画素数が 1 のもの）
        for (int i = 1; i < height - 1; i++) {
            for (int j = 1; j < width - 1; j++) {
                if (!skeleton[i, j])
                    continue;
                int nb = countNeighbors(skeleton, i, j);
                if (nb == 1)
                    endpoints ~= Point(j, i);
            }
        }
        Point start;
        if (endpoints.length > 0) {
            start = endpoints[0];
        } else {
            bool found = false;
            for (int i = 0; i < height && !found; i++) {
                for (int j = 0; j < width && !found; j++) {
                    if (skeleton[i, j]) {
                        start = Point(j, i);
                        found = true;
                    }
                }
            }
        }
        Point[] path;
        Point current = start;
        path ~= current;
        visited[current.y, current.x] = 1;
        bool done = false;
        while (!done) {
            bool foundNeighbor = false;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if (di == 0 && dj == 0)
                        continue;
                    int ny = current.y + di;
                    int nx = current.x + dj;
                    if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    if (skeleton[ny, nx] && !visited[ny, nx]) {
                        current = Point(nx, ny);
                        path ~= current;
                        visited[ny, nx] = 1;
                        foundNeighbor = true;
                        break;
                    }
                }
                if (foundNeighbor)
                    break;
            }
            if (!foundNeighbor)
                done = true;
        }
        return path;
    }

    /////////////////////////////////////////////////////////////
    // 4. Ramer-Douglas-Peucker (RDP) アルゴリズムによる曲線単純化
    Point[] rdp(Point[] points, double epsilon) {
        if (points.length < 3)
            return points;
        double dmax = 0.0;
        int index = 0;
        Point start = points[0];
        Point end = points[$ - 1];
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        double lineLength = sqrt(dx * dx + dy * dy);
        for (int i = 1; i < points.length - 1; i++) {
            double dist = 0.0;
            if (lineLength == 0)
                dist = sqrt(cast(double)pow(points[i].x - start.x, 2) + pow(points[i].y - start.y, 2));
            else {
                double cross = abs(dx * (start.y - points[i].y) - (start.x - points[i].x) * dy);
                dist = cross / lineLength;
            }
            if (dist > dmax) {
                index = i;
                dmax = dist;
            }
        }
        if (dmax > epsilon) {
            auto recResults1 = rdp(points[0 .. index + 1], epsilon);
            auto recResults2 = rdp(points[index .. points.length], epsilon);
            // 重複する点を除いて結合
            return recResults1[0 .. $ - 1] ~ recResults2;
        } else {
            return [start, end];
        }
    }

    /////////////////////////////////////////////////////////////
    // 5. 制御点の座標を調整
    //    一筆書きになっている controlPoints の中から、Y 座標が最も少ないものを原点 (0,0) に平行移動する。
    void adjustCoordinates() {
        /*
        if (controlPoints.length == 0)
            return;
        uint minY = controlPoints[0].y;
        Point origin = controlPoints[0];
        foreach (pt; controlPoints) {
            if (pt.y < minY) {
                minY = pt.y;
                origin = pt;
            }
        }
        for (size_t i = 0; i < controlPoints.length; i++) {
            controlPoints[i].x -= origin.x;
            controlPoints[i].y -= origin.y;
        }
        */
    }

public:
    override
    string icon() {
        return "\uefb1";
    }
}