#include "Router.h"
#include <queue>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <numeric>

namespace router
{
using juce::Point;
using juce::Rectangle;

static constexpr float STEP = 10.0f;      // grid, in design pixels
static constexpr float STUB = 16.0f;      // straight out of the outlet / into the inlet
static constexpr int TURN = 4;            // a corner costs this many steps
static constexpr int SHARED = 1;          // running along another outlet's cable costs this per step (its own: nothing)

namespace
{
struct Grid
{
    Rectangle<float> area;
    int w = 0, h = 0;
    std::vector<uint8_t> blocked;
    std::vector<uint16_t> used;
    std::vector<int> owner;         // the outlet whose cable last passed here

    int idx (int x, int y) const { return y * w + x; }
    bool inside (int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Point<int> cellOf (Point<float> p) const
    {
        return { juce::jlimit (0, w - 1, (int) std::floor ((p.x - area.getX()) / STEP)),
                 juce::jlimit (0, h - 1, (int) std::floor ((p.y - area.getY()) / STEP)) };
    }
    Point<float> centre (int x, int y) const { return { area.getX() + ((float) x + 0.5f) * STEP, area.getY() + ((float) y + 0.5f) * STEP }; }
};

const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };   // right, down, left, up

// A* over (cell, heading): a step costs 1, a turn TURN, a cell another cable uses SHARED
std::vector<Point<int>> search (Grid& g, Point<int> s, Point<int> t, int source)
{
    const int n = g.w * g.h * 4;
    std::vector<int> cost ((size_t) n, INT32_MAX), from ((size_t) n, -1);
    using Item = std::pair<int, int>;                 // f, state
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    auto state = [&] (int x, int y, int d) { return (g.idx (x, y)) * 4 + d; };
    auto h = [&] (int x, int y) { return std::abs (x - t.x) + std::abs (y - t.y); };
    int s0 = state (s.x, s.y, 0);                     // leaving the outlet heading right
    cost[(size_t) s0] = 0;
    open.push ({ h (s.x, s.y), s0 });
    int goal = -1;
    while (! open.empty())
    {
        auto [f, st] = open.top(); open.pop();
        int d = st % 4, c = st / 4, x = c % g.w, y = c / g.w;
        if (f - h (x, y) > cost[(size_t) st]) continue;
        if (x == t.x && y == t.y) { goal = st; break; }
        for (int nd = 0; nd < 4; ++nd)
        {
            if (nd == (d + 2) % 4) continue;          // no going straight back
            int nx = x + DX[nd], ny = y + DY[nd];
            if (! g.inside (nx, ny) || g.blocked[(size_t) g.idx (nx, ny)]) continue;
            const auto ci = (size_t) g.idx (nx, ny);
            int step = 1 + (nd != d ? TURN : 0) + (g.owner[ci] == source && source >= 0 ? 0 : SHARED * (int) std::min<uint16_t> (g.used[ci], 3));
            if (nx == t.x && ny == t.y && nd != 0) step += TURN;   // come into the inlet from the left
            int ns = state (nx, ny, nd);
            int nc = cost[(size_t) st] + step;
            if (nc < cost[(size_t) ns]) { cost[(size_t) ns] = nc; from[(size_t) ns] = st; open.push ({ nc + h (nx, ny), ns }); }
        }
    }
    std::vector<Point<int>> cells;
    for (int st = goal; st >= 0; st = from[(size_t) st])
    {
        int c = st / 4;
        Point<int> p { c % g.w, c / g.w };
        if (cells.empty() || cells.back() != p) cells.push_back (p);
    }
    std::reverse (cells.begin(), cells.end());
    return cells;
}
}

std::vector<juce::Path> route (const std::vector<Rectangle<float>>& boards, const std::vector<Cable>& cables)
{
    std::vector<juce::Path> out (cables.size());
    if (cables.empty()) return out;

    Grid g;
    Rectangle<float> all (cables.front().from, cables.front().to);
    for (auto& b : boards) all = all.getUnion (b);
    for (auto& c : cables) all = all.getUnion (Rectangle<float> (c.from, c.to));
    g.area = all.expanded (120);
    g.w = (int) std::ceil (g.area.getWidth() / STEP);
    g.h = (int) std::ceil (g.area.getHeight() / STEP);
    if ((long) g.w * g.h > 400000) return out;          // absurdly spread out: leave them as curves
    g.blocked.assign ((size_t) (g.w * g.h), 0);
    g.used.assign ((size_t) (g.w * g.h), 0);
    g.owner.assign ((size_t) (g.w * g.h), -1);
    for (auto& b : boards)
    {
        auto r = b.expanded (5);
        for (int y = 0; y < g.h; ++y)
            for (int x = 0; x < g.w; ++x)
                if (r.contains (g.centre (x, y))) g.blocked[(size_t) g.idx (x, y)] = 1;
    }

    // short cables first: they have the fewest ways round
    std::vector<size_t> order (cables.size());
    std::iota (order.begin(), order.end(), 0);
    std::sort (order.begin(), order.end(), [&] (size_t a, size_t b)
               { return cables[a].from.getDistanceFrom (cables[a].to) < cables[b].from.getDistanceFrom (cables[b].to); });

    for (auto i : order)
    {
        auto a = cables[i].from, b = cables[i].to;
        auto s = g.cellOf (a + Point<float> (STUB, 0)), t = g.cellOf (b - Point<float> (STUB, 0));
        auto keepS = g.blocked[(size_t) g.idx (s.x, s.y)], keepT = g.blocked[(size_t) g.idx (t.x, t.y)];
        g.blocked[(size_t) g.idx (s.x, s.y)] = 0; g.blocked[(size_t) g.idx (t.x, t.y)] = 0;
        auto cells = search (g, s, t, cables[i].source);
        g.blocked[(size_t) g.idx (s.x, s.y)] = keepS; g.blocked[(size_t) g.idx (t.x, t.y)] = keepT;
        if (cells.size() < 2) continue;                   // no way round (or no distance): it stays a curve
        for (auto& c : cells) { g.used[(size_t) g.idx (c.x, c.y)]++; g.owner[(size_t) g.idx (c.x, c.y)] = cables[i].source; }

        // corners only
        std::vector<Point<float>> pts;
        for (size_t k = 0; k < cells.size(); ++k)
        {
            bool corner = k == 0 || k + 1 == cells.size()
                          || (cells[k - 1].x == cells[k].x) != (cells[k].x == cells[k + 1].x);
            if (corner) pts.push_back (g.centre (cells[k].x, cells[k].y));
        }
        // line the first and last runs up with the ports, so they leave and arrive dead straight
        if (pts.size() >= 2 && std::abs (pts[0].y - pts[1].y) < 0.5f) pts[0].y = pts[1].y = a.y;
        else pts.insert (pts.begin(), Point<float> (pts[0].x, a.y));
        size_t L = pts.size() - 1;
        if (pts.size() >= 2 && std::abs (pts[L].y - pts[L - 1].y) < 0.5f) pts[L].y = pts[L - 1].y = b.y;
        else pts.push_back (Point<float> (pts[L].x, b.y));

        juce::Path p;
        p.startNewSubPath (a);
        for (auto& q : pts) p.lineTo (q);
        p.lineTo (b);
        out[i] = p.createPathWithRoundedCorners (7.0f);
    }
    return out;
}
}
