module nijigenerate.viewport.common.automesh.optimum;

import std.stdio;
import std.exception;
import std.array;
import std.math;
import mir.ndslice;
import mir.ndslice.slice; // Sliced, reshape などを利用
import std.algorithm.iteration : map;
alias stdMap = map;
import std.algorithm;

// ---------------------------------------------------------------------
// 型定義：画像データ、点群データなどを1次元配列として管理する前提
// ---------------------------------------------------------------------
struct UByteImage {
    // 画像データは1次元の ubyte[] として保持し、width, height で管理する
    ubyte[] data;
    int width;
    int height;
}

alias FloatMatrix = float[]; // 1次元 float 配列として管理（shape 情報は関数引数等で渡す）

// Point に等価演算子の定義
struct Point {
    float x;
    float y;

    bool opEquals(const Point other) const {
        return (abs(x - other.x) < 1e-6) && (abs(y - other.y) < 1e-6);
    }
}

// Triangle に等価演算子の定義
struct Triangle {
    Point a;
    Point b;
    Point c;

    bool opEquals(const Triangle other) const {
        return (a == other.a) && (b == other.b) && (c == other.c);
    }
}

alias Points = Point[];

struct Contour {
    // 輪郭は1次元配列の Point として保持
    Points pts;
}

// ---------------------------------------------------------------------
// 外部関数の宣言（OpenCV 相当の関数）
// ---------------------------------------------------------------------

private {
UByteImage cvFlip(in UByteImage image, int flipCode)
{
    UByteImage result;
    result.width = image.width;
    result.height = image.height;
    result.data = new ubyte[image.data.length];

    // 画像は1次元配列 data に row-major で格納されている前提
    auto w = image.width;
    auto h = image.height;

    switch (flipCode)
    {
        case 0: // 垂直反転：行の順序を反転
            for (int row = 0; row < h; row++) {
                int srcRowStart = row * w;
                int dstRowStart = (h - 1 - row) * w;
                for (int col = 0; col < w; col++) {
                    result.data[dstRowStart + col] = image.data[srcRowStart + col];
                }
            }
            break;
        case 1: // 水平方向反転：各行内で左右を反転
            for (int row = 0; row < h; row++) {
                int rowStart = row * w;
                for (int col = 0; col < w; col++) {
                    result.data[rowStart + (w - 1 - col)] = image.data[rowStart + col];
                }
            }
            break;
        case -1: // 両方向反転：垂直反転＋水平方向反転
            for (int row = 0; row < h; row++) {
                int srcRowStart = row * w;
                int dstRowStart = (h - 1 - row) * w;
                for (int col = 0; col < w; col++) {
                    result.data[dstRowStart + (w - 1 - col)] = image.data[srcRowStart + col];
                }
            }
            break;
        default:
            // 未定義の場合は単に入力画像のコピーを返す
            result.data = image.data.dup;
            break;
    }
    return result;
}

void cvThreshold(in UByteImage src, ref UByteImage dst, double thresh, double maxVal, int type)
{
    // 出力画像のサイズを入力画像と同じに設定
    dst.width = src.width;
    dst.height = src.height;
    dst.data = new ubyte[src.data.length];

    // 閾値と最大値を ubyte に変換（仮定として、値が [0, 255] 内にあるとする）
    ubyte threshold = cast(ubyte)thresh;
    ubyte maxValue = cast(ubyte)maxVal;

    // 各画素について、閾値処理を行う
    foreach (i, pix; src.data)
    {
        dst.data[i] = (pix >= threshold) ? maxValue : 0;
    }
}

void cvCvtColor(in UByteImage src, ref UByteImage dst, int code)
{
    // 入力画像は BGR 形式なので、1画素あたり3バイトと仮定
    int numPixels = src.width * src.height;
    enforce(src.data.length == numPixels * 3, "入力画像のデータサイズが不正です。");

    // 出力画像はグレースケールなので、1画素あたり1バイト
    dst.width = src.width;
    dst.height = src.height;
    dst.data = new ubyte[numPixels];

    // 各画素について BGR から Gray に変換
    // 輝度の計算: gray = 0.299 * R + 0.587 * G + 0.114 * B
    for (int i = 0; i < numPixels; i++)
    {
        int baseIndex = i * 3;
        ubyte B = src.data[baseIndex + 0];
        ubyte G = src.data[baseIndex + 1];
        ubyte R = src.data[baseIndex + 2];
        float grayF = 0.299f * R + 0.587f * G + 0.114f * B;
        // 四捨五入して ubyte にキャスト
        dst.data[i] = cast(ubyte)(grayF + 0.5f);
    }
}

// 1D 距離変換（Felzenszwalb–Huttenlocher 法）
// f: 入力関数（各画素の初期値、背景なら 0、その他は infinity）
// n: 配列長
float[] dt1d(const(float)[] f, int n)
{
    float[] d = new float[n];
    // v: 一時的なインデックス配列、z: 分割点（境界）を保持する
    int[] v = new int[n];
    float[] z = new float[n + 1];

    int k = 0;
    v[0] = 0;
    z[0] = -float.infinity;
    z[1] = float.infinity;

    for (int q = 1; q < n; q++)
    {
        // 平方項の違いによる交点を計算
        float s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * (q - v[k]));
        while (s <= z[k])
        {
            k--;
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0f * (q - v[k]));
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = float.infinity;
    }
    k = 0;
    for (int q = 0; q < n; q++)
    {
        while (z[k + 1] < q)
            k++;
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
    }
    return d;
}

