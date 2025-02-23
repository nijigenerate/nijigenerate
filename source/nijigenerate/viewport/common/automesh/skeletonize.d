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
    int targetPointCount = 10;
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
        vec2 imgCenter = vec2(texture.width / 2, texture.height / 2);

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

        // 曲線単純化
        controlPoints = simplifyByTargetCount(path, targetPointCount);

        mesh.clear();
        foreach (Point pt; controlPoints) {
            vec2 position = vec2(pt.x, pt.y);
            mesh.vertices ~= new MeshVertex(position - imgCenter);
        }

        if (auto dcomposite = cast(DynamicComposite)target) {
            foreach (vertex; mesh.vertices) {
                vertex.position += dcomposite.textureOffset;
            }
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
        import std.algorithm : sort;
        import std.algorithm.iteration: uniq;
        import std.array : array;
        
        // 画像サイズのローカル変数化
        ulong h = imbin.shape[0];
        ulong w = imbin.shape[1];

        // 初期候補セット: 内部領域（境界を除く）の前景画素全て
        Point[] candidates;
        for (int i = 1; i < h - 1; i++) {
            for (int j = 1; j < w - 1; j++) {
                if (imbin[i, j])
                    candidates ~= Point(j, i);  // Point(x, y)
            }
        }
        
        bool changedOverall = true;
        
        // ループ：候補セットが空になるか、変更がなくなるまで
        while (changedOverall) {
            changedOverall = false;
            
            // --- サブイテレーション 1 ---
            Point[] toDelete;
            foreach (pt; candidates) {
                int i = pt.y;
                int j = pt.x;
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
                // 条件を満たす場合は削除対象に追加
                toDelete ~= pt;
            }
            
            Point[] newCandidates;
            if (toDelete.length > 0) {
                changedOverall = true;
                // 削除対象の画素を0に設定し、その近傍を新たな候補に追加
                foreach (pt; toDelete) {
                    int i = pt.y;
                    int j = pt.x;
                    imbin[i, j] = 0;
                    // 近傍 (8方向) を候補に追加（重複は後で除去）
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            int ni = i + di;
                            int nj = j + dj;
                            // 範囲チェック：境界は除外
                            if (ni < 1 || ni >= h - 1 || nj < 1 || nj >= w - 1)
                                continue;
                            newCandidates ~= Point(nj, ni);
                        }
                    }
                }
                // 重複除去：y座標、x座標でソートしてユニークな候補に
                newCandidates.sort!((a, b) =>
                    (a.y < b.y) || (a.y == b.y && a.x < b.x)
                );
                newCandidates = newCandidates.uniq.array;
            }
            
            // --- サブイテレーション 2 ---
            Point[] toDelete2;
            foreach (pt; newCandidates) {
                int i = pt.y;
                int j = pt.x;
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
                toDelete2 ~= pt;
            }
            
            Point[] newCandidates2;
            if (toDelete2.length > 0) {
                changedOverall = true;
                foreach (pt; toDelete2) {
                    int i = pt.y;
                    int j = pt.x;
                    imbin[i, j] = 0;
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            int ni = i + di;
                            int nj = j + dj;
                            if (ni < 1 || ni >= h - 1 || nj < 1 || nj >= w - 1)
                                continue;
                            newCandidates2 ~= Point(nj, ni);
                        }
                    }
                }
                newCandidates2.sort!((a, b) =>
                    (a.y < b.y) || (a.y == b.y && a.x < b.x)
                );
                newCandidates2 = newCandidates2.uniq.array;
            }
            
            // 次回の候補は、サブイテレーション2で更新された候補集合
            candidates = newCandidates2;
            // 候補が空の場合、全画素を再スキャンして前景が残っていれば候補集合に追加
            if (candidates.length == 0) {
                for (int i = 1; i < h - 1; i++) {
                    for (int j = 1; j < w - 1; j++) {
                        if (imbin[i, j])
                            candidates ~= Point(j, i);
                    }
                }
                // 変更がなければアルゴリズム終了
                if (candidates.length == 0)
                    break;
            }
        }
    }


    /// 8近傍（上下左右・斜め）の前景画素数を返す
    int countNeighbors(T)(T image, int i, int j)
    {
        int count = 0;
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
    // 修正: 複数の連結成分それぞれについて、直径（最も長いパス）を求め、
    //       全体で最も長いパスを返すように変更
    Point[] extractPath(T)(T skeleton, int width, int height)
    {
        if (skeleton.length == 0) return [];
        // 連結成分の管理用 visited 配列（グローバル）
        ubyte[] globalVisitedData = new ubyte[height * width];
        int err;
        auto globalVisited = globalVisitedData.sliced.reshape([height, width], err);
        Point[] longestPathOverall;

        // 座標を 1 次元インデックスに変換するヘルパー関数
        int encode(Point p) {
            return p.y * width + p.x;
        }

        // 与えた点から連結成分（このコンポーネント内の全画素）を BFS で収集する
        Point[] getComponent(Point p) {
            Point[] comp;
            Point[] queue;
            queue ~= p;
            globalVisited[p.y, p.x] = 1;
            while(queue.length)
            {
                auto cur = queue[0];
                queue = queue[1 .. $];
                comp ~= cur;
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ny = cur.y + di;
                        int nx = cur.x + dj;
                        if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                            continue;
                        if (skeleton[ny, nx] && !globalVisited[ny, nx]) {
                            globalVisited[ny, nx] = 1;
                            queue ~= Point(nx, ny);
                        }
                    }
                }
            }
            return comp;
        }

        // comp 配列内に pt が含まれているかを調べる（線形探索）
        bool inComponent(Point pt, Point[] comp) {
            foreach (cpt; comp) {
                if (cpt.x == pt.x && cpt.y == pt.y)
                    return true;
            }
            return false;
        }

        // comp 内で pt の 8近傍にある画素を返す
        Point[] getNeighbors(Point pt, Point[] comp) {
            Point[] neighbors;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if (di == 0 && dj == 0) continue;
                    Point np = Point(pt.x + dj, pt.y + di);
                    if (inComponent(np, comp))
                        neighbors ~= np;
                }
            }
            return neighbors;
        }

        import std.typecons : Tuple, tuple;
        import std.array : array;
        // BFS を用いて、コンポーネント内の src からの最遠点とその経路（src～最遠点）を求める
        Tuple!(Point, Point[]) bfsDiameter(Point src, Point[] comp) {
            Point[] queue;
            int[] dist = new int[height * width];
            foreach (i; 0 .. dist.length)
                dist[i] = -1;
            // 各点の直前の点を記録するための連想配列（キーは encode した値）
            Point[int] pred;
            queue ~= src;
            dist[encode(src)] = 0;
            while(queue.length) {
                auto cur = queue[0];
                queue = queue[1 .. $];
                foreach (n; getNeighbors(cur, comp)) {
                    if (dist[encode(n)] == -1) {
                        dist[encode(n)] = dist[encode(cur)] + 1;
                        pred[encode(n)] = cur;
                        queue ~= n;
                    }
                }
            }
            int maxDist = 0;
            Point farthest = src;
            foreach (pt; comp) {
                int d = dist[encode(pt)];
                if (d > maxDist) {
                    maxDist = d;
                    farthest = pt;
                }
            }
            // 最遠点から src までの経路を復元（逆順に得られるので reverse する）
            Point[] pathReversed;
            auto cur = farthest;
            while (true) {
                pathReversed ~= cur;
                if (cur.x == src.x && cur.y == src.y)
                    break;
                cur = pred[encode(cur)];
            }
            auto path = pathReversed.reverse.array;
            return tuple(farthest, path);
        }

        // skeleton 全体を走査し、各連結成分ごとに直径を求め、最も長いパスを記録する
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                if (skeleton[i, j] && !globalVisited[i, j]) {
                    // 新たな連結成分を取得
                    auto comp = getComponent(Point(j, i));
                    // 端点（近傍が1個）の探索
                    Point[] endpoints;
                    foreach (pt; comp) {
                        auto nbrs = getNeighbors(pt, comp);
                        if (nbrs.length == 1)
                            endpoints ~= pt;
                    }
                    // 端点が存在すればそのうちの1つ、なければ comp の最初の点を直径計算の起点とする
                    Point startForDiameter = (endpoints.length > 0) ? endpoints[0] : comp[0];
                    // 2回の BFS により、コンポーネント内の直径（最長経路）を求める
                    auto result1 = bfsDiameter(startForDiameter, comp);
                    auto result2 = bfsDiameter(result1[0], comp);
                    if (result2[1].length > longestPathOverall.length)
                        longestPathOverall = result2[1];
                }
            }
        }
        return longestPathOverall;
    }

    /////////////////////////////////////////////////////////////
    // 4. Visvalingam–Whyatt 法に基づく、指定したポイント数に単純化するアルゴリズム
    Point[] simplifyByTargetCount(Point[] pts, int targetCount) {
        // pts の数が既に目標以下ならそのまま返す
        if (pts.length <= targetCount)
            return pts.dup;

        // 内部点（先頭と末尾は保持）を順次削除して目標数にする
        auto simplified = pts.dup;

        // ヘルパー：3点 (a, b, c) から三角形の面積を計算
        auto triangleArea = (Point a, Point b, Point c) {
            double ax = cast(double)a.x, ay = cast(double)a.y;
            double bx = cast(double)b.x, by = cast(double)b.y;
            double cx = cast(double)c.x, cy = cast(double)c.y;
            double area = abs(ax*(by - cy) + bx*(cy - ay) + cx*(ay - by));
            return area / 2.0;
        };

        while (simplified.length > targetCount) {
            int removeIndex = 1;
            double minArea = triangleArea(simplified[0], simplified[1], simplified[2]);
            // 内部点（先頭と末尾は除く）について、三角形の面積が最小となる点を探索
            for (int i = 2; i < simplified.length - 1; i++) {
                double area = triangleArea(simplified[i - 1], simplified[i], simplified[i + 1]);
                if (area < minArea) {
                    minArea = area;
                    removeIndex = i;
                }
            }
            // 最小の内部点を削除
            simplified = simplified[0 .. removeIndex] ~ simplified[removeIndex + 1 .. $];
        }
        return simplified;
    }

public:
    override
    string icon() {
        return "\uebbb";
    }
}