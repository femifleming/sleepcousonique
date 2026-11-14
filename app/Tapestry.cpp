#include "Tapestry.h"
#include "Art.h"
#include "Scales.h"
#include "Router.h"

using namespace juce;
using namespace scq;

static const char* HAND_ROWS[] = { "fader1", "fader2", "fader3", "fader4", "button1", "button2", "button3", "button4",
                                   "rocker", "rocker.up", "rocker.down", "play", "dots" };
static const char* EAR_ROWS[]  = { "left", "right", "out", "leds", "leds.track", "leds.play",
                                   "scene.next", "scene.back", "scene.miss" };
static constexpr float BW = 168, HEAD = 46, ROW = 17;

static int rowIndex (const char* const* rows, int n, const String& t)
{
    for (int i = 0; i < n; ++i) if (t == rows[i]) return i;
    return -1;
}

Tapestry::Tapestry (Document& d, Host& h) : doc (d), host (h)
{
    setWantsKeyboardFocus (true);
    themeChanged();
    startTimerHz (15);
}

int Tapestry::selectedNode() const
{
    if (selected.size() != 1) return -1;
    int b = *selected.begin();
    return b < (int) boards.size() ? boards[(size_t) b].node : -1;
}

void Tapestry::themeChanged()
{
    canvasImg = Image (Image::RGB, 1400, 1000, true);
    Graphics g (canvasImg);
    art::paintCanvas (g, { 0, 0, 1400, 1000 }, 11);
    routesDirty = true;
    repaint();
}

void Tapestry::resized() {}

void Tapestry::refresh()
{
    layout();
    routesDirty = true;
    selectedCord = -1;
    hoverCord = -1;
    std::set<int> keep;
    for (int b : selected) if (b < (int) boards.size()) keep.insert (b);
    selected = keep;
    repaint();
}

void Tapestry::edited (bool recompile)
{
    if (onTextEdited) onTextEdited (recompile);
}

String Tapestry::arrow (int kind) const
{
    switch (kind) { case 1: return "~>"; case 2: return "=>"; case 3: return ">|"; default: return "->"; }
}

// ------------------------------------------------------------------------------------------ layout

Point<float> Tapestry::defaultPos (const NodeInfo& n, int index)
{
    auto* p = findPlace (n.place);
    int fam = p ? (int) p->family : 0;
    int col = fam == Current ? 0 : (fam == Source ? 1 : (fam == Shape ? 2 : 3));
    return { 250.0f + (float) col * 220.0f, 40.0f + (float) (index % 6) * 150.0f + (float) col * 12.0f };
}

void Tapestry::layout()
{
    boards.clear();
    auto pos = [this] (const String& key, Point<float> def)
    {
        auto it = doc.positions.find (doc.key (doc.scene, key));
        return it != doc.positions.end() ? it->second : def;
    };

    Board hands;
    hands.node = -1; hands.key = "@hands"; hands.title = "SP-1"; hands.sub = "hands"; hands.family = Hands;
    hands.help = "the SP-1's faders, track buttons, rocker, PLAY and dots";
    for (auto* r : HAND_ROWS)
    {
        String t (r);
        int sig = t.startsWith ("fader") || t == "rocker" ? Flow : Pulse;
        hands.outs.push_back ({ t, t, sig });
    }
    hands.r = { 0, 0, 120, HEAD + ROW * (float) hands.outs.size() + 8 };
    hands.r.setPosition (pos ("@hands", { 30, 40 }));
    boards.push_back (hands);

    int perCol[4] = { 0, 0, 0, 0 };
    float maxX = 900;
    for (auto& n : doc.nodes)
    {
        auto* p = findPlace (n.place);
        if (! p || p->family == Hands || p->family == Ears) continue;
        Board b;
        b.node = n.id; b.key = n.name; b.title = n.place; b.family = p->family; b.help = p->help;
        b.sub = (n.name != n.place ? n.name + "  " : String()) + (p->nature != 0 || n.nature != 0 ? String::fromUTF8 ("\xc2\xb7 ") + fmt (n.nature) : String());
        for (auto& [k, v] : n.opts)
            if (k != "tune" && ! k.startsWith ("att.") && ! (v.isNotEmpty() && (CharacterFunctions::isDigit (v[0]) || v[0] == '-' || v[0] == '.')))    // knobs on the board
                b.sub << "  " << (v == "on" ? k : (v == "off" ? "no " + k : v));
        b.hasTune = p->tune;
        b.hasAtt = p->att;
        if (n.opts.count ("tune")) b.tune = n.opts.at ("tune").getFloatValue();
        for (auto& in : p->ins)
            b.att.push_back (n.opts.count ("att." + in) ? n.opts.at ("att." + in).getFloatValue() : 1.0f);
        for (int k = 0; k < p->ins.size(); ++k)        // panel knobs, as the module would have them
        {
            float lo = 0, hi = 1;
            bool has = scq::knobRange (*p, k, lo, hi);
            for (auto& o : p->opts) if (o.name == p->ins[k]) has = false;   // a param of that name is its knob already
            float def = scq::knobDefault (*p, k);
            b.hasPanel.push_back (has ? 1 : 0);
            b.panelLo.push_back (lo); b.panelHi.push_back (hi); b.panelDef.push_back (def);
            b.panel.push_back (n.opts.count (p->ins[k]) ? n.opts.at (p->ins[k]).getFloatValue() : def);
            b.anyPanel = b.anyPanel || has;
        }
        for (int k = 0; k < (int) p->opts.size(); ++k)       // numeric params become knobs
        {
            auto& o = p->opts[(size_t) k];
            if (! o.positions.isEmpty() || o.name == "tune" || o.name == "samplerate") continue;
            Board::Param pr;
            pr.name = o.name; pr.k = k;
            pr.def = o.def.getFloatValue();
            pr.value = n.opts.count (o.name) ? n.opts.at (o.name).getFloatValue() : pr.def;
            if (o.name == "ratio") { pr.lo = 1; pr.hi = 3; }
            else { pr.lo = 0; pr.hi = jmax (1.0f, pr.def * 4); }
            b.params.push_back (pr);
        }
        for (int i = 0; i < p->ins.size(); ++i)
            b.ins.push_back ({ p->ins[i], n.name + (i > 0 ? "." + p->ins[i] : String()), 0 });
        for (int i = 0; i < p->outs.size(); ++i)
            b.outs.push_back ({ p->outs[i], n.name + (i > 0 ? "." + p->outs[i] : String()), (int) outletSignal (*p, i) });
        int rows = jmax ((int) b.ins.size(), (int) b.outs.size(), 1);
        b.r = { 0, 0, b.anyPanel ? BW + 22 : BW, HEAD + ROW * (float) rows + 8 };
        int col = p->family == Current ? 0 : (p->family == Source ? 1 : (p->family == Shape ? 2 : 3));
        Point<float> def (250.0f + (float) col * 220.0f, 40.0f + (float) perCol[col]++ * 0.0f);
        def.y = 40.0f;
        for (auto& o : boards)    // stack under whatever already sits in this column
            if (o.node >= 0 && std::abs (o.r.getX() - def.x) < 1 && o.r.getBottom() + 24 > def.y) def.y = o.r.getBottom() + 24;
        b.r.setPosition (pos (n.name, def));
        maxX = jmax (maxX, b.r.getRight());
        boards.push_back (b);
    }

    // nothing hides behind anything: a board that overlaps one before it moves down, below it
    for (size_t i = 1; i < boards.size(); ++i)
    {
        for (int guard = 0; guard < 100; ++guard)
        {
            bool moved = false;
            for (size_t j = 0; j < i; ++j)
                if (boards[i].r.intersects (boards[j].r.expanded (6)))
                {
                    boards[i].r.setY (boards[j].r.getBottom() + 14);
                    moved = true;
                }
            if (! moved) break;
        }
    }

    Board ears;
    ears.node = -2; ears.key = "@ears"; ears.title = "SP-1"; ears.sub = "out, leds, scene"; ears.family = Ears;
    ears.help = "the outputs (left, right, out = both), the eight LEDs and scene changes";
    for (auto* r : EAR_ROWS) ears.ins.push_back ({ r, r, 0 });
    ears.r = { 0, 0, 132, HEAD + ROW * (float) ears.ins.size() + 8 };
    ears.r.setPosition (pos ("@ears", { maxX + 90, 40 }));
    boards.push_back (ears);
}

