module nijigenerate.core.cv.contours;

import std.array;
import std.algorithm;
import std.range;
import std.conv;
import std.typecons;
import std.math;
import std.algorithm;
import mir.rc.array;    // mir.rc.array を利用
import inmath;         // vec2i は inmath モジュールに存在する
import core.thread.fiber;

/****************************************************
 * D言語による OpenCV cv::findContours に等価な実装（改善版）
 *
 * 主な改善点:
 * 1. メモリアロケーションの削減: 動的配列のリザーブや再利用
 * 2. ループ内演算の最適化: 8方向の探索で方向マッピングテーブルを使用
 * 3. 内部領域塗りつぶしの高速化: スキャンライン法による塗りつぶしに変更
 * 4. 階層構築処理の効率化: 各輪郭のバウンディングボックスを事前計算して内包判定の負荷を削減
 ****************************************************/

/// 輪郭抽出のモード
enum RetrievalMode {
    EXTERNAL,  // 外側の輪郭のみ
    LIST,      // 全輪郭（階層無視）
    CCOMP,     // 2層階層（外側と穴）
    TREE       // 完全な階層構造
}

/// 輪郭近似の手法
enum ApproximationMethod {
    NONE,      // 全点保持
    SIMPLE,    // 単純な直線冗長点削減
    TC89_L1,   // Douglas‐Peucker法（TC89_L1相当）
    TC89_KCOS  // Douglas‐Peucker法（TC89_KCOS相当）
}

/// 階層情報（OpenCV と同等の形式）
struct ContourHierarchy {
    int next;   // 同レベル次の輪郭
    int prev;   // 同レベル前の輪郭
    int child;  // 最初の子輪郭
    int parent; // 親輪郭
};

/// 簡易なバウンディングボックス構造体
struct BoundingBox {
    int minX, minY, maxX, maxY;
};

/// 画像内の座標が有効か判定
bool inBounds(int x, int y, int width, int height) {
    return (x >= 0 && x < width && y >= 0 && y < height);
}

/// 8方向（右手法用）のオフセット（時計回り順）
/// [ 0: 東, 1: 北東, 2: 北, 3: 北西, 4: 西, 5: 南西, 6: 南, 7: 南東 ]
static immutable int[2][8] DIRECTIONS = [
    [0, 1],
    [-1, 1],
    [-1, 0],
    [-1, -1],
    [0, -1],
    [1, -1],
    [1, 0],
    [1, 1]
];

/// 8方向の (dx,dy) から対応するインデックスを即時に得るためのマッピングテーブル
/// インデックス: [dy+1][dx+1] （中心は無効: -1）
enum int[3][3] DIR_MAP = [
    [ 3, 4, 5 ],  // dy = -1, dx = -1,0,1
    [ 2, -1, 6 ], // dy =  0, dx = -1,0,1 (中心は無効)
    [ 1, 0, 7 ]   // dy =  1, dx = -1,0,1
];