///
/// 距離変換（Felzenszwalb–Huttenlocher の方法）
/// 入力: binary (UByteImage) --- 画像サイズは width×height, 
///        binary.data には背景は 0、その他は非 0 とする
/// 出力: distTransform --- 各画素について、背景画素までのユークリッド距離
///
void cvDistanceTransform(in UByteImage binary, ref FloatMatrix distTransform, int distanceType, int maskSize)
{
    // ※ distanceType, maskSize は本実装では使用しません

    int w = binary.width;
    int h = binary.height;
    int numPixels = w * h;

    // 初期関数 f: 背景なら 0、その他は infinity
    float[] f = new float[numPixels];
    for (int i = 0; i < numPixels; i++)
    {
        f[i] = (binary.data[i] == 0) ? 0.0f : float.infinity;
    }

    // 1段階目: 水平方向の 1D 距離変換
    float[] d = new float[numPixels];
    for (int i = 0; i < h; i++)
    {
        // 行単位のスライス（1次元配列として取り出す）
        int rowStart = i * w;
        float[] rowF = f[rowStart .. rowStart + w];
        float[] rowD = dt1d(rowF, w);
        // 結果を d の該当部分に保存
        d[rowStart .. rowStart + w] = rowD[];
    }

    // 2段階目: 垂直方向の 1D 距離変換
    float[] distSquared = new float[numPixels];
    for (int j = 0; j < w; j++)
    {
        float[] colF = new float[h];
        // 各列 j の値を収集
        for (int i = 0; i < h; i++)
        {
            colF[i] = d[i * w + j];
        }
        float[] colD = dt1d(colF, h);
        // 結果を distSquared に格納
        for (int i = 0; i < h; i++)
        {
            distSquared[i * w + j] = colD[i];
        }
    }

    // 最終結果: 各画素の距離は sqrt(二乗距離)
    distTransform = new float[numPixels];
    for (int i = 0; i < numPixels; i++)
    {
        distTransform[i] = sqrt(distSquared[i]);
    }
}

void cvFillPoly(ref UByteImage dst, in Triangle triangle, ubyte fillValue)
{
    // 三角形の頂点座標を四捨五入して整数に変換
    int ax = cast(int)floor(triangle.a.x + 0.5f);
    int ay = cast(int)floor(triangle.a.y + 0.5f);
    int bx = cast(int)floor(triangle.b.x + 0.5f);
    int by = cast(int)floor(triangle.b.y + 0.5f);
    int cx = cast(int)floor(triangle.c.x + 0.5f);
    int cy = cast(int)floor(triangle.c.y + 0.5f);

    // バウンディングボックスを求める
    int minX = min(ax, min(bx, cx));
    int maxX = max(ax, max(bx, cx));
    int minY = min(ay, min(by, cy));
    int maxY = max(ay, max(by, cy));

    // 画像領域にクランプ
    minX = max(minX, 0);
    minY = max(minY, 0);
    maxX = min(maxX, dst.width - 1);
    maxY = min(maxY, dst.height - 1);

    // エッジ関数: 点 p と辺 (a,b) に対し、p が辺の左側にあるかの符号を返す
    auto edgeFunction = (Point a, Point b, Point p) {
         return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
    };

    // 各ピクセルの中心座標 (x+0.5, y+0.5) を用いて内部判定
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            Point p = Point(x + 0.5f, y + 0.5f);
            float w0 = edgeFunction(triangle.b, triangle.c, p);
            float w1 = edgeFunction(triangle.c, triangle.a, p);
            float w2 = edgeFunction(triangle.a, triangle.b, p);
            // 全ての符号が同じなら内部にあると判定
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
            {
                dst.data[y * dst.width + x] = fillValue;
            }
        }
    }
}