Point<float> Tapestry::inletPos (const Board& b, int i) const { return { b.r.getX(), b.r.getY() + HEAD + ROW * (float) i + 8 }; }
Point<float> Tapestry::outletPos (const Board& b, int i) const { return { b.r.getRight(), b.r.getY() + HEAD + ROW * (float) i + 8 }; }
Point<float> Tapestry::tunePos (const Board& b) const { return { b.r.getRight() - 21, b.r.getY() + 25 }; }
Point<float> Tapestry::paramPos (const Board& b, int j) const { return tunePos (b).translated (-28.0f * (float) (j + (b.hasTune ? 1 : 0)), 0); }
Point<float> Tapestry::attPos (const Board& b, int i) const { return { b.r.getX() + (b.anyPanel ? 98.0f : 76.0f), inletPos (b, i).y }; }
Point<float> Tapestry::panelPos (const Board& b, int i) const { return { b.r.getX() + 78, inletPos (b, i).y }; }

static constexpr float TUNE_RANGE = 12;   // the tune knob: +-12 octaves (well below 1 Hz to past 15 kHz)
static float attRange (const String& inlet) { return inlet == "pitch" ? 10.0f : 2.0f; }

Tapestry::Knob Tapestry::knobAt (Point<float> d) const
{
    for (int i = (int) boards.size(); --i >= 0;)
    {
        auto& b = boards[(size_t) i];
        if (b.node < 0) continue;
        if (b.hasTune && tunePos (b).getDistanceFrom (d) < 13) return { i, -1 };
        for (int j = 0; j < (int) b.params.size(); ++j)
            if (paramPos (b, j).getDistanceFrom (d) < 12) return { i, -2, j };
        for (int k = 0; k < (int) b.ins.size() && k < (int) b.hasPanel.size(); ++k)
            if (b.hasPanel[(size_t) k] && panelPos (b, k).getDistanceFrom (d) < 8.0f) return { i, k, -1, true };
        if (b.hasAtt)
            for (int k = 0; k < (int) b.ins.size(); ++k)
                if (attPos (b, k).getDistanceFrom (d) < 6.0f) return { i, k };
    }
    return {};
}

// a little knob: a cream cap, a tick for the value, an arc from zero (bipolar) or from the bottom
void Tapestry::drawKnob (Graphics& g, Point<float> c, float rad, float value, float range, bool bipolar, bool hot) const
{
    const float sweep = MathConstants<float>::pi * 0.75f;            // +-135 degrees
    float v = jlimit (-1.0f, 1.0f, value / range);
    float a = v * sweep, a0 = bipolar ? 0.0f : -sweep;
    g.setColour (Colour (0x30000000));
    g.fillEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (c).translated (0.8f, 1.5f));
    const bool pop = art::theme().pop;
    g.setColour (hot ? art::gold.brighter (0.4f) : (pop ? Colour (0xff141418) : (art::theme().dark ? Colour (0xff6a6254) : Colour (0xfff4efe2))));
    g.fillEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (c));
    g.setColour (pop ? art::theme().cardHead : art::ink.withAlpha (0.7f));
    g.drawEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (c), 1.0f);
    if (std::abs (a - a0) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, rad + 2.2f, rad + 2.2f, 0, jmin (a0, a), jmax (a0, a), true);
        g.setColour (value < 0 && bipolar ? art::umber : art::gold.darker (0.15f));
        g.strokePath (arc, PathStrokeType (1.8f));
    }
    auto tip = c.getPointOnCircumference (rad * 0.85f, a);
    g.setColour (pop && ! hot ? Colours::white : art::ink);
    g.drawLine ({ c.getPointOnCircumference (rad * 0.2f, a), tip }, 1.6f);
}