/// Suzuki‐Abe法（右手法）による輪郭追跡
/// 改善点: 
/// - contour 配列に対してあらかじめリザーブを行い、動的再確保を削減
/// - 方向判定に DIR_MAP を使用してループ回数を削減
vec2i[] suzukiAbeContour(T)(in T image, ref int[][] labels, int label, int startX, int startY, int width, int height) {
    vec2i b = vec2i(startX, startY);
    vec2i c = b;
    // 初期 p は開始点の上方向
    vec2i p = vec2i(startX, startY - 1);
    vec2i[] contour;
    // 初期リザーブ（概ねの見積もり）
    contour.reserve(256);
    contour ~= b;
    labels[startY][startX] = label;
    bool firstIteration = true;

    while (true) {
        // ループ内で p と c の相対位置(dx,dy)を即時テーブル参照
        int dx = cast(int)p.x - cast(int)c.x;
        int dy = cast(int)p.y - cast(int)c.y;
        int startDir = 0;
        // テーブルから取得。中心(0,0)の場合は -1となるので、その場合は 0 を採用
        int tableVal = (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) ? DIR_MAP[dy + 1][dx + 1] : -1;
        if(tableVal != -1)
            startDir = (tableVal + 1) % 8;
        else
            startDir = 0;

        bool found = false;
        vec2i next;
        int nextDir = startDir;
        // 8方向の探索
        for (size_t i = 0; i < 8; i++) {
            int idx = (startDir + i) % 8;
            int nx = c.x + DIRECTIONS[idx][0];
            int ny = c.y + DIRECTIONS[idx][1];
            
            if (!inBounds(nx, ny, width, height))
                continue;
            // 内部領域(-1)は再訪問対象外、また既処理画素は現在の輪郭番号も有効
            if (image[ny, nx] != 0 && (labels[ny][nx] == 0 || labels[ny][nx] == label || (nx == b.x && ny == b.y && !firstIteration))) {
                next = vec2i(nx, ny);
                nextDir = idx;
                found = true;
                break;
            }
        }
        if (!found)
            break;

        // p を更新：次の候補位置は (nextDir + 7) mod 8 の方向にずらす
        {
            int pdx = DIRECTIONS[(nextDir + 7) % 8][0];
            int pdy = DIRECTIONS[(nextDir + 7) % 8][1];
            p = vec2i(c.x + pdx, c.y + pdy);
        }
        c = next;

        // 再訪の場合はラベル更新を除外
        if (!(c.x == b.x && c.y == b.y))
            labels[c.y][c.x] = label;

        contour ~= c;

        if (!firstIteration && c.x == b.x && c.y == b.y)
            break;

        firstIteration = false;
    }

    return contour;
}

/// スキャンライン法による輪郭内部塗りつぶし
/// 改善点: バウンディングボックス内各画素に対し pointInPolygon を呼ぶのではなく、
/// 各走査線で輪郭との交点を求め、その間を一括で塗りつぶす。
void fillContourInterior(ref int[][] labels, vec2i[] contour, int width, int height) {
    // バウンディングボックス計算
    int minX = contour[0].x, maxX = contour[0].x;
    int minY = contour[0].y, maxY = contour[0].y;
    foreach (pt; contour) {
        minX = min(minX, pt.x);
        maxX = max(maxX, pt.x);
        minY = min(minY, pt.y);
        maxY = max(maxY, pt.y);
    }
    // 範囲外は考慮せず、バウンディングボックス内で走査
    for (int y = minY; y <= maxY; y++) {
        double[] inters;
        // 輪郭の各エッジについて、走査線 y との交点を求める
        for (size_t i = 0; i < contour.length; i++) {
            vec2i a = contour[i];
            vec2i b = contour[(i + 1) % contour.length];
            // エッジが走査線を跨いでいるか判定
            if ((a.y <= y && b.y > y) || (a.y > y && b.y <= y)) {
                // 交点の x 座標（線形補間）
                double atX = a.x + (cast(double)(y - a.y) / (b.y - a.y)) * (b.x - a.x);
                inters ~= atX;
            }
        }
        // 交点が2個以上の場合、ソートしてペアで塗りつぶす
        if (inters.length >= 2) {
            inters.sort();
            // ペアごとに塗りつぶす
            for (size_t i = 0; i < inters.length - 1; i += 2) {
                int start = cast(int)ceil(inters[i]);
                int end   = cast(int)floor(inters[i + 1]);
                // バウンディングボックス内かつ未処理の画素を塗りつぶす
                for (int x = start; x <= end; x++) {
                    if (inBounds(x, y, width, height) && labels[y][x] == 0)
                        labels[y][x] = -1;
                }
            }
        }
    }
}

/// 標準的なレイキャスティング法による点 p の多角形 polygon 内外判定（補助的に利用）
bool pointInPolygon(vec2i p, vec2i[] polygon) {
    size_t n = polygon.length;
    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        vec2i a = polygon[i];
        vec2i b = polygon[(i + 1) % n];
        if ((a.y > p.y) != (b.y > p.y)) {
            double atX = a.x + (cast(double)(p.y - a.y) / (b.y - a.y)) * (b.x - a.x);
            if (p.x < atX)
                cnt++;
        }
    }
    return (cnt & 1) == 1;
}

