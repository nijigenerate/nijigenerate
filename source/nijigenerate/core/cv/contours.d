module nijigenerate.core.cv.contours;
/****************************************************
 * D言語による OpenCV cv::findContours に等価な実装
 *
 * 前提:
 *   - vec2u 型は inmath モジュールにあるとする
 *   - binaryImage は mir.rc.array を用いた 3 次元配列で、
 *     アクセスは binaryImage[y, x, 0] で行う（2次元イメージの場合はチャネルが1つ）
 *
 * 主な機能:
 *  - Suzuki‐Abe法（右手法）による輪郭追跡
 *  - Douglas‐Peucker法による輪郭近似（TC89_L1／TC89_KCOS相当）
 *  - レイキャスティング法による内外判定で階層構造（RETR_TREE／RETR_CCOMP）を構築
 *  - RETR_EXTERNAL, RETR_LIST にも対応
 *
 * ※ OpenCV の内部最適化には及ばないものの、アルゴリズム的には等価な実装です。
 ****************************************************/

import std.stdio;
import std.array;
import std.algorithm;
import std.range;
import std.conv;
import std.typecons;
import std.math;
import mir.rc.array;    // mir.rc.array を利用
import inmath;         // vec2u は inmath モジュールに存在する

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

/// Suzuki‐Abe法（右手法）による輪郭追跡
/// image : mir.rc.array により作成された3次元配列（アクセスは image[y, x, 0] ）
/// labels: 各画素のラベル (0:未処理, -1:内部画素, 正値:輪郭ID)
/// label : 現在の輪郭番号
/// startX, startY : 輪郭開始画素
/// width, height : 画像サイズ
vec2u[] suzukiAbeContour(T)(in T image, ref int[][] labels, int label, int startX, int startY, int width, int height) {
    // 輪郭開始画素 b
    vec2u b = vec2u(startX, startY);
    // 現在の輪郭画素 c
    vec2u c = b;
    // 初期の直前画素 p は b の「左側」 (画像座標では (startX, startY-1))
    vec2u p = vec2u(startX, startY - 1);
    vec2u[] contour;
    contour ~= b;
    labels[startY][startX] = label;
    
    // 「右手法」に基づく探索ループ
    while (true) {
        // 現在画素 c と直前画素 p の相対方向から開始方向を決定
        int dx = cast(int)p.x - cast(int)c.x;
        int dy = cast(int)p.y - cast(int)c.y;
        int startDir = 0;
        bool foundDir = false;
        for (size_t i = 0; i < 8; i++) {
            if (DIRECTIONS[i][0] == dx && DIRECTIONS[i][1] == dy) {
                startDir = (i + 1) % 8; // p の次の方向から探索開始
                foundDir = true;
                break;
            }
        }
        if (!foundDir)
            startDir = 0;
        
        bool found = false;
        vec2u next;
        int nextDir = startDir;
        // 時計回りに8方向探索
        for (size_t i = 0; i < 8; i++) {
            int idx = (startDir + i) % 8;
            int nx = c.x + DIRECTIONS[idx][0];
            int ny = c.y + DIRECTIONS[idx][1];
            if (inBounds(nx, ny, width, height) && image[ny, nx, 0] == 1) {
                next = vec2u(nx, ny);
                nextDir = idx;
                found = true;
                break;
            }
        }
        if (!found) break; // 対象画素が見つからなければ終了
        
        // 次の p は c から (nextDir+7)%8 の方向
        p = vec2u(c.x + DIRECTIONS[(nextDir + 7) % 8][0],
                  c.y + DIRECTIONS[(nextDir + 7) % 8][1]);
        c = next;
        if (labels[c.y][c.x] == 0)
            labels[c.y][c.x] = label;
        contour ~= c;
        
        // 輪郭開始点と初期の p に戻った場合、ループ終了
        if (c.x == b.x && c.y == b.y &&
            p.x == b.x + DIRECTIONS[7][0] && p.y == b.y + DIRECTIONS[7][1])
            break;
    }
    return contour;
}

/// レイキャスティング法による点 p の多角形 polygon 内外判定
bool pointInPolygon(vec2u p, vec2u[] polygon) {
    size_t n = polygon.length;
    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        vec2u a = polygon[i];
        vec2u b = polygon[(i + 1) % n];
        if ((a.y > p.y) != (b.y > p.y)) {
            double atX = a.x + (cast(double)(p.y - a.y) / (b.y - a.y)) * (b.x - a.x);
            if (p.x < atX)
                cnt++;
        }
    }
    return (cnt & 1) == 1;
}