bool Tapestry::endpoint (int nodeId, int port, bool out, int& board, int& row) const
{
    auto* n = doc.node (nodeId);
    auto* p = n ? findPlace (n->place) : nullptr;
    if (! p) return false;
    if (p->family == Hands)
    {
        String t = n->name;
        if (p->name == "rocker" && port > 0) t = port == 1 ? "rocker.up" : "rocker.down";
        board = 0;
        row = rowIndex (HAND_ROWS, 13, t);
        return row >= 0;
    }
    if (p->family == Ears)
    {
        String t = n->name;
        if (p->name == "leds" && port > 0) t = port == 1 ? "leds.track" : "leds.play";
        if (p->name == "scene") t = port == 0 ? "scene.next" : (port == 1 ? "scene.back" : "scene.miss");
        board = (int) boards.size() - 1;
        row = rowIndex (EAR_ROWS, 9, t);
        return row >= 0 && ! out;
    }
    for (int i = 0; i < (int) boards.size(); ++i)
        if (boards[(size_t) i].node == nodeId) { board = i; row = port; return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ hits

Tapestry::Hit Tapestry::portAt (Point<float> d, float slop) const
{
    for (int i = (int) boards.size(); --i >= 0;)
    {
        auto& b = boards[(size_t) i];
        for (int k = 0; k < (int) b.ins.size(); ++k)
            if (inletPos (b, k).getDistanceFrom (d) < slop) return { i, false, k };
        for (int k = 0; k < (int) b.outs.size(); ++k)
            if (outletPos (b, k).getDistanceFrom (d) < slop) return { i, true, k };
    }
    return {};
}

int Tapestry::boardAt (Point<float> d) const
{
    for (int i = (int) boards.size(); --i >= 0;)
        if (boards[(size_t) i].r.contains (d)) return i;
    return -1;
}

Rectangle<float> Tapestry::bottleRect (int ci) const
{
    auto& c = doc.cords[(size_t) ci];
    int b, row;
    if (! endpoint (c.dst, c.dport, false, b, row)) return {};
    int nth = 0;
    for (int k = 0; k < ci; ++k)
        if (doc.cords[(size_t) k].src < 0 && doc.cords[(size_t) k].dst == c.dst && doc.cords[(size_t) k].dport == c.dport) ++nth;
    auto p = inletPos (boards[(size_t) b], row);
    return Rectangle<float> (40, 14).withRightX (p.x - 9 - (float) nth * 44).withCentre ({ p.x - 29 - (float) nth * 44, p.y });
}

int Tapestry::bottleAt (Point<float> d) const
{
    for (int k = 0; k < (int) doc.cords.size(); ++k)
        if (doc.cords[(size_t) k].src < 0 && bottleRect (k).expanded (2).contains (d)) return k;
    return -1;
}

bool Tapestry::cordEnds (int k, Point<float>& a, Point<float>& b) const
{
    auto& c = doc.cords[(size_t) k];
    int sb, sr, db, dr;
    if (c.src < 0 || ! endpoint (c.src, c.sport, true, sb, sr) || ! endpoint (c.dst, c.dport, false, db, dr)) return false;
    a = outletPos (boards[(size_t) sb], sr);
    b = inletPos (boards[(size_t) db], dr);
    return true;
}

Path Tapestry::cordPath (int k) const
{
    Point<float> a, b;
    if (! cordEnds (k, a, b)) return {};
    if (routing && mode != Mode::Move)            // while a board is being dragged, cables hang; they route when you let go
    {
        if (routesDirty || routes.size() != doc.cords.size())
        {
            std::vector<Rectangle<float>> obstacles;
            for (auto& bd : boards) obstacles.push_back (bd.r);
            std::vector<router::Cable> cables;
            std::vector<int> which;
            for (int j = 0; j < (int) doc.cords.size(); ++j)
            {
                Point<float> p, q;
                if (cordEnds (j, p, q)) { cables.push_back ({ p, q, doc.cords[(size_t) j].src * 16 + doc.cords[(size_t) j].sport }); which.push_back (j); }
            }
            auto paths = router::route (obstacles, cables);
            routes.assign (doc.cords.size(), Path());
            for (size_t j = 0; j < which.size(); ++j) routes[(size_t) which[j]] = paths[j];
            routesDirty = false;
        }
        if (k < (int) routes.size() && ! routes[(size_t) k].isEmpty()) return routes[(size_t) k];
    }
    auto& c = doc.cords[(size_t) k];
    int sb, sr;
    bool audio = endpoint (c.src, c.sport, true, sb, sr) && boards[(size_t) sb].outs[(size_t) sr].signal == scq::Sound;
    return art::cablePath (a, b, audio ? 1.25f : 1.0f);     // the fat audio leads hang a little lower
}

void Tapestry::setRouting (bool on) { routing = on; routesDirty = true; repaint(); }

void Tapestry::setLight (float l)
{
    light = jlimit (0.1f, 1.0f, l);
    art::cableStyle().glow = jlimit (0.0f, 1.0f, (1.0f - light) * 1.4f);
    repaint();
}

void Tapestry::setHighlight (int a, int b)
{
    if (a == hlFirst && b == hlLast) return;
    hlFirst = a; hlLast = b;
    repaint();
}

int Tapestry::cordAt (Point<float> d) const
{
    for (int k = (int) doc.cords.size(); --k >= 0;)
    {
        auto& c = doc.cords[(size_t) k];
        if (c.src < 0) continue;
        int sb, sr, db, dr;
        if (! endpoint (c.src, c.sport, true, sb, sr) || ! endpoint (c.dst, c.dport, false, db, dr)) continue;
        Point<float> onPath;
        cordPath (k).getNearestPoint (d, onPath);
        if (d.getDistanceFrom (onPath) < 6.0f / zoom + 2) return k;
    }
    return -1;
}

// ------------------------------------------------------------------------------------------ paint

void Tapestry::paint (Graphics& g)
{
    // canvas tiles
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::translation (-pan.x, -pan.y).scaled (zoom));
        auto vis = Rectangle<float> (pan.x, pan.y, (float) getWidth() / zoom, (float) getHeight() / zoom);
        const float tw = (float) canvasImg.getWidth(), th = (float) canvasImg.getHeight();
        for (float y = std::floor (vis.getY() / th) * th; y < vis.getBottom(); y += th)
            for (float x = std::floor (vis.getX() / tw) * tw; x < vis.getRight(); x += tw)
                g.drawImageAt (canvasImg, (int) x, (int) y);
        if (light < 0.999f)                    // the lights are down
        {
            g.setColour (Colours::black.withAlpha ((1.0f - light) * 0.85f));
            g.fillRect (vis);
        }

        auto drawCords = [&]
        {
        for (int k = 0; k < (int) doc.cords.size(); ++k)
        {
            auto& c = doc.cords[(size_t) k];
            int db, dr;
            if (! endpoint (c.dst, c.dport, false, db, dr)) continue;
            if (c.src < 0)
            {
                auto br = bottleRect (k);
                auto ip = inletPos (boards[(size_t) db], dr);
                g.setColour (art::ink.withAlpha (0.6f));
                g.drawLine (br.getRight(), br.getCentreY(), ip.x - 5, ip.y, 1.4f);
                g.setColour (selectedCord == k ? art::gold : art::theme().card);
                g.fillRoundedRectangle (br, 6);
                g.setColour (art::blue);
                g.drawRoundedRectangle (br, 6, 1.2f);
                g.setFont (art::mono (10.5f));
                g.drawText (fmt (k == valueCord ? valueNow : c.amt), br, Justification::centred);
                continue;
            }
            int sb, sr;
            if (! endpoint (c.src, c.sport, true, sb, sr)) continue;
            auto& from = boards[(size_t) sb];
            auto col = art::signalColour (from.outs[(size_t) sr].signal);
            auto path = cordPath (k);
            art::drawCablePath (g, path, col, c.kind == scq::Range ? 4 : c.kind, k == hoverCord || k == selectedCord, 1.0f, (int) from.outs[(size_t) sr].signal);
            if ((c.kind != Hall || std::abs (c.amt - 100) > 0.01f) && c.kind != Door)
            {
                auto m = path.getPointAlongPath (path.getLength() * 0.5f);
                g.setFont (art::mono (9.5f));
                g.setColour (art::ink.withAlpha (0.8f));
                String t = c.kind == scq::Range ? fmt (c.amt) + ".." + fmt (c.amt2) : (c.kind == Water ? fmt (c.amt) + "ms" : fmt (c.amt) + "%");
                g.drawText (t, Rectangle<float> (m.x - 30, m.y + 7, 60, 12), Justification::centred);
            }
        }


        };
        // the journal look: cords under the boards, so nothing on a board is hidden;
        // the openprocess look: over them
        if (! art::theme().pop) drawCords();

        // boards, the selected ones on top
        std::vector<int> order;
        for (int i = 0; i < (int) boards.size(); ++i) if (! selected.count (i)) order.push_back (i);
        for (int i : selected) if (i < (int) boards.size()) order.push_back (i);
        for (int i : order)
        {
            auto& b = boards[(size_t) i];
            auto r = b.r;
            bool sp1 = b.node < 0;
            auto& th = art::theme();
            if (th.pop)                       // a flat card with a grey head; the head goes checker when selected
            {
                g.setColour (sp1 ? th.cardSp1 : th.card);
                g.fillRect (r);
                auto head = r.withHeight (22);
                if (selected.count (i)) art::fillChecker (g, head);
                else { g.setColour (th.cardHead); g.fillRect (head); }
                g.setColour (art::familyColour (b.family));
                g.fillRect (head.withWidth (5));
                g.setColour (selected.count (i) ? th.checkB : th.cardHead);
                g.drawRect (r, selected.count (i) ? 2.0f : 1.2f);
                g.setColour (selected.count (i) ? Colour (0xff1a1a1a) : th.cardHeadText);
                g.setFont (art::label (12));
                g.drawText (b.title.toLowerCase(), head.reduced (11, 0), Justification::centredLeft);
                g.setColour (th.chromeDim.interpolatedWith (art::ink, 0.3f));
                g.setFont (art::label (9.5f));
            }
            else
            {
                g.setColour (Colour (0x40000000));
                g.fillRoundedRectangle (r.translated (3, 6), 5);
                g.setColour (sp1 ? th.cardSp1 : art::cream);
                g.fillRoundedRectangle (r, 5);
                g.setColour (art::familyColour (b.family).withAlpha (sp1 ? 0.5f : 0.85f));
                g.fillRoundedRectangle (r.withHeight (6), 3);
                g.setColour (selected.count (i) ? art::gold : (th.dark ? Colour (0x55ffffff) : Colour (0x55000000)));
                g.drawRoundedRectangle (r, 5, selected.count (i) ? 2.4f : 1.0f);
                auto label = r.reduced (10, 0).withY (r.getY() + 11).withHeight (17);
                art::drawDymo (g, label.withWidth (jmin (label.getWidth(), 12.0f + 8.6f * (float) b.title.length())), b.title);
                g.setColour (art::ink.withAlpha (0.75f));
                g.setFont (art::hand (12));
            }
            g.drawText (b.sub, r.reduced (10, 0).withY (r.getY() + 29).withHeight (15).withTrimmedRight ((b.hasTune ? 26.0f : 0.0f) + 28.0f * (float) b.params.size()), Justification::left);
            for (int j = 0; j < (int) b.params.size(); ++j)
            {
                auto& pr = b.params[(size_t) j];
                bool hot = (knob.board == i && knob.param == j) || (hoverKnob.board == i && hoverKnob.param == j);
                // unipolar from lo: the arc shows how far up its range it is
                drawKnob (g, paramPos (b, j), 9, (pr.value - pr.lo) / (pr.hi - pr.lo) * 2 - 1, 1, false, hot);
                g.setColour (art::ink.withAlpha (0.6f));
                g.setFont (art::mono (8.5f));
                g.drawText (pr.name, Rectangle<float> (44, 10).withCentre (paramPos (b, j).translated (0, 16)), Justification::centred);
            }
            if (b.hasTune)
            {
                bool hot = (knob.board == i && knob.inlet == -1) || (hoverKnob.board == i && hoverKnob.inlet == -1);
                drawKnob (g, tunePos (b), 10, b.tune, TUNE_RANGE, true, hot);
                g.setColour (art::ink.withAlpha (0.6f));
                g.setFont (art::mono (8.5f));
                g.drawText ("tune", Rectangle<float> (40, 10).withCentre (tunePos (b).translated (0, 16)), Justification::centred);
            }
            g.setFont (art::theme().pop ? art::label (9.5f) : art::mono (10.5f));
            for (int k = 0; k < (int) b.ins.size(); ++k)
            {
                auto p = inletPos (b, k);
                art::drawLoop (g, p, hoverPort.board == i && ! hoverPort.out && hoverPort.port == k, art::ink);
                g.setColour (art::ink);
                g.drawText (b.ins[(size_t) k].label, Rectangle<float> (p.x + 9, p.y - 7, (b.hasAtt || b.anyPanel) ? 58.0f : r.getWidth() * 0.55f, 14), Justification::left);
                if (k < (int) b.hasPanel.size() && b.hasPanel[(size_t) k])      // the inlet's own knob
                {
                    bool hot = (knob.board == i && knob.panel && knob.inlet == k) || (hoverKnob.board == i && hoverKnob.panel && hoverKnob.inlet == k);
                    float lo = b.panelLo[(size_t) k], hi = b.panelHi[(size_t) k], v = b.panel[(size_t) k];
                    if (lo < 0) drawKnob (g, panelPos (b, k), 7, v, hi, true, hot);
                    else drawKnob (g, panelPos (b, k), 7, (v - lo) / jmax (1.0e-6f, hi - lo) * 2 - 1, 1, false, hot);
                }
                if (b.hasAtt && k < (int) b.att.size())
                {
                    bool hot = (knob.board == i && ! knob.panel && knob.inlet == k) || (hoverKnob.board == i && ! hoverKnob.panel && hoverKnob.inlet == k);
                    drawKnob (g, attPos (b, k), b.anyPanel ? 4.5f : 5.0f, b.att[(size_t) k], attRange (b.ins[(size_t) k].label), true, hot);
                }
            }
            for (int k = 0; k < (int) b.outs.size(); ++k)
            {
                auto p = outletPos (b, k);
                art::drawLoop (g, p, hoverPort.board == i && hoverPort.out && hoverPort.port == k, art::signalColour (b.outs[(size_t) k].signal));
                g.setColour (art::ink);
                g.drawText (b.outs[(size_t) k].label, Rectangle<float> (p.x - 9 - r.getWidth() * 0.5f, p.y - 7, r.getWidth() * 0.5f, 14), Justification::right);
            }
            if (light < 0.999f)                // lights down: the boards go dark, what's lit stays lit
            {
                g.setColour (Colours::black.withAlpha ((1.0f - light) * 0.72f));
                g.fillRoundedRectangle (r, 5);
            }
            if (b.node >= 0)                   // selected in the journal: outlined in gold
                if (auto* nd = doc.node (b.node); nd && lit (nd->line))
                {
                    g.setColour (art::gold.withAlpha (0.35f));
                    g.drawRoundedRectangle (r.expanded (4), 8, 6.0f);
                    g.setColour (art::gold);
                    g.drawRoundedRectangle (r.expanded (1.5f), 6, 2.6f);
                }
            if (b.node == -2)   // the lamp's eight LEDs, live
                for (int k = 0; k < 8; ++k)
                {
                    float v = host.lamp (k);
                    auto c = Rectangle<float> (7, 7).withCentre ({ r.getX() + 66 + (float) (k % 4) * 12, r.getY() + HEAD + ROW * 3 + 2 + (float) (k / 4) * 10 });
                    g.setColour (Colour (0xff2a2a2a)); g.fillEllipse (c.expanded (1));
                    if (light < 0.999f && v > 0.02f)    // glowing in the dark
                    {
                        auto glowCol = Colour (0xffff3020).interpolatedWith (Colours::white, v);
                        g.setColour (glowCol.withAlpha (0.18f * v * (1.0f - light) * 1.5f)); g.fillEllipse (c.expanded (9));
                        g.setColour (glowCol.withAlpha (0.35f * v * (1.0f - light) * 1.5f)); g.fillEllipse (c.expanded (4));
                    }
                    g.setColour (Colour (0xffff3020).interpolatedWith (Colours::white, v).withAlpha (0.15f + 0.85f * v));
                    g.fillEllipse (c);
                }
        }

        if (art::theme().pop) drawCords();

        // the one you're pointing at or have picked, and those selected in the journal, on top of the boards
        for (int k = 0; k < (int) doc.cords.size(); ++k)
        {
            auto& c = doc.cords[(size_t) k];
            if (c.src < 0 || ! (k == hoverCord || k == selectedCord || lit (c.line))) continue;
            int sb, sr;
            if (! endpoint (c.src, c.sport, true, sb, sr)) continue;
            const int sig = (int) boards[(size_t) sb].outs[(size_t) sr].signal;
            art::drawCablePath (g, cordPath (k), art::signalColour (sig), c.kind == scq::Range ? 4 : c.kind, true, 1.0f, sig);
        }

        // the cord in your hand
        if (mode == Mode::Cable && cableFrom.board >= 0)
        {
            auto& b = boards[(size_t) cableFrom.board];
            auto a = cableFrom.out ? outletPos (b, cableFrom.port) : inletPos (b, cableFrom.port);
            auto col = cableFrom.out ? art::signalColour (b.outs[(size_t) cableFrom.port].signal) : art::umber;
            const int sig = cableFrom.out ? (int) b.outs[(size_t) cableFrom.port].signal : 0;
            if (cableFrom.out) art::drawCable (g, a, dragPos, col, handKind, false, 0.85f, sig);
            else art::drawCable (g, dragPos, a, col, handKind, false, 0.85f, sig);
        }
        if (dropPos.x >= 0)
        {
            g.setColour (art::gold);
            g.drawEllipse (Rectangle<float> (22, 22).withCentre (dropPos), 2.0f);
        }
    }

    // the SP-1s over Bluetooth, top right: one chip each (green: playing, hollow: not answering,
    // faint: left out). Click one to take it in or out.
    for (int i = 0; i < (int) fleet.size(); ++i)
    {
        auto& f = fleet[(size_t) i];
        auto& th = art::theme();
        auto r = chipRect (i).toFloat();
        g.setColour ((th.pop ? th.button : art::cream).withAlpha (f.on ? 1.0f : 0.45f));
        if (th.pop) g.fillRect (r); else g.fillRoundedRectangle (r, 6);
        auto dot = Rectangle<float> (10, 10).withCentre ({ r.getX() + 13, r.getCentreY() });
        g.setColour (f.answered && f.on ? Colour (0xff3fbf62) : th.chromeDim);
        if (f.answered) g.fillEllipse (dot); else g.drawEllipse (dot, 1.6f);
        g.setColour ((th.pop ? th.buttonText : art::ink).withAlpha (f.on ? 1.0f : 0.5f));
        g.setFont (art::label (11));
        g.drawText ("SP-1 " + f.name, r.withTrimmedLeft (24), Justification::centredLeft);
    }

    // find my patch: always there, bottom right
    {
        auto fb = findButton().toFloat();
        auto& th = art::theme();
        if (th.pop) { g.setColour (th.button); g.fillRect (fb); }
        else
        {
            g.setColour (Colour (0x30000000));
            g.fillRoundedRectangle (fb.translated (2, 3), 6);
            g.setColour (art::cream);
            g.fillRoundedRectangle (fb, 6);
            g.setColour (art::ink.withAlpha (0.6f));
            g.drawRoundedRectangle (fb, 6, 1.0f);
        }
        g.setColour (th.pop ? th.buttonText : art::ink);
        g.setFont (art::label (11.5f));
        g.drawText (String::fromUTF8 ("find my patch  \xe2\x8c\x98" "0"), fb, Justification::centred);
    }
    if (doc.nodes.empty() || boards.size() <= 2)
    {
        g.setColour (art::ink.withAlpha (0.7f));
        g.setFont (art::hand (18));
        g.drawText ("right-click to add a place, or write in the journal", getLocalBounds().removeFromBottom (60), Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------ mouse

void Tapestry::timerCallback()
{
    if (hoverPort.board >= 0 && hoverPort.out && onStatus)
    {
        auto& b = boards[(size_t) hoverPort.board];
        if (b.node >= 0)
            onStatus (b.outs[(size_t) hoverPort.port].text + "   " + String (host.probe (b.node, hoverPort.port), 3));
    }
    repaint (getLocalBounds().withWidth (getWidth()));   // lamp + probes
}

void Tapestry::mouseMove (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    lastMouse = d;
    auto hk = knobAt (d);
    if (hk.board != hoverKnob.board || hk.inlet != hoverKnob.inlet || hk.param != hoverKnob.param || hk.panel != hoverKnob.panel)
    {
        hoverKnob = hk;
        if (hk.board >= 0 && onStatus)
        {
            auto& b = boards[(size_t) hk.board];
            if (hk.panel) onStatus (b.ins[(size_t) hk.inlet].text + " " + fmt (b.panel[(size_t) hk.inlet]) + ": its own knob (what's patched in adds to it)");
            else if (hk.param >= 0) onStatus (b.key + " " + b.params[(size_t) hk.param].name + " " + fmt (b.params[(size_t) hk.param].value) + ": drag to set");
            else onStatus (hk.inlet < 0 ? b.key + " tune " + fmt (b.tune) + " octaves: drag to set the base pitch"
                                   : b.ins[(size_t) hk.inlet].text + " attenuverter " + fmt (b.att[(size_t) hk.inlet]) + ": scales what's patched in");
        }
        repaint();
    }
    if (hk.board >= 0) { setMouseCursor (MouseCursor::UpDownResizeCursor); return; }
    auto hp = portAt (d);
    int hc = hp.board < 0 ? cordAt (d) : -1;
    int hb = boardAt (d);
    if (hp.board != hoverPort.board || hp.port != hoverPort.port || hp.out != hoverPort.out || hc != hoverCord || hb != hoverBoard)
    {
        hoverPort = hp; hoverCord = hc; hoverBoard = hb;
        if (onStatus)
        {
            if (hp.board >= 0)
            {
                auto& b = boards[(size_t) hp.board];
                onStatus ((hp.out ? b.outs : b.ins)[(size_t) hp.port].text + (hp.out ? "   drag to an inlet" : "   drag to an outlet"));
            }
            else if (hc >= 0) onStatus (doc.cordText (doc.cords[(size_t) hc]) + "      option-click: remove   cmd-drag: stack another   right-click: change");
            else if (hb >= 0) onStatus (boards[(size_t) hb].title + ": " + boards[(size_t) hb].help);
            else onStatus ({});
        }
        repaint();
    }
    setMouseCursor (hp.board >= 0 ? MouseCursor::CrosshairCursor : (bottleAt (d) >= 0 ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor));
}

void Tapestry::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    if (findButton().contains (e.getPosition()) && ! e.mods.isPopupMenu()) { fitAll(); mode = Mode::None; return; }
    for (int i = 0; i < (int) fleet.size(); ++i)
        if (chipRect (i).contains (e.getPosition()))
        {
            if (onFleetToggle) onFleetToggle (fleet[(size_t) i].id, ! fleet[(size_t) i].on);
            mode = Mode::None;
            return;
        }
    auto d = toDesign (e.position);
    anchor = e.position;
    dragPos = d;
    if (e.mods.isPopupMenu())
    {
        int ci = cordAt (d), bi = boardAt (d), bo = bottleAt (d);
        if (bo >= 0) cordMenu (bo);
        else if (ci >= 0) cordMenu (ci);
        else if (bi >= 0) boardMenu (bi);
        else canvasMenu (d);
        return;
    }
    if (e.mods.isAltDown())          // option-click: pull cables out (a cable, or every cable at a port)
    {
        std::set<int> drop;
        auto hp = portAt (d);
        if (hp.board >= 0)
        {
            for (int k = 0; k < (int) doc.cords.size(); ++k)
            {
                auto& c = doc.cords[(size_t) k];
                int b, row;
                if (hp.out ? (c.src >= 0 && endpoint (c.src, c.sport, true, b, row)) : endpoint (c.dst, c.dport, false, b, row))
                    if (b == hp.board && row == hp.port) drop.insert (k);
            }
        }
        else if (int ci = cordAt (d); ci >= 0) drop.insert (ci);
        else if (int bo = bottleAt (d); bo >= 0) drop.insert (bo);
        if (! drop.empty())
        {
            doc.removeCords (drop);
            selectedCord = -1;
            if (onStatus) onStatus (drop.size() == 1 ? "cable removed   (cmd-Z puts it back)" : String ((int) drop.size()) + " cables removed   (cmd-Z puts them back)");
            edited (true);
        }
        return;
    }
    if (e.mods.isCommandDown())      // cmd-drag a cable: stack another one on its outlet
    {
        int ci = cordAt (d);
        if (ci >= 0 && doc.cords[(size_t) ci].src >= 0)
        {
            int b, row;
            auto& c = doc.cords[(size_t) ci];
            if (endpoint (c.src, c.sport, true, b, row)) { mode = Mode::Cable; cableFrom = { b, true, row }; dragPos = d; return; }
        }
    }
    if (auto kn = knobAt (d); kn.board >= 0)
    {
        auto& b = boards[(size_t) kn.board];
        mode = Mode::Knob; knob = kn;
        knobStart = kn.panel ? b.panel[(size_t) kn.inlet] : kn.param >= 0 ? b.params[(size_t) kn.param].value : (kn.inlet < 0 ? b.tune : b.att[(size_t) kn.inlet]);
        return;
    }
    auto hp = portAt (d);
    if (hp.board >= 0) { mode = Mode::Cable; cableFrom = hp; return; }
    int bo = bottleAt (d);
    if (bo >= 0) { mode = Mode::Value; valueCord = bo; valueStart = valueNow = doc.cords[(size_t) bo].amt; selectedCord = bo; return; }
    int ci = cordAt (d);
    if (ci >= 0) { selectedCord = ci; selected.clear(); repaint(); return; }
    int bi = boardAt (d);
    if (bi >= 0)
    {
        if (e.mods.isShiftDown()) { if (selected.count (bi)) selected.erase (bi); else selected.insert (bi); }
        else if (! selected.count (bi)) { selected.clear(); selected.insert (bi); }
        selectedCord = -1;
        mode = Mode::Move;
        moving = bi;
        moveStart = d;
        repaint();
        return;
    }
    selected.clear();
    selectedCord = -1;
    mode = Mode::Pan;
    repaint();
}

void Tapestry::mouseDrag (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    switch (mode)
    {
        case Mode::Pan:
            pan -= (e.position - anchor) / zoom;
            anchor = e.position;
            repaint();
            break;
        case Mode::Move:
        {
            auto delta = d - moveStart;
            moveStart = d;
            for (int b : selected)
            {
                boards[(size_t) b].r.translate (delta.x, delta.y);
                doc.setPosition (doc.scene, boards[(size_t) b].key, boards[(size_t) b].r.getPosition());
            }
            repaint();
            break;
        }
        case Mode::Cable:
            dragPos = d;
            hoverPort = portAt (d, 14);
            repaint();
            break;
        case Mode::Knob:
        {
            auto& b = boards[(size_t) knob.board];
            float dy = anchor.y - e.position.y;
            bool fine = e.mods.isShiftDown();
            if (knob.panel)           // the inlet's own knob: 200 px across its range, shift for fine
            {
                const auto k = (size_t) knob.inlet;
                float lo = b.panelLo[k], hi = b.panelHi[k];
                float step = (hi - lo) / 200.0f / (fine ? 10.0f : 1.0f);
                float v = jlimit (lo, hi, knobStart + std::round (dy) * step);
                v = std::round (v * 1000.0f) / 1000.0f;
                b.panel[k] = v;
                host.setKnob (b.node, knob.inlet, v);
                if (onStatus) onStatus (b.ins[k].text + " " + fmt (v) + "   shift: fine, double-click: " + fmt (b.panelDef[k]));
            }
            else if (knob.param >= 0) // a param: 200 px across its range, shift for fine
            {
                auto& pr = b.params[(size_t) knob.param];
                float step = (pr.hi - pr.lo) / 200.0f / (fine ? 10.0f : 1.0f);
                pr.value = jlimit (pr.lo, pr.hi, knobStart + std::round (dy) * step);
                pr.value = std::round (pr.value * 1000.0f) / 1000.0f;
                host.setParam (b.node, pr.k, pr.value);
                if (onStatus) onStatus (b.key + " " + pr.name + " " + fmt (pr.value) + "   shift: fine, double-click: " + fmt (pr.def));
            }
            else if (knob.inlet < 0)  // tune: semitones, or cents with shift
            {
                float v = fine ? knobStart + std::round (dy) / 1200.0f : knobStart + std::round (dy / 3.0f) / 12.0f;
                b.tune = jlimit (-TUNE_RANGE, TUNE_RANGE, v);
                host.setTune (b.node, b.tune);
                if (onStatus) onStatus (b.key + " tune " + fmt (b.tune) + " octaves (" + String (b.tune * 12.0f, 1) + " semitones)   shift: fine, double-click: 0");
            }
            else
            {
                float range = attRange (b.ins[(size_t) knob.inlet].label);
                float step = range / 200.0f / (fine ? 10.0f : 1.0f);
                float v = jlimit (-range, range, knobStart + std::round (dy) * step);
                if (std::abs (v) < step * 0.5f) v = 0;
                b.att[(size_t) knob.inlet] = v;
                host.setAtt (b.node, knob.inlet, v);
                if (onStatus) onStatus (b.ins[(size_t) knob.inlet].text + " attenuverter " + fmt (v) + "   shift: fine, double-click: 1");
            }
            repaint();
            break;
        }
        case Mode::Value:
        {
            float step = e.mods.isShiftDown() ? 0.1f : 1.0f;
            valueNow = valueStart + std::round ((anchor.y - e.position.y) / 2.0f) * step;
            host.setAmount (valueCord, valueNow);
            if (onStatus) onStatus ("value " + fmt (valueNow));
            repaint();
            break;
        }
        default: break;
    }
}

void Tapestry::connect (const Port& from, const Port& to)
{
    doc.addLine (from.text + " " + arrow (handKind) + " " + to.text);
    edited (true);
}

void Tapestry::mouseUp (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    if (mode == Mode::Cable)
    {
        auto to = portAt (d, 14);
        if (to.board >= 0 && to.out != cableFrom.out)
        {
            auto& a = boards[(size_t) (cableFrom.out ? cableFrom.board : to.board)];
            auto& b = boards[(size_t) (cableFrom.out ? to.board : cableFrom.board)];
            connect (a.outs[(size_t) (cableFrom.out ? cableFrom.port : to.port)], b.ins[(size_t) (cableFrom.out ? to.port : cableFrom.port)]);
        }
        cableFrom = {};
    }
    else if (mode == Mode::Move && e.getDistanceFromDragStart() > 2)
    {
        routesDirty = true;
        doc.writePositions();
        edited (false);
    }
    else if (mode == Mode::Knob && knob.board >= 0)
    {
        auto& b = boards[(size_t) knob.board];
        if (knob.panel) doc.setOption (b.node, b.ins[(size_t) knob.inlet].label, fmt (b.panel[(size_t) knob.inlet]));
        else if (knob.param >= 0) doc.setOption (b.node, b.params[(size_t) knob.param].name, fmt (b.params[(size_t) knob.param].value));
        else if (knob.inlet < 0) doc.setOption (b.node, "tune", fmt (b.tune));
        else doc.setOption (b.node, "att." + b.ins[(size_t) knob.inlet].label, fmt (b.att[(size_t) knob.inlet]));
        knob = {};
        edited (false);    // already playing; the text just catches up
    }
    else if (mode == Mode::Value && valueCord >= 0)
    {
        bool alone = doc.cordsOnLine (doc.cords[(size_t) valueCord].line) == 1;
        doc.setCordAmount (valueCord, valueNow);
        doc.cords[(size_t) valueCord].amt = valueNow;
        edited (! alone);
        valueCord = -1;
    }
    mode = Mode::None;
    repaint();
}

void Tapestry::mouseDoubleClick (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    if (auto kn = knobAt (d); kn.board >= 0)     // back to where it rests: tune 0, attenuverter 1
    {
        auto& b = boards[(size_t) kn.board];
        if (kn.panel) { auto k = (size_t) kn.inlet; b.panel[k] = b.panelDef[k]; host.setKnob (b.node, kn.inlet, b.panelDef[k]); doc.setOption (b.node, b.ins[k].label, fmt (b.panelDef[k])); }
        else if (kn.param >= 0) { auto& pr = b.params[(size_t) kn.param]; pr.value = pr.def; host.setParam (b.node, pr.k, pr.def); doc.setOption (b.node, pr.name, fmt (pr.def)); }
        else if (kn.inlet < 0) { b.tune = 0; host.setTune (b.node, 0); doc.setOption (b.node, "tune", "0"); }
        else { b.att[(size_t) kn.inlet] = 1; host.setAtt (b.node, kn.inlet, 1); doc.setOption (b.node, "att." + b.ins[(size_t) kn.inlet].label, "1"); }
        edited (false);
        repaint();
        return;
    }
    int ci = cordAt (d);
    if (ci < 0) ci = bottleAt (d);
    if (ci >= 0) { askAmount (ci); return; }
    int bi = boardAt (d);
    if (bi < 0) { fitAll(); return; }                 // double-click the empty canvas: find my patch
    if (boards[(size_t) bi].node >= 0)
        if (auto* n = doc.node (boards[(size_t) bi].node)) if (onShowLine) onShowLine (n->line);
}

void Tapestry::zoomAbout (float z, Point<float> screen)
{
    z = jlimit (0.3f, 2.5f, z);
    auto before = toDesign (screen);
    zoom = z;
    pan = before - screen / zoom;
    repaint();
}

void Tapestry::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        zoomAbout (zoom * (1.0f + w.deltaY * 0.6f), e.position);
    else
    {
        pan -= Point<float> (w.deltaX, w.deltaY) * 220.0f / zoom;
        repaint();
    }
}

void Tapestry::mouseMagnify (const MouseEvent& e, float s) { zoomAbout (zoom * s, e.position); }

juce::Rectangle<int> Tapestry::chipRect (int i) const
{
    return Rectangle<int> (getWidth() - 10 - 118 * (i + 1), 8, 112, 24);
}

juce::Rectangle<int> Tapestry::findButton() const { return getLocalBounds().removeFromBottom (40).removeFromRight (170).reduced (8); }

void Tapestry::centreOn (int bi)
{
    if (bi < 0 || bi >= (int) boards.size()) return;
    if (zoom < 0.7f) zoom = 0.9f;
    pan = boards[(size_t) bi].r.getCentre() - getLocalBounds().toFloat().getCentre() / zoom;
    selected.clear(); selected.insert (bi); selectedCord = -1;
    if (onStatus) onStatus ("here is " + boards[(size_t) bi].key);
    repaint();
}

void Tapestry::fitAll()
{
    if (boards.empty()) return;
    Rectangle<float> all = boards.front().r;
    for (auto& b : boards) all = all.getUnion (b.r);
    all = all.expanded (60);
    zoom = jlimit (0.3f, 1.0f, jmin ((float) getWidth() / all.getWidth(), (float) (getHeight() - 30) / all.getHeight()));
    pan = all.getPosition() - Point<float> (((float) getWidth() / zoom - all.getWidth()) * 0.5f, 0);
    repaint();
}

bool Tapestry::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey) { removeSelection(); return true; }
    if (k == KeyPress ('c', ModifierKeys::commandModifier, 0)) { SystemClipboard::copyTextToClipboard (copySelection()); return true; }
    if (k == KeyPress ('v', ModifierKeys::commandModifier, 0)) { paste (SystemClipboard::getTextFromClipboard()); return true; }
    if (k == KeyPress ('x', ModifierKeys::commandModifier, 0)) { SystemClipboard::copyTextToClipboard (copySelection()); removeSelection(); return true; }
    if (k == KeyPress ('0', ModifierKeys::commandModifier, 0)) { fitAll(); return true; }
    if (k == KeyPress ('=', ModifierKeys::commandModifier, 0)) { zoomAbout (zoom * 1.2f, getLocalBounds().getCentre().toFloat()); return true; }
    if (k == KeyPress ('-', ModifierKeys::commandModifier, 0)) { zoomAbout (zoom / 1.2f, getLocalBounds().getCentre().toFloat()); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ editing

String Tapestry::copySelection() const
{
    if (selectedCord >= 0 && selectedCord < (int) doc.cords.size()) return doc.cordText (doc.cords[(size_t) selectedCord]);
    std::vector<int> ids;
    for (int b : selected) if (boards[(size_t) b].node >= 0) ids.push_back (boards[(size_t) b].node);
    return doc.copyNodes (ids);
}

void Tapestry::removeSelection()
{
    if (selectedCord >= 0) { doc.removeCord (selectedCord); selectedCord = -1; edited (true); return; }
    std::set<int> ids;
    for (int b : selected) if (boards[(size_t) b].node >= 0) ids.insert (boards[(size_t) b].node);
    if (ids.empty()) return;
    doc.removeNodes (ids);
    selected.clear();
    edited (true);
}

static bool wordChar (juce_wchar c) { return CharacterFunctions::isLetterOrDigit (c) || c == '_'; }

static String renameWord (const String& line, const String& from, const String& to)
{
    String out;
    int i = 0;
    while (i < line.length())
    {
        if (line.substring (i).startsWith (from) && (i == 0 || ! wordChar (line[i - 1]))
            && (i + from.length() >= line.length() || ! wordChar (line[i + from.length()])))
        {
            out << to;
            i += from.length();
        }
        else out << line[i++];
    }
    return out;
}

void Tapestry::paste (const String& text)
{
    auto lines = StringArray::fromLines (text.trim());
    lines.removeEmptyStrings();
    if (lines.isEmpty()) return;
    // a pasted dream keeps its own names unless they're taken here
    std::set<String> taken;
    for (auto& n : doc.nodes) taken.insert (n.name);
    StringArray newNames;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto t = StringArray::fromTokens (lines[i].trim(), " ", "");
        if (t.size() >= 4 && t[0].equalsIgnoreCase ("the") && (t[2] == "is" || t[2] == "are"))
        {
            String name = t[1];
            if (taken.count (name))
            {
                String fresh;
                for (char c = 'b'; c <= 'z' && fresh.isEmpty(); ++c)
                    if (! taken.count (name + String::charToString (c))) fresh = name + String::charToString (c);
                for (int k = 0; k < lines.size(); ++k) lines.set (k, renameWord (lines[k], name, fresh));
                name = fresh;
            }
            taken.insert (name);
            newNames.add (name);
        }
    }
    // put pasted boards where the mouse is
    float x = lastMouse.x, y = lastMouse.y;
    for (auto& n : newNames) { doc.setPosition (doc.scene, n, { x, y }); x += 200; if (x > lastMouse.x + 600) { x = lastMouse.x; y += 170; } }
    doc.addLines (lines);
    if (! newNames.isEmpty()) doc.writePositions();
    edited (true);
}

