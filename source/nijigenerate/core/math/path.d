module nijigenerate.core.math.path;

import inmath;
import mir.ndslice;
import std.algorithm;

/////////////////////////////////////////////////////////////
// 3. スケルトン上の連続パスを DFS で抽出する（visited も NDslice で管理）
// 修正: 複数の連結成分それぞれについて、直径（最も長いパス）を求め、
//       全体で最も長いパスを返すように変更
vec2u[] extractPath(T)(T skeleton, int width, int height)
{
    if (skeleton.length == 0) return [];
    // 連結成分の管理用 visited 配列（グローバル）
    ubyte[] globalVisitedData = new ubyte[height * width];
    int err;
    auto globalVisited = globalVisitedData.sliced.reshape([height, width], err);
    vec2u[] longestPathOverall;

    // 座標を 1 次元インデックスに変換するヘルパー関数
    int encode(vec2u p) {
        return p.y * width + p.x;
    }

    // 与えた点から連結成分（このコンポーネント内の全画素）を BFS で収集する
    vec2u[] getComponent(vec2u p) {
        vec2u[] comp;
        vec2u[] queue;
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
                        queue ~= vec2u(nx, ny);
                    }
                }
            }
        }
        return comp;
    }

    // comp 配列内に pt が含まれているかを調べる（線形探索）
    bool inComponent(vec2u pt, vec2u[] comp) {
        foreach (cpt; comp) {
            if (cpt.x == pt.x && cpt.y == pt.y)
                return true;
        }
        return false;
    }

    // comp 内で pt の 8近傍にある画素を返す
    vec2u[] getNeighbors(vec2u pt, vec2u[] comp) {
        vec2u[] neighbors;
        for (int di = -1; di <= 1; di++) {
            for (int dj = -1; dj <= 1; dj++) {
                if (di == 0 && dj == 0) continue;
                vec2u np = vec2u(pt.x + dj, pt.y + di);
                if (inComponent(np, comp))
                    neighbors ~= np;
            }
        }
        return neighbors;
    }

    import std.typecons : Tuple, tuple;
    import std.array : array;
    // BFS を用いて、コンポーネント内の src からの最遠点とその経路（src～最遠点）を求める
    Tuple!(vec2u, vec2u[]) bfsDiameter(vec2u src, vec2u[] comp) {
        vec2u[] queue;
        int[] dist = new int[height * width];
        foreach (i; 0 .. dist.length)
            dist[i] = -1;
        // 各点の直前の点を記録するための連想配列（キーは encode した値）
        vec2u[int] pred;
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
        vec2u farthest = src;
        foreach (pt; comp) {
            int d = dist[encode(pt)];
            if (d > maxDist) {
                maxDist = d;
                farthest = pt;
            }
        }
        // 最遠点から src までの経路を復元（逆順に得られるので reverse する）
        vec2u[] pathReversed;
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
                auto comp = getComponent(vec2u(j, i));
                // 端点（近傍が1個）の探索
                vec2u[] endvec2us;
                foreach (pt; comp) {
                    auto nbrs = getNeighbors(pt, comp);
                    if (nbrs.length == 1)
                        endvec2us ~= pt;
                }
                // 端点が存在すればそのうちの1つ、なければ comp の最初の点を直径計算の起点とする
                vec2u startForDiameter = (endvec2us.length > 0) ? endvec2us[0] : comp[0];
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