void cvBitwiseAnd(in UByteImage src1, in UByteImage src2, ref UByteImage dst)
{
    // 入力画像のサイズが一致していることを確認
    enforce(src1.width == src2.width && src1.height == src2.height, "画像サイズが一致しません");
    
    dst.width = src1.width;
    dst.height = src1.height;
    dst.data = new ubyte[src1.data.length];

    foreach (i, pixel1; src1.data)
    {
        // 論理AND を各画素ごとに実施
        dst.data[i] = pixel1 & src2.data[i];
    }
}

void cvSubtract(in UByteImage src1, in UByteImage src2, ref UByteImage dst)
{
    // 入力画像のサイズが一致していることを確認
    enforce(src1.width == src2.width && src1.height == src2.height, "画像サイズが一致しません");

    dst.width = src1.width;
    dst.height = src1.height;
    dst.data = new ubyte[src1.data.length];

    foreach (i, pixel1; src1.data)
    {
        // 減算後に負の値が出ないように 0 でクリッピング
        int diff = cast(int)pixel1 - cast(int)src2.data[i];
        dst.data[i] = cast(ubyte)(diff < 0 ? 0 : diff);
    }
}

/// cvFindContours (Suzuki–Abe 法に基づく高速輪郭抽出)
/// mode, method は本実装では固定動作（外部輪郭抽出、近似なし）とする。
Contour[] cvFindContours(in UByteImage binary, int mode, int method)
{
    int w = binary.width;
    int h = binary.height;
    int numPixels = w * h;
    bool[] visited = new bool[numPixels];
    Contour[] contours;

    // 8近傍（時計回り順：上、右上、右、右下、下、左下、左、左上）
    int[8] dRow = [ -1, -1,  0, 1, 1, 1, 0, -1 ];
    int[8] dCol = [  0,  1,  1, 1, 0, -1, -1, -1 ];

    // 画像境界内かを判定するラムダ
    auto inRange = (int r, int c) => (r >= 0 && r < h && c >= 0 && c < w);

    // 輪郭追跡（border following）のヘルパー
    Points followContour(int startR, int startC)
    {
        Points contour;
        int curR = startR;
        int curC = startC;
        // b_prev は「前回確認した隣接方向」の逆方向（初期は 7 とする）
        int b_prev = 7;
        bool closed = false;
        do {
            // 輪郭上の点を追加（座標は中心とする）
            contour ~= Point(curC + 0.5f, curR + 0.5f);
            // 探索方向は、b_prev+1 から時計回りに
            int foundDir = -1;
            int dir = (b_prev + 1) % 8;
            for (int i = 0; i < 8; i++) {
                int nr = curR + dRow[dir];
                int nc = curC + dCol[dir];
                if (inRange(nr, nc) && binary.data[nr * w + nc] != 0) {
                    foundDir = dir;
                    break;
                }
                dir = (dir + 1) % 8;
            }
            if (foundDir < 0) {
                // 周囲に前景が見つからなかった場合は終了
                break;
            }
            // 次の点
            int nextR = curR + dRow[foundDir];
            int nextC = curC + dCol[foundDir];
            // 更新 b_prev：探査開始方向は (foundDir + 6) mod 8（逆方向の 2 つ前）
            b_prev = (foundDir + 6) % 8;
            curR = nextR;
            curC = nextC;
            // 輪郭が閉じたか判定：開始点に戻れば終了
            if (curR == startR && curC == startC)
                closed = true;
        } while (!closed);
        return contour;
    }

    // 画像全体を走査して輪郭抽出
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            int idx = r * w + c;
            if (!visited[idx] && binary.data[idx] != 0) {
                // 輪郭追跡開始
                Points contour = followContour(r, c);
                // マーク：追跡した輪郭の領域は全て visited にする
                // ※ 簡易のため、輪郭線上の点のみマーク（完全な領域塗りつぶしは Suzuki の実装で行うが、ここでは省略）
                foreach(pt; contour) {
                    int pr = cast(int)pt.y;
                    int pc = cast(int)pt.x;
                    if (inRange(pr, pc))
                        visited[pr * w + pc] = true;
                }
                // 輪郭が十分な長さの場合にのみ採用
                if (contour.length > 0)
                    contours ~= Contour(contour);
            }
        }
    }
    return contours;
}