void Tapestry::askAmount (int ci)
{
    auto c = doc.cords[(size_t) ci];
    if (c.kind == Door) return;
    auto* w = new AlertWindow ("amount", c.kind == Water ? "how slow, in milliseconds" : (c.kind == scq::Range ? "low..high" : "percent"), MessageBoxIconType::NoIcon);
    w->addTextEditor ("v", c.kind == scq::Range ? fmt (c.amt) + ".." + fmt (c.amt2) : fmt (c.amt));
    w->addButton ("ok", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([this, w, ci, c] (int r)
    {
        if (r == 1)
        {
            auto t = w->getTextEditorContents ("v");
            if (c.kind == scq::Range) doc.setCordAmount (ci, t.upToFirstOccurrenceOf ("..", false, false).getFloatValue(), t.fromFirstOccurrenceOf ("..", false, false).getFloatValue());
            else doc.setCordAmount (ci, t.getFloatValue());
            edited (true);
        }
    }), true);
}

void Tapestry::cordMenu (int ci)
{
    selectedCord = ci;
    repaint();
    auto c = doc.cords[(size_t) ci];
    PopupMenu m;
    m.addSectionHeader (doc.cordText (c));
    if (c.src >= 0 && c.kind != scq::Range)
    {
        m.addItem (1, "hall  ->   (a plain wire)", true, c.kind == Hall);
        m.addItem (2, "fog   ~>   (a resistor: softer)", true, c.kind == Fog);
        m.addItem (3, "water =>   (a capacitor: slower)", true, c.kind == Water);
        m.addItem (4, "door  >|   (a diode: one way)", true, c.kind == Door);
        m.addSeparator();
    }
    if (c.kind != Door) m.addItem (5, "amount...");
    m.addItem (6, "show in the journal");
    m.addItem (7, "remove");
    m.showMenuAsync ({}, [this, ci, c] (int r)
    {
        if (r >= 1 && r <= 4) { doc.setCordKind (ci, (Kind) (r - 1)); edited (true); }
        else if (r == 5) askAmount (ci);
        else if (r == 6 && onShowLine) onShowLine (c.line);
        else if (r == 7) { doc.removeCord (ci); edited (true); }
    });
}