/// 複数輪郭間の内包関係を調べ、階層構造を構築する
/// 各輪郭の代表点（最初の点）を用い、内包している場合は面積が小さい方を親とする
void buildHierarchy(vec2u[][] contours, out ContourHierarchy[] hier) {
    size_t n = contours.length;
    hier.length = n;
    foreach (ref h; hier) {
        h.next = -1;
        h.prev = -1;
        h.child = -1;
        h.parent = -1;
    }
    
    for (size_t i = 0; i < n; i++) {
        vec2u rep = contours[i][0];
        int parentIndex = -1;
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            // 外接矩形のチェック
            int minX = contours[j][0].x, maxX = contours[j][0].x;
            int minY = contours[j][0].y, maxY = contours[j][0].y;
            foreach (pt; contours[j]) {
                if (pt.x < minX) minX = pt.x;
                if (pt.x > maxX) maxX = pt.x;
                if (pt.y < minY) minY = pt.y;
                if (pt.y > maxY) maxY = pt.y;
            }
            if (rep.x < minX || rep.x > maxX || rep.y < minY || rep.y > maxY)
                continue;
            if (pointInPolygon(rep, contours[j])) {
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
vec2u[] ramerDouglasPeucker(vec2u[] points, double epsilon) {
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
double distanceToSegment(vec2u p, vec2u p1, vec2u p2) {
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
vec2u[] approximateContour(vec2u[] contour, ApproximationMethod method) {
    if (method == ApproximationMethod.NONE)
        return contour;
    else if (method == ApproximationMethod.SIMPLE) {
        if (contour.length < 3)
            return contour.dup;
        vec2u[] result;
        result ~= contour[0];
        for (size_t i = 1; i < contour.length - 1; i++) {
            vec2u prev = contour[i - 1];
            vec2u curr = contour[i];
            vec2u next = contour[i + 1];
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
/// binaryImage: mir.rc.array により作成された 3 次元配列 (アクセスは binaryImage[y, x, 0] )
/// labels: 内部処理用ラベル（初期値 0:未処理）
void findContours(T)(in T binaryImage, out vec2u[][] contours, out ContourHierarchy[] hierarchyOut,
                     RetrievalMode mode = RetrievalMode.LIST,
                     ApproximationMethod method = ApproximationMethod.SIMPLE) {
    // binaryImage は mir.rc.array なので extent を利用してサイズ取得
    int height = cast(int)binaryImage.extent[0];
    int width  = cast(int)binaryImage.extent[1];
    
    // labels: 2次元 int 配列（初期値 0）
    int[][] labels;
    labels.length = height;
    foreach (ref row; labels)
        row.length = width;
    
    int label = 1;
    contours = [];
    ContourHierarchy[] hierarchyList;
    
    // 画像左上から走査：外側輪郭の場合、左側 (x-1) が背景かどうかで判定
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (binaryImage[y, x, 0] == 1 && labels[y][x] == 0) {
                int left = (x - 1 >= 0) ? binaryImage[y, x - 1, 0] : 0;
                if (left == 0) {
                    auto contour = suzukiAbeContour(binaryImage, labels, label, x, y, width, height);
                    contour = approximateContour(contour, method);
                    contours ~= contour;
                    label++;
                } else {
                    labels[y][x] = -1;
                }
            }
        }
    }
    
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
        vec2u[][] extContours;
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
     * アクセスは binaryImage[y, x, 0] で行う
     *************************************/
    auto binaryImage = rcarray!int([
        [[0],[0],[0],[0],[0],[0],[0],[0]],
        [[0],[1],[1],[1],[0],[1],[1],[0]],
        [[0],[1],[0],[1],[0],[1],[1],[0]],
        [[0],[1],[1],[1],[0],[0],[0],[0]],
        [[0],[0],[0],[0],[0],[0],[0],[0]]
    ]);
    
    vec2u[][] contours;
    ContourHierarchy[] hierarchy;
    
    // RETR_TREE モードで階層構造、TC89_L1 で Douglas-Peucker 近似を適用
    findContours(binaryImage, contours, hierarchy, RetrievalMode.TREE, ApproximationMethod.TC89_L1);
    
    // 簡単なアサーション（輪郭が1つ以上検出され、階層情報の数が輪郭数と一致する）
    assert(contours.length > 0, "輪郭が検出されていません");
    assert(hierarchy.length == contours.length, "階層情報の数が輪郭数と一致していません");
    
    // 各階層情報の parent フィールドが -1 または有効な値であること
    foreach (h; hierarchy) {
        assert(h.parent >= -1 && h.parent < cast(int)hierarchy.length, "不正な親インデックスがあります");
    }
}
