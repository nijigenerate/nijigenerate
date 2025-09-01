module nijigenerate.core.math.skeletonize;

import mir.ndslice;
import inmath;
import std.algorithm;
import std.array;
import std.algorithm.iteration: uniq;
import core.thread.fiber;


/////////////////////////////////////////////////////////////
// 2. Zhang-Suen 細線化アルゴリズム（NDslice を用いる）
void skeletonizeImage(T)(T imbin) {

    int countNeighbors(T)(T image, int i, int j) {
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
    int countTransitions(T)(T image, int i, int j) {
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


    // 画像サイズのローカル変数化
    ulong h = imbin.shape[0];
    ulong w = imbin.shape[1];

    // 初期候補セット: 内部領域（境界を除く）の前景画素全て
    vec2u[] candidates;
    for (int i = 1; i < h - 1; i++) {
        for (int j = 1; j < w - 1; j++) {
            if (imbin[i, j])
                candidates ~= vec2u(j, i);  // vec2u(x, y)
        }
    }
    
    bool changedOverall = true;
    
    // ループ：候補セットが空になるか、変更がなくなるまで
    while (changedOverall) {
        changedOverall = false;
        
        // --- サブイテレーション 1 ---
        vec2u[] toDelete;
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
        
        vec2u[] newCandidates;
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
                        newCandidates ~= vec2u(nj, ni);
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
        vec2u[] toDelete2;
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
        
        vec2u[] newCandidates2;
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
                        newCandidates2 ~= vec2u(nj, ni);
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
                        candidates ~= vec2u(j, i);
                }
            }
            // 変更がなければアルゴリズム終了
            if (candidates.length == 0)
                break;
        }
        Fiber.yield();
    }
}