void Tapestry::boardMenu (int bi)
{
    auto& b = boards[(size_t) bi];
    if (b.node < 0) return;
    if (! selected.count (bi)) { selected.clear(); selected.insert (bi); repaint(); }
    PopupMenu m;
    m.addSectionHeader (b.title + (b.key != b.title ? "  (" + b.key + ")" : String()));
    // switches and params: one submenu per switch, a prompt per param
    std::vector<std::pair<String, String>> choices;   // menu id - 100 -> (key, value)
    auto* nd = doc.node (b.node);
    auto* pl = nd ? findPlace (nd->place) : nullptr;
    if (pl && ! pl->opts.empty())
    {
        for (auto& o : pl->opts)
        {
            auto it = nd->opts.find (o.name);
            auto now = it != nd->opts.end() ? it->second : o.def;
            if (o.positions.isEmpty())
            {
                choices.push_back ({ o.name, {} });
                m.addItem (100 + (int) choices.size() - 1, o.name + " = " + now + "...");
                continue;
            }
            PopupMenu sub;
            for (auto& pos : o.positions)
            {
                choices.push_back ({ o.name, pos });
                sub.addItem (100 + (int) choices.size() - 1, pos, true, pos == now);
            }
            if (o.name == "scale")       // and every .scl in the scales folder
            {
                sub.addSectionHeader ("from the scales folder");
                for (auto& sc : scales::names())
                {
                    choices.push_back ({ o.name, sc });
                    sub.addItem (100 + (int) choices.size() - 1, sc, true, sc == now);
                }
                sub.addSeparator();
                choices.push_back ({ "@folder", "open" });
                sub.addItem (100 + (int) choices.size() - 1, "open the scales folder...");
            }
            m.addSubMenu (o.name + "   " + now, sub);
        }
        m.addSeparator();
    }
    m.addItem (1, "show in the journal");
    m.addItem (2, "copy as text");
    m.addItem (3, "remove");
    auto line = nd ? nd->line : 0;
    int id = b.node;
    m.showMenuAsync ({}, [this, line, id, choices] (int r)
    {
        if (r >= 100 && r - 100 < (int) choices.size())
        {
            auto [key, value] = choices[(size_t) (r - 100)];
            if (key == "@folder") { scales::folder().startAsProcess(); return; }
            if (value.isNotEmpty()) { doc.setOption (id, key, value); edited (true); return; }
            auto* w = new AlertWindow (key, "a number", MessageBoxIconType::NoIcon);
            auto* nn = doc.node (id);
            w->addTextEditor ("v", nn && nn->opts.count (key) ? nn->opts.at (key) : String());
            w->addButton ("ok", 1, KeyPress (KeyPress::returnKey));
            w->addButton ("cancel", 0, KeyPress (KeyPress::escapeKey));
            w->enterModalState (true, ModalCallbackFunction::create ([this, w, id, key] (int ok)
            {
                if (ok == 1) { doc.setOption (id, key, fmt (w->getTextEditorContents ("v").getFloatValue())); edited (true); }
            }), true);
        }
        else if (r == 1 && onShowLine) onShowLine (line);
        else if (r == 2) SystemClipboard::copyTextToClipboard (copySelection());
        else if (r == 3) removeSelection();
    });
}