// まず、スーパー三角形を返すヘルパー
Triangle superTriangle(Points pts)
{
    // すべての点を含む軸平行の外接矩形を求める
    float minX = pts[0].x, minY = pts[0].y;
    float maxX = pts[0].x, maxY = pts[0].y;
    foreach (p; pts) {
        if (p.x < minX) minX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.x > maxX) maxX = p.x;
        if (p.y > maxY) maxY = p.y;
    }
    float dx = maxX - minX;
    float dy = maxY - minY;
    float deltaMax = dx > dy ? dx : dy;
    float midX = (minX + maxX) / 2.0f;
    float midY = (minY + maxY) / 2.0f;
    // スーパー三角形の頂点を決定（十分大きい三角形）
    Point p1 = Point(midX - 20 * deltaMax, midY - deltaMax);
    Point p2 = Point(midX, midY + 20 * deltaMax);
    Point p3 = Point(midX + 20 * deltaMax, midY - deltaMax);
    return Triangle(p1, p2, p3);
}

///
/// Bowyer–Watson 法による Delaunay 三角形分割
/// 入力: vertices は 1次元の float[]（サイズは 2 * vertexCount）
///        vertexCount: 頂点数
///
/// Bowyer–Watson 法による Delaunay 三角形分割
// Bowyer–Watson 法による Delaunay 三角形分割
Triangle[] cvSubdiv2DInsert(UByteImage binary, FloatMatrix vertices, int vertexCount, int margin)
{
    import std.algorithm.iteration : filter;
    import std.algorithm.searching : canFind;
    import std.array : array;
    // 1. 頂点集合を Point[] に変換
    Points pts = flattenVertices(vertices);
    
    // 2. スーパー三角形の作成（すべての点を含む外接矩形から作成）
    Triangle superTri = (() {
        float minX = pts[0].x, minY = pts[0].y;
        float maxX = pts[0].x, maxY = pts[0].y;
        foreach(p; pts)
        {
            if (p.x < minX) minX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.x > maxX) maxX = p.x;
            if (p.y > maxY) maxY = p.y;
        }
        float dx = maxX - minX;
        float dy = maxY - minY;
        float deltaMax = dx > dy ? dx : dy;
        float midX = (minX + maxX) / 2.0f;
        float midY = (minY + maxY) / 2.0f;
        Point p1 = Point(midX - 20 * deltaMax, midY - deltaMax);
        Point p2 = Point(midX, midY + 20 * deltaMax);
        Point p3 = Point(midX + 20 * deltaMax, midY - deltaMax);
        return Triangle(p1, p2, p3);
    })();
    
    // 初期三角形集合にスーパー三角形を登録
    Triangle[] triangulation;
    triangulation ~= superTri;
    
    // 3. 各頂点を順次挿入
    foreach (p; pts)
    {
        Triangle[] badTriangles;
        // 外接円内に p を含む三角形を収集
        foreach (tri; triangulation)
        {
            float ax = tri.a.x, ay = tri.a.y;
            float bx = tri.b.x, by = tri.b.y;
            float cx = tri.c.x, cy = tri.c.y;
            float d = 2.0f * (ax*(by - cy) + bx*(cy - ay) + cx*(ay - by));
            if (abs(d) < 1e-6)
                continue;
            float ax2 = ax*ax + ay*ay;
            float bx2 = bx*bx + by*by;
            float cx2 = cx*cx + cy*cy;
            float centerX = (ax2*(by - cy) + bx2*(cy - ay) + cx2*(ay - by)) / d;
            float centerY = (ax2*(cx - bx) + bx2*(ax - cx) + cx2*(bx - ax)) / d;
            float r2 = (ax - centerX)*(ax - centerX) + (ay - centerY)*(ay - centerY);
            float dx_ = p.x - centerX;
            float dy_ = p.y - centerY;
            if (dx_*dx_ + dy_*dy_ <= r2 + 1e-6)
                badTriangles ~= tri;
        }
        // 4. 辺集合の構築：badTriangles の各辺のうち、共有されていないものが穴の境界
        struct Edge { Point a; Point b; }
        Edge[] polygon;
        foreach (tri; badTriangles)
        {
            Edge[] edges = [ Edge(tri.a, tri.b), Edge(tri.b, tri.c), Edge(tri.c, tri.a) ];
            foreach (e; edges)
            {
                bool isShared = false;
                foreach (otherTri; badTriangles)
                {
                    if (otherTri == tri)
                        continue;
                    Edge[] otherEdges = [ Edge(otherTri.a, otherTri.b),
                                            Edge(otherTri.b, otherTri.c),
                                            Edge(otherTri.c, otherTri.a) ];
                    foreach (oe; otherEdges)
                    {
                        // 順序を無視した比較
                        if ((e.a == oe.a && e.b == oe.b) ||
                            (e.a == oe.b && e.b == oe.a))
                        {
                            isShared = true;
                            break;
                        }
                    }
                    if (isShared)
                        break;
                }
                if (!isShared)
                    polygon ~= e;
            }
        }
        // 5. triangulation から badTriangles を削除
        triangulation = triangulation.filter!( (tri) => !badTriangles.canFind(tri) ).array;
        // 6. 新たな三角形を形成して追加
        foreach (edge; polygon)
        {
            triangulation ~= Triangle(edge.a, edge.b, p);
        }
    }
    
    // 7. スーパー三角形と共有する三角形を削除
    Triangle[] finalTriangles;
    foreach (tri; triangulation)
    {
        bool containsSuper = (tri.a == superTri.a || tri.a == superTri.b || tri.a == superTri.c) ||
                             (tri.b == superTri.a || tri.b == superTri.b || tri.b == superTri.c) ||
                             (tri.c == superTri.a || tri.c == superTri.b || tri.c == superTri.c);
        if (!containsSuper)
            finalTriangles ~= tri;
    }
    return finalTriangles;
}

}