/// 複数輪郭間の内包関係を調べ、階層構造を構築する
/// 改善点: 各輪郭のバウンディングボックスを事前に計算し、内包判定の前処理として利用する
void buildHierarchy(vec2i[][] contours, out ContourHierarchy[] hier) {
    size_t n = contours.length;
    hier.length = n;
    foreach (ref h; hier) {
        h.next = -1;
        h.prev = -1;
        h.child = -1;
        h.parent = -1;
    }
    
    // 各輪郭のバウンディングボックスを計算
    BoundingBox[] boxes;
    boxes.length = n;
    foreach (i, contour; contours) {
        int minX = contour[0].x, maxX = contour[0].x;
        int minY = contour[0].y, maxY = contour[0].y;
        foreach (pt; contour) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }
        boxes[i] = BoundingBox(minX, minY, maxX, maxY);
    }
    
    for (size_t i = 0; i < n; i++) {
        vec2i rep = contours[i][0];
        int parentIndex = -1;
        // まず自輪郭のバウンディングボックス
        auto bb = boxes[i];
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            // rep が j のバウンディングボックス内にあるか確認
            auto bbj = boxes[j];
            if (rep.x < bbj.minX || rep.x > bbj.maxX || rep.y < bbj.minY || rep.y > bbj.maxY)
                continue;
            // rep が実際に多角形内部にあるか判定
            if (pointInPolygon(rep, contours[j])) {
                // 輪郭の点数で親候補を絞る（面積感触として利用）
                if (parentIndex == -1 || contours[j].length < contours[parentIndex].length)
                    parentIndex = cast(int)j;
            }
        }
        hier[i].parent = parentIndex;
        if (parentIndex != -1) {
            if (hier[parentIndex].child == -1)
                hier[parentIndex].child = cast(int)i;
            else {
                int sib = hier[parentIndex].child;
                while (hier[sib].next != -1)
                    sib = hier[sib].next;
                hier[sib].next = cast(int)i;
                hier[i].prev = sib;
            }
        }
    }
}

/// Douglas-Peucker 法による輪郭近似
vec2i[] ramerDouglasPeucker(vec2i[] points, double epsilon) {
    if (points.length < 3)
        return points.dup;
    double maxDist = 0;
    int index = 0;
    for (size_t i = 1; i < points.length - 1; i++) {
        double d = distanceToSegment(points[i], points[0], points[$ - 1]);
        if (d > maxDist) {
            maxDist = d;
            index = cast(int)i;
        }
    }
    if (maxDist > epsilon) {
        auto left = ramerDouglasPeucker(points[0 .. index + 1], epsilon);
        auto right = ramerDouglasPeucker(points[index .. $], epsilon);
        return left ~ right[1 .. $];
    } else {
        return [points[0], points[$ - 1]];
    }
}

/// 点 p と線分 (p1, p2) との距離を計算
double distanceToSegment(vec2i p, vec2i p1, vec2i p2) {
    double A = p.x - p1.x;
    double B = p.y - p1.y;
    double C = p2.x - p1.x;
    double D = p2.y - p1.y;
    double dot = A * C + B * D;
    double lenSq = C * C + D * D;
    double param = (lenSq != 0) ? dot / lenSq : -1;
    double xx, yy;
    if (param < 0) {
        xx = p1.x;
        yy = p1.y;
    } else if (param > 1) {
        xx = p2.x;
        yy = p2.y;
    } else {
        xx = p1.x + param * C;
        yy = p1.y + param * D;
    }
    return sqrt((p.x - xx) * (p.x - xx) + (p.y - yy) * (p.y - yy));
}

/// 輪郭近似処理（各種手法に対応）
/// SIMPLE: 直線上の冗長点削減
/// TC89_L1 / TC89_KCOS: Douglas-Peucker 法を適用（ここでは epsilon を固定値 2.0 とする）
vec2i[] approximateContour(vec2i[] contour, ApproximationMethod method) {
    if (method == ApproximationMethod.NONE)
        return contour;
    else if (method == ApproximationMethod.SIMPLE) {
        if (contour.length < 3)
            return contour.dup;
        vec2i[] result;
        // 事前にリザーブ
        result.reserve(contour.length);
        result ~= contour[0];
        for (size_t i = 1; i < contour.length - 1; i++) {
            vec2i prev = contour[i - 1];
            vec2i curr = contour[i];
            vec2i next = contour[i + 1];
            if ((curr.x - prev.x) * (next.y - curr.y) != (curr.y - prev.y) * (next.x - curr.x))
                result ~= curr;
        }
        result ~= contour[$ - 1];
        return result;
    } else if (method == ApproximationMethod.TC89_L1 || method == ApproximationMethod.TC89_KCOS) {
        return ramerDouglasPeucker(contour, 2.0);
    }
    return contour;
}