void Tapestry::canvasMenu (Point<float> d)
{
    PopupMenu m, fams[4];
    const char* names[] = { "sources", "shapes", "spaces", "currents" };
    int id = 1;
    std::vector<String> ids;
    for (auto& p : catalog())
    {
        if (p.family >= Hands) continue;
        // what it does (after the dream image), and where it comes from
        auto what = p.help.fromFirstOccurrenceOf (": ", false, false).upToFirstOccurrenceOf (" (after", false, false);
        auto from = p.help.fromFirstOccurrenceOf ("(after ", false, false).upToLastOccurrenceOf (")", false, false);
        if (what.length() > 58) what = what.substring (0, 56) + "...";
        fams[p.family].addItem (id++, p.name.paddedRight (' ', 12) + what + (from.isNotEmpty() ? "   [" + from.upToFirstOccurrenceOf (";", false, false) + "]" : String()));
        ids.push_back (p.name);
    }
    for (int f = 0; f < 4; ++f) m.addSubMenu (names[f], fams[f]);
    m.addSeparator();
    m.addItem (1000, "paste", SystemClipboard::getTextFromClipboard().isNotEmpty());
    m.addItem (1001, "find my patch (fit everything in view)");
    PopupMenu find;
    for (int i = 0; i < (int) boards.size(); ++i)
        if (boards[(size_t) i].node >= 0)
            find.addItem (2000 + i, boards[(size_t) i].key + (boards[(size_t) i].key != boards[(size_t) i].title ? "   (" + boards[(size_t) i].title + ")" : String()));
    m.addSubMenu ("find...", find);
    lastMouse = d;
    m.showMenuAsync ({}, [this, ids, d] (int r)
    {
        if (r >= 2000) { centreOn (r - 2000); return; }
        if (r >= 1 && r <= (int) ids.size())
        {
            auto name = doc.addPlace (ids[(size_t) r - 1]);
            doc.setPosition (doc.scene, name, d);
            doc.writePositions();
            edited (true);
        }
        else if (r == 1000) paste (SystemClipboard::getTextFromClipboard());
        else if (r == 1001) fitAll();
    });
}

// ------------------------------------------------------------------------------------------ drops from the hands panel

bool Tapestry::isInterestedInDragSource (const SourceDetails& s) { return s.description.toString().startsWith ("hand:"); }

void Tapestry::itemDragMove (const SourceDetails& s)
{
    auto d = toDesign (s.localPosition.toFloat());
    auto hp = portAt (d, 16);
    dropPos = (hp.board >= 0 && ! hp.out) ? inletPos (boards[(size_t) hp.board], hp.port) : Point<float> (-1, -1);
    repaint();
}

void Tapestry::itemDragExit (const SourceDetails&) { dropPos = { -1, -1 }; repaint(); }

void Tapestry::itemDropped (const SourceDetails& s)
{
    dropPos = { -1, -1 };
    auto d = toDesign (s.localPosition.toFloat());
    auto hp = portAt (d, 16);
    if (hp.board >= 0 && ! hp.out)
    {
        Port from { {}, s.description.toString().fromFirstOccurrenceOf ("hand:", false, false), 1 };
        connect (from, boards[(size_t) hp.board].ins[(size_t) hp.port]);
    }
    repaint();
}
