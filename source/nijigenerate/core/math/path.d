module nijigenerate.core.math.path;

import inmath;
import mir.ndslice;
import std.algorithm;
import core.thread.fiber;
import std.typecons : Tuple, tuple;
import std.array : array;

vec2u[] extractPath(T)(T skeleton, int width, int height)
{
    if (skeleton.length == 0) return [];
    
    // 連結成分管理用のグローバル visited 配列（NDslice）
    ubyte[] globalVisitedData = new ubyte[height * width];
    int err;
    auto globalVisited = globalVisitedData.sliced.reshape([height, width], err);
    vec2u[] longestPathOverall;

    // 2次元座標を1次元インデックスに変換するヘルパー関数
    int encode(vec2u p) {
        return p.y * width + p.x;
    }

    // 与えた点から連結成分をBFSで収集する（キューは先頭インデックス管理により高速化）
    vec2u[] getComponent(vec2u p) {
        vec2u[] comp;
        vec2u[] queue;
        size_t head = 0;
        queue ~= p;
        globalVisited[p.y, p.x] = 1;
        while(head < queue.length)
        {
            auto cur = queue[head];
            head++;
            comp ~= cur;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if(di == 0 && dj == 0) continue;
                    int ny = cur.y + di;
                    int nx = cur.x + dj;
                    if(ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    if(skeleton[ny, nx] && !globalVisited[ny, nx]) {
                        globalVisited[ny, nx] = 1;
                        queue ~= vec2u(nx, ny);
                    }
                }
            }
        }
        return comp;
    }

    // コンポーネント内の隣接点を、連想配列 compMap を使って高速に探索する
    vec2u[] getNeighbors(vec2u pt, bool[int] compMap) {
        vec2u[] neighbors;
        for (int di = -1; di <= 1; di++) {
            for (int dj = -1; dj <= 1; dj++) {
                if (di == 0 && dj == 0) continue;
                vec2u np = vec2u(pt.x + dj, pt.y + di);
                if (compMap.get(encode(np), false))
                    neighbors ~= np;
            }
        }
        return neighbors;
    }

    // BFSを用いて、コンポーネント内の src からの最遠点とその経路を求める
    Tuple!(vec2u, vec2u[]) bfsDiameter(vec2u src, vec2u[] comp, bool[int] compMap) {
        vec2u[] queue;
        int[] dist = new int[height * width];
        foreach (i; 0 .. dist.length)
            dist[i] = -1;
        vec2u[int] pred; // 各点の直前の点を記録する連想配列

        int head = 0;
        queue ~= src;
        dist[encode(src)] = 0;
        while(head < queue.length) {
            auto cur = queue[head];
            head++;
            foreach (n; getNeighbors(cur, compMap)) {
                int enc = encode(n);
                if (dist[enc] == -1) {
                    dist[enc] = dist[encode(cur)] + 1;
                    pred[enc] = cur;
                    queue ~= n;
                }
            }
        }
        int maxDist = 0;
        vec2u farthest = src;
        foreach (pt; comp) {
            int d = dist[encode(pt)];
            if(d > maxDist) {
                maxDist = d;
                farthest = pt;
            }
            Fiber.yield;
        }
        vec2u[] pathReversed;
        auto cur = farthest;
        while(true) {
            pathReversed ~= cur;
            if(cur.x == src.x && cur.y == src.y)
                break;
            cur = pred[encode(cur)];
        }
        auto path = pathReversed.reverse.array;
        return tuple(farthest, path);
    }

    // skeleton 全体を走査し、各連結成分ごとに直径（最長パス）を求める
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (skeleton[i, j] && !globalVisited[i, j]) {
                // 新たな連結成分を取得
                auto comp = getComponent(vec2u(j, i));
                // 連想配列を用いてコンポーネント内の各点をマーク（定数時間アクセス）
                bool[int] compMap;
                foreach (pt; comp) {
                    // ptが有効な座標であることを確認
                    if (pt.x < 0 || pt.x >= width || pt.y < 0 || pt.y >= height)
                        continue;
                    compMap[encode(pt)] = true;
                }
                Fiber.yield();
                // 端点（隣接画素が1個）の探索（線形探索ではなく、compMapを利用）
                vec2u[] endpoints;
                foreach (pt; comp) {
                    auto nbrs = getNeighbors(pt, compMap);
                    if (nbrs.length == 1)
                        endpoints ~= pt;
                    Fiber.yield;
                }
                // 端点が存在すればそのうちの1つ、なければ comp の最初の点を直径計算の起点とする
                vec2u startForDiameter = (endpoints.length > 0) ? endpoints[0] : comp[0];
                auto result1 = bfsDiameter(startForDiameter, comp, compMap);
                auto result2 = bfsDiameter(result1[0], comp, compMap);
                if (result2[1].length > longestPathOverall.length)
                    longestPathOverall = result2[1];
            }
        }
    }
    return longestPathOverall;
}