/// findContours のメイン処理
/// binaryImage: mir.rc.array により作成された 3 次元配列 (アクセスは binaryImage[y, x] )
/// labels: 内部処理用ラベル（初期値 0:未処理）
void findContours(T)(in T binaryImage, out vec2i[][] contours, out ContourHierarchy[] hierarchyOut,
                     RetrievalMode mode = RetrievalMode.LIST,
                     ApproximationMethod method = ApproximationMethod.SIMPLE) {
    int height = cast(int)binaryImage.shape[0];
    int width  = cast(int)binaryImage.shape[1];
    
    // labels を事前に必要なサイズで確保
    int[][] labels;
    labels.length = height;
    foreach (ref row; labels)
        row.length = width;
    
    int label = 1;
    vec2i[][] localContours = [];
    ContourHierarchy[] hierarchyList;
    
    // 画像左上から走査（外側輪郭判定）
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (binaryImage[y, x] != 0 && labels[y][x] == 0) {
                int left = (x - 1 >= 0) ? binaryImage[y, x - 1] : 0;
                if (left == 0) {
                    auto contour = suzukiAbeContour(binaryImage, labels, label, x, y, width, height);
                    // 内部領域をスキャンライン法で埋める
                    fillContourInterior(labels, contour, width, height);
                    contour = approximateContour(contour, method);
                    localContours ~= contour;
                    label++;
                } else {
                    labels[y][x] = -1;
                }
            }
            Fiber.yield;
        }
    }
    contours = localContours;
    
    // 階層構築（TREE／CCOMP モードの場合）
    if (mode == RetrievalMode.TREE || mode == RetrievalMode.CCOMP) {
        buildHierarchy(contours, hierarchyList);
    } else {
        hierarchyList.length = contours.length;
        foreach (ref h; hierarchyList)
            h = ContourHierarchy(-1, -1, -1, -1);
    }
    
    // EXTERNAL モードの場合、親を持たない輪郭のみ返す
    if (mode == RetrievalMode.EXTERNAL) {
        vec2i[][] extContours;
        ContourHierarchy[] extHierarchy;
        for (size_t i = 0; i < contours.length; i++) {
            if (hierarchyList[i].parent == -1) {
                extContours ~= contours[i];
                extHierarchy ~= hierarchyList[i];
            }
        }
        contours = extContours;
        hierarchyList = extHierarchy;
    }
    hierarchyOut = hierarchyList;
}

unittest {
    /*************************************
     * サンプル: mir.rc.array を用いた二値画像生成
     * 画像サイズ: 5 x 8, チャンネル数: 1
     * アクセスは binaryImage[y, x] で行う
     *************************************/
    auto binaryImage = rcarray!int([
        [[0],[0],[0],[0],[0],[0],[0],[0]],
        [[0],[1],[1],[1],[0],[1],[1],[0]],
        [[0],[1],[0],[1],[0],[1],[1],[0]],
        [[0],[1],[1],[1],[0],[0],[0],[0]],
        [[0],[0],[0],[0],[0],[0],[0],[0]]
    ]);
    
    vec2i[][] contours;
    ContourHierarchy[] hierarchy;
    
    // RETR_TREE モードで階層構造、TC89_L1 で Douglas-Peucker 近似を適用
    findContours(binaryImage, contours, hierarchy, RetrievalMode.TREE, ApproximationMethod.TC89_L1);
    
    // 簡単なアサーション
    assert(contours.length > 0, "輪郭が検出されていません");
    assert(hierarchy.length == contours.length, "階層情報の数が輪郭数と一致していません");
    
    foreach (h; hierarchy) {
        assert(h.parent >= -1 && h.parent < cast(int)hierarchy.length, "不正な親インデックスがあります");
    }
}