// ---------------------------------------------------------------------
// 補助関数群（1次元配列として管理し、mir の Sliced で形状変換）
// ---------------------------------------------------------------------
bool areImagesLeftRightSymmetric(UByteImage img1, UByteImage img2, int tol = 20)
{
    auto img2Mirror = cvFlip(img2, 1);
    enforce(img1.width == img2Mirror.width && img1.height == img2Mirror.height,
            "画像サイズが一致しません");

    // 1次元配列として全画素の差の絶対値の平均を計算
    double sumDiff = 0;
    foreach(i, pix; img1.data)
        sumDiff += abs(cast(int)pix - cast(int)img2Mirror.data[i]);
    return (sumDiff / img1.data.length) < tol;
}

Points mirrorPoints(Points points, int imageWidth)
{
    // stdMap を用いて、各 Point から [x, y] の float[] を生成し、それを1次元配列に連結
    float[] flat = points.stdMap!(p => [p.x, p.y]).joiner.array; // flat のサイズは 2 * points.length
    // ここでは flat をループで処理して x 値を反転
    for (size_t i = 0; i < points.length; i++) {
        flat[i * 2] = (imageWidth - 1) - flat[i * 2];
    }
    // flat から Point[] に再構築
    Points mirrored;
    for (size_t i = 0; i < points.length; i++) {
        mirrored ~= Point(flat[i * 2], flat[i * 2 + 1]);
    }
    return mirrored;
}

UByteImage extractSkeleton(UByteImage binaryImage)
{
    // 仮実装：実際は外部のスケルトン化関数を呼び出す前提
    return binaryImage;
}

FloatMatrix calculateWidthMap(in UByteImage binaryImage, in UByteImage skeleton)
{
    FloatMatrix distTransform;
    cvDistanceTransform(binaryImage, distTransform, /*distanceType*/ 1, /*maskSize*/ 5);
    // widthMap は1次元配列、サイズは画像全画素数（width * height）
    FloatMatrix widthMap = new float[binaryImage.data.length];
    for (size_t i = 0; i < skeleton.data.length; i++) {
        if (skeleton.data[i] > 0)
            widthMap[i] = distTransform[i] * 2;
        else
            widthMap[i] = 0;
    }
    return widthMap;
}

Point calculateNormalVector(Point p1, Point p2)
{
    float vx = p2.y - p1.y;
    float vy = p1.x - p2.x;
    float norm = sqrt(vx * vx + vy * vy);
    return (norm == 0) ? Point(0, 0) : Point(vx / norm, vy / norm);
}

Points thinPoints(Points points, float minDistance)
{
    if (points.length == 0)
        return points;
    Points thinned;
    thinned ~= points[0];
    foreach(p; points[1 .. $])
    {
        float dx = p.x - thinned[$ - 1].x;
        float dy = p.y - thinned[$ - 1].y;
        if (sqrt(dx * dx + dy * dy) >= minDistance)
            thinned ~= p;
    }
    return thinned;
}

