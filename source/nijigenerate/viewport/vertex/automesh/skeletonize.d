module nijigenerate.viewport.vertex.automesh.skeletonize;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import nijigenerate.core.math.skeletonize;
import nijigenerate.core.math.path;
import inmath;
import mir.ndslice;
import mir.ndslice.slice;    // NDslice 用の slice 操作
import mir.ndslice.topology; // reshape() などのため
import mir.math.stat: mean;
import nijigenerate.core.cv;
import std.algorithm;
//import std.stdio;
import std.math: abs;
import std.conv;
import std.array;
import std.algorithm.iteration: uniq;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import nijigenerate.core.cv.image;
import nijigenerate.viewport.vertex.automesh.common : getAlphaInput, mapImageCenteredMeshToTargetLocal;

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
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;

public:
    override IncMesh autoMesh(Drawable target, IncMesh mesh,
                              bool mirrorHoriz = false, float axisHoriz = 0,
                              bool mirrorVert = false, float axisVert = 0)
    {
        // 1) 最初の処理のみ分岐: AlphaInput の取得
        auto ai = getAlphaInput(target);
        if (ai.w <= 0 || ai.h <= 0 || ai.img is null) return mesh;

        // 2) 共通処理: 二値化
        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3].dup;
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        // 3) 共通処理: 細線化 → パス抽出 → 単純化
        skeletonizeImage(imbin);
        auto path = extractPath(imbin, ai.w, ai.h);
        controlPoints = simplifyByTargetCount(path, targetPointCount);

        // 4) 共通処理: 画像中心基準で頂点生成
        mesh.clear();
        vec2 imgCenter = vec2(ai.w / 2, ai.h / 2);
        foreach (Point pt; controlPoints) {
            vec2 position = vec2(pt.x, pt.y);
            mesh.vertices ~= new MeshVertex(position - imgCenter);
        }

        // 5) 共通処理: 対象ローカルへマッピング
        mapImageCenteredMeshToTargetLocal(mesh, target, ai);
        return mesh.autoTriangulate();
    }

    override void configure() {
        igSeparator();
        incText(_("Alpha Preview"));
        igIndent();
        alphaPreviewWidget(_alphaPreview, ImVec2(192, 192));
        igUnindent();
    }

    /// 抽出された制御点の取得
    Point[] getControlPoints() {
        return controlPoints;
    }

private:

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