Points sampleOriginalContour(Contour contour, float minDistance)
{
    return thinPoints(contour.pts, minDistance);
}

Points sampleExpandedContour(Contour contour, float expDist, float minDistance)
{
    Points pts;
    auto n = contour.pts.length;
    for (size_t i = 0; i < n; i++) {
        auto pPrev = contour.pts[(i + n - 1) % n];
        auto pCurr = contour.pts[i];
        auto pNext = contour.pts[(i + 1) % n];
        auto normal = calculateNormalVector(pPrev, pNext);
        pts ~= Point(pCurr.x - normal.x * expDist, pCurr.y - normal.y * expDist);
    }
    return thinPoints(pts, minDistance);
}

Points sampleExpandedFromThinned(Points thinnedPoints, float expDist)
{
    auto n = thinnedPoints.length;
    if (n < 3)
        return thinnedPoints;
    Points expanded;
    for (size_t i = 0; i < n; i++) {
        auto pPrev = thinnedPoints[(i + n - 1) % n];
        auto pNext = thinnedPoints[(i + 1) % n];
        auto normal = calculateNormalVector(pPrev, pNext);
        expanded ~= Point(thinnedPoints[i].x - normal.x * expDist,
                           thinnedPoints[i].y - normal.y * expDist);
    }
    return expanded;
}

Points sampleContractedFromThinned(Points thinnedPoints, float contDist, float factor = 0.5)
{
    auto n = thinnedPoints.length;
    if (n < 3)
        return thinnedPoints;
    Points contracted;
    for (size_t i = 0; i < n; i++) {
        auto pPrev = thinnedPoints[(i + n - 1) % n];
        auto pNext = thinnedPoints[(i + 1) % n];
        auto normal = calculateNormalVector(pPrev, pNext);
        contracted ~= Point(thinnedPoints[i].x + normal.x * (contDist * factor),
                             thinnedPoints[i].y + normal.y * (contDist * factor));
    }
    return contracted;
}

Points sampleCentroidContractedContour(Contour contour, float scale, float minDistance)
{
    float sumX = 0, sumY = 0;
    foreach(p; contour.pts)
    {
        sumX += p.x;
        sumY += p.y;
    }
    float cx = sumX / contour.pts.length;
    float cy = sumY / contour.pts.length;
    Point centroid = Point(cx, cy);
    Points pts;
    foreach(p; contour.pts)
    {
        pts ~= Point(centroid.x + scale * (p.x - centroid.x),
                      centroid.y + scale * (p.y - centroid.y));
    }
    return thinPoints(pts, minDistance);
}

Contour[] extractContours(in UByteImage binaryImage)
{
    return cvFindContours(binaryImage, /*mode*/ 0, /*method*/ 0);
}

Triangle[] triangulate(UByteImage binaryImage, FloatMatrix vertices, int vertexCount)
{
    if (vertexCount < 3)
        return null;
    enum margin = 10;
    return cvSubdiv2DInsert(binaryImage, vertices, vertexCount, margin);
}

bool checkTriangleContainsWhitePixels(UByteImage binaryImage, Triangle triangle)
{
    UByteImage mask; // 仮実装：mask.data は1次元配列、同じ幅・高さを持つ
    cvFillPoly(mask, triangle, 255);
    UByteImage bitwiseResult;
    cvBitwiseAnd(binaryImage, mask, bitwiseResult);
    return true; // 仮実装
}

import std.typecons : Tuple, tuple;
Tuple!(uint, float, float, UByteImage, UByteImage) calcShapeInformation(UByteImage image)
{
    UByteImage gray;
    cvCvtColor(image, gray, 0);
    UByteImage binary;
    cvThreshold(gray, binary, 1, 255, 0);
    auto skeleton = extractSkeleton(binary);
    auto widthMap = calculateWidthMap(binary, skeleton);
    uint length = 0;
    foreach(val; skeleton.data)
        if(val > 0) length++;
    float sumWidth = 0;
    uint count = 0;
    foreach(val; widthMap)
    {
        if(val > 0) { sumWidth += val; count++; }
    }
    float avgWidth = count > 0 ? sumWidth / count : 0;
    float ratio = length > 0 ? avgWidth / length : 0;
    return tuple(length, avgWidth, ratio, skeleton, binary);
}

// flattenVertices: 1次元の float[]（サイズは2*vertexCount）から Point[] に変換
/// ヘルパー: 頂点集合（Points）を 1次元の float[] に変換したものから Point[] へ変換
Points flattenVertices(FloatMatrix vertices)
{
    // ここではすでに vertices は1次元の float[]（サイズは2*vertexCount）とする
    Points pts;
    enforce(vertices.length % 2 == 0, "頂点配列のサイズが不正です");
    for (size_t i = 0; i < vertices.length; i += 2)
        pts ~= Point(vertices[i], vertices[i + 1]);
    return pts;
}


// reshapeTriangles: 1次元の Point[] から Triangle[]（3点ずつ）に変換
Triangle[] reshapeTriangles(Points pts)
{
    enforce(pts.length % 3 == 0, "頂点数が三角形の数に整合しません");
    Triangle[] tris;
    for (size_t i = 0; i < pts.length; i += 3)
        tris ~= Triangle(pts[i], pts[i + 1], pts[i + 2]);
    return tris;
}

// pointsToFloatMatrix: Point[] から 1次元の float[] に変換
FloatMatrix pointsToFloatMatrix(Points pts)
{
    FloatMatrix result;
    foreach(p; pts)
    {
        result ~= p.x;
        result ~= p.y;
    }
    return result;
}

// completeUncoveredArea は仮実装（1次元の float[] と Triangle[] を返す）
Tuple!(FloatMatrix, Triangle[]) completeUncoveredArea(UByteImage binary, FloatMatrix finalVertices, Triangle[] triangles, Contour[] contours, float minDistance)
{
    return tuple(finalVertices, triangles);
}

// ---------------------------------------------------------------------
// 画像処理メイン処理（processImageRefactored 相当）
// ---------------------------------------------------------------------
Tuple!(UByteImage, FloatMatrix, Contour[], FloatMatrix, FloatMatrix, FloatMatrix, Triangle[])
processImageRefactored(UByteImage image,
                       float largeThreshold = 400,
                       float lengthThreshold = 100,
                       float ratioThreshold = 0.2,
                       float sharpExpansionFactor = 0.03,
                       float nonsharpExpansionFactor = 0.05,
                       float nonsharpContractionFactor = 0.05,
                       float[] pcaScales = [0.0, 0.5],
                       float minDistance = 10,
                       float sizeAvg = 100)
{
    UByteImage binary;
    if (image.data.length == image.width * image.height && image.width == 4)
        cvThreshold(image, binary, 1, 255, 0);
    else
    {
        UByteImage gray;
        cvCvtColor(image, gray, 0);
        cvThreshold(gray, binary, 1, 255, 0);
    }
    auto shapeInfo = calcShapeInformation(image);
    uint length = shapeInfo[0];
    float avgWidth = shapeInfo[1];
    float ratio = shapeInfo[2];
    auto skeleton = shapeInfo[3];
    auto contoursAll = extractContours(binary);
    bool sharpFlag = (avgWidth < largeThreshold) &&
                     ((length < lengthThreshold) || ((avgWidth / length) < ratioThreshold));
    FloatMatrix verticesList;
    int vertexCount = 0;
    if (sharpFlag) {
        foreach(contour; contoursAll) {
            auto ptsB2 = sampleExpandedContour(contour, sizeAvg * sharpExpansionFactor, minDistance);
            foreach(p; ptsB2) {
                verticesList ~= p.x;
                verticesList ~= p.y;
                vertexCount++;
            }
        }
    }
    else
    {
        foreach(contour; contoursAll)
        {
            auto ptsB1 = sampleOriginalContour(contour, minDistance);
            foreach(p; ptsB1)
            {
                verticesList ~= p.x;
                verticesList ~= p.y;
                vertexCount++;
            }
            auto ptsB2 = sampleExpandedFromThinned(ptsB1, max(sizeAvg * nonsharpExpansionFactor, 16));
            foreach(p; ptsB2)
            {
                verticesList ~= p.x;
                verticesList ~= p.y;
                vertexCount++;
            }
            auto ptsB3 = sampleContractedFromThinned(ptsB1, max(sizeAvg * nonsharpContractionFactor, 16), 0.5);
            foreach(p; ptsB3)
            {
                verticesList ~= p.x;
                verticesList ~= p.y;
                vertexCount++;
            }
            foreach(scale; pcaScales)
            {
                auto ptsCentroid = sampleCentroidContractedContour(contour, scale, minDistance);
                foreach(p; ptsCentroid)
                {
                    verticesList ~= p.x;
                    verticesList ~= p.y;
                    vertexCount++;
                }
            }
        }
    }
    if (verticesList.length == 0)
        return tuple(skeleton, cast(FloatMatrix)null, contoursAll, cast(FloatMatrix)null, cast(FloatMatrix)null, cast(FloatMatrix)null, cast(Triangle[])null);
    auto triangles = triangulate(binary, verticesList, vertexCount);
    Triangle[] validTriangles;
    if (triangles !is null)
    {
        foreach(tri; triangles)
        {
            if (checkTriangleContainsWhitePixels(binary, tri))
                validTriangles ~= tri;
        }
    }
    return tuple(skeleton, verticesList, contoursAll, cast(FloatMatrix)null, cast(FloatMatrix)null, cast(FloatMatrix)null, validTriangles);
}

// flattenTriangles: Point[] から Triangle[] に変換（1次元配列から生成）
Points flattenTriangles(Triangle[] triangles)
{
    Points pts;
    foreach(tri; triangles)
    {
        pts ~= tri.a;
        pts ~= tri.b;
        pts ~= tri.c;
    }
    return pts;
}

// ---------------------------------------------------------------------
// エントリ関数 automesh
// ---------------------------------------------------------------------
struct MeshResult {
    UByteImage image;
    UByteImage skeleton;
    FloatMatrix vertices; // 1次元の float[]（各頂点は [x, y] の連結配列）
    Triangle[] triangles;
    Contour[] contours;
    FloatMatrix expanded;
    FloatMatrix contracted;
    FloatMatrix reduced;
}

MeshResult[] automesh(UByteImage[] textures,
                      float largeThreshold = 400,
                      float lengthThreshold = 100,
                      float ratioThreshold = 0.2,
                      float sharpExpansionFactor = 0.03,
                      float nonsharpExpansionFactor = 0.05,
                      float nonsharpContractionFactor = 0.05,
                      float[] pcaScales = [0.0, 0.5])
{
    MeshResult[] results;
    MeshResult[] pastResults;
    foreach(i, tex; textures)
    {
        if (i == 0)
            continue; // 最初の画像は対象外
        enforce(tex.data.length > 0, "入力画像が見つかりません。");
        int w = tex.width;
        float sizeAvg = (w + tex.height) / 2.0f;
        float minDistance = max(w / 10, 10);
        auto proc = processImageRefactored(tex,
                     largeThreshold, lengthThreshold, ratioThreshold,
                     sharpExpansionFactor, nonsharpExpansionFactor, nonsharpContractionFactor,
                     pcaScales, minDistance, sizeAvg);
        UByteImage skeleton = proc[0];
        FloatMatrix vertices = proc[1];
        Contour[] contours = proc[2];
        FloatMatrix expanded = proc[3];
        FloatMatrix contracted = proc[4];
        FloatMatrix reduced = proc[5];
        Triangle[] triangles = proc[6];
        bool useMirror = false;
        foreach(past; pastResults)
        {
            if (areImagesLeftRightSymmetric(tex, past.image, 20))
            {
                // mirrorPoints を利用して、vertices（1次元配列）の各頂点を再構築
                auto pts = flattenVertices(vertices);
                pts = mirrorPoints(pts, w);
                // 再度1次元の float[] に変換
                FloatMatrix newVertices = pointsToFloatMatrix(pts);
                vertices = newVertices;
                if (past.triangles.length > 0)
                {
                    auto pastPts = pointsToFloatMatrix(flattenTriangles(past.triangles));
                    auto mirroredPts = mirrorPoints(flattenVertices(pastPts), w);
                    triangles = reshapeTriangles(mirroredPts);
                }
                else
                {
                    triangles = null;
                }
                UByteImage gray;
                cvCvtColor(tex, gray, 0);
                cvThreshold(gray, skeleton, 1, 255, 0);
                skeleton = extractSkeleton(skeleton);
                useMirror = true;
                break;
            }
        }
        if (!useMirror)
        {
            auto comp = completeUncoveredArea(tex, vertices, triangles, contours, minDistance);
            vertices = comp[0];
            triangles = comp[1];
        }
        MeshResult res;
        res.image = tex;
        res.skeleton = skeleton;
        res.vertices = vertices;
        res.triangles = triangles;
        res.contours = contours;
        res.expanded = expanded;
        res.contracted = contracted;
        res.reduced = reduced;
        results ~= res;
        pastResults ~= res;
    }
    return results;
}
