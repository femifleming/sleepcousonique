#include "Art.h"

using namespace juce;

namespace art
{
Theme& theme() { static Theme t; return t; }   // set with setTheme() before anything paints

void setTheme (bool pop, bool dark)
{
    Theme& t = theme();
    t.pop = pop; t.dark = dark;
    auto c = [] (uint32 v) { return Colour (v); };
    if (! pop && ! dark)            // journal, light: the original
    {
        t.window = c (0xffe8e3d6); t.bar = c (0xfff2ede0); t.chromeText = c (0xff1d1b19); t.chromeDim = c (0xff9a9488); t.line = c (0x22000000);
        t.surface = c (0xffe9e3d4); t.card = c (0xfff3ead2); t.cardSp1 = c (0xffd6d4cc); t.cardHead = c (0xff101010); t.cardHeadText = c (0xfff2f2ee);
        t.button = c (0xfff4f0e4); t.buttonText = c (0xff1d1b19); t.rail = c (0xffd9d6cc);
        t.checkA = c (0xffd2a531); t.checkB = c (0xffe0bc5a);
        t.sound = c (0xff7a4a2a); t.current = c (0xff1f3f8f); t.pulse = c (0xffd2a531);
        const uint32 f[] = { 0xffd8b24a, 0xffb06a4a, 0xff4f6f7a, 0xff2c4a9a, 0xff6a6a64, 0xff222222 };
        for (int i = 0; i < 6; ++i) t.fam[i] = c (f[i]);
        t.ledOff = c (0xff3a3a38); t.faderSlot = c (0xff2a2a28); t.faderCap = c (0xfff4f2ec);
        t.note = c (0xff8a8578); t.date = c (0xff7a4a2a); t.word = c (0xff6b5f8a); t.place = c (0xff1f3f8f); t.passage = c (0xffa87a12);
        t.number = c (0xff935a2c); t.map = c (0xffcfc9bb); t.hand = c (0xff3d7a4e); t.gutter = c (0xfff2ede0); t.gutterText = c (0xffb0a890);
        paper = c (0xfffbf8f0); cream = t.card; canvas = t.surface;
    }
    else if (! pop)                 // journal, dark: the same journal by lamplight
    {
        t.window = c (0xff1c1a17); t.bar = c (0xff24211d); t.chromeText = c (0xffe8e0cc); t.chromeDim = c (0xff8a8270); t.line = c (0x33ffffff);
        t.surface = c (0xff2a2620); t.card = c (0xff3a342b); t.cardSp1 = c (0xff45423b); t.cardHead = c (0xff0c0b0a); t.cardHeadText = c (0xffe8e0cc);
        t.button = c (0xff34302a); t.buttonText = c (0xffe8e0cc); t.rail = c (0xff2a2723);
        t.checkA = c (0xffd2a531); t.checkB = c (0xffb08a2a);
        t.sound = c (0xffc8875a); t.current = c (0xff7f9be6); t.pulse = c (0xffe0b84a);
        const uint32 f[] = { 0xffd8b24a, 0xffc87a5a, 0xff6f94a2, 0xff5f7ed0, 0xff8a8a84, 0xff6a6a6a };
        for (int i = 0; i < 6; ++i) t.fam[i] = c (f[i]);
        t.ledOff = c (0xff111110); t.faderSlot = c (0xff0e0d0c); t.faderCap = c (0xffc8c2b4);
        t.note = c (0xff8a8270); t.date = c (0xffd2a531); t.word = c (0xffa99be0); t.place = c (0xff8fb0f0); t.passage = c (0xffe0b84a);
        t.number = c (0xffd89a6a); t.map = c (0xff4a453c); t.hand = c (0xff7fc08e); t.gutter = c (0xff24211d); t.gutterText = c (0xff6a6250);
        paper = c (0xff1f1d1a); cream = t.card; canvas = t.surface;
    }
    else if (! dark)                // openprocess, light: daylight
    {
        t.window = c (0xffd9dae2); t.bar = c (0xffd9dae2); t.chromeText = c (0xff1a1a1a); t.chromeDim = c (0xff7d7e80); t.line = c (0x22000000);
        t.surface = c (0xffffffff); t.card = c (0xffffffff); t.cardSp1 = c (0xfff0f0f4); t.cardHead = c (0xffb7b8ba); t.cardHeadText = c (0xff1a1a1a);
        t.button = c (0xffb7b8ba); t.buttonText = c (0xff1a1a1a); t.rail = c (0xffb7b8ba);
        t.checkA = c (0xfff2c400); t.checkB = c (0xfff7931e);
        t.sound = c (0xffd22a26); t.current = c (0xff2a7fd0); t.pulse = c (0xfff2c400);
        const uint32 f[] = { 0xfff7931e, 0xff1f8a3c, 0xff48a8c8, 0xff2a7fd0, 0xff7d7e80, 0xff1a1a1a };
        for (int i = 0; i < 6; ++i) t.fam[i] = c (f[i]);
        t.ledOff = c (0xff3a3a40); t.faderSlot = c (0xffb7b8ba); t.faderCap = c (0xff1a1a1a);
        t.note = c (0xff8a8b96); t.date = c (0xffa88a1e); t.word = c (0xff2a7fd0); t.place = c (0xffe0781e); t.passage = c (0xffd22a26);
        t.number = c (0xffb07a1e); t.map = c (0xffc8c9d1); t.hand = c (0xff1f8a3c); t.gutter = c (0xfff4f4f7); t.gutterText = c (0xff9a9cb0);
        paper = c (0xffffffff); cream = t.card; canvas = t.surface;
    }
    else                            // openprocess, dark: midnight
    {
        t.window = c (0xff16171d); t.bar = c (0xff16171d); t.chromeText = c (0xffe6e6ee); t.chromeDim = c (0xff7c7e92); t.line = c (0x22ffffff);
        t.surface = c (0xff22232c); t.card = c (0xff2b2c38); t.cardSp1 = c (0xff262733); t.cardHead = c (0xff3a3c4c); t.cardHeadText = c (0xffe6e6ee);
        t.button = c (0xff3a3c4c); t.buttonText = c (0xffe6e6ee); t.rail = c (0xff3a3c4c);
        t.checkA = c (0xfff2c400); t.checkB = c (0xfff7931e);
        t.sound = c (0xffff4a3a); t.current = c (0xff4f9ae8); t.pulse = c (0xfff2c400);
        const uint32 f[] = { 0xfff7931e, 0xff3fbf62, 0xff48b8d8, 0xff4f9ae8, 0xff7c7e92, 0xff9a9cb0 };
        for (int i = 0; i < 6; ++i) t.fam[i] = c (f[i]);
        t.ledOff = c (0xff0e0e12); t.faderSlot = c (0xff3a3c4c); t.faderCap = c (0xffe6e6ee);
        t.note = c (0xff7c7e92); t.date = c (0xfff2c400); t.word = c (0xff6fb0f0); t.place = c (0xfff7931e); t.passage = c (0xffff6a55);
        t.number = c (0xfffbc26b); t.map = c (0xff44465a); t.hand = c (0xff5fcf7a); t.gutter = c (0xff1c1d25); t.gutterText = c (0xff5a5c70);
        paper = c (0xff22232c); cream = t.card; canvas = t.surface;
    }
    ink = pop ? (dark ? c (0xffe6e6ee) : c (0xff1a1a1a)) : (dark ? c (0xffe8e0cc) : c (0xff1d1b19));
    umber = t.sound; blue = t.current; gold = pop ? t.checkA : t.pulse;
    copper = pop ? t.checkB : (dark ? c (0xffc8875a) : c (0xffb8733b));
    night = t.window;
}

Font label (float h)
{
    if (theme().pop) return Font (FontOptions ("Verdana", h * 0.92f, Font::bold)).withExtraKerningFactor (0.04f);
    return mono (h);
}

void fillChecker (Graphics& g, Rectangle<float> r, float cell)
{
    auto& t = theme();
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (t.checkB); g.fillRect (r);
    g.setColour (t.checkA);
    for (float y = r.getY(); y < r.getBottom(); y += cell)
        for (float x = r.getX() + (std::fmod ((y - r.getY()) / cell, 2.0f) < 1 ? 0 : cell); x < r.getRight(); x += cell * 2)
            g.fillRect (x, y, cell, cell);
    g.restoreState();
}

Colour familyColour (int f) { return theme().fam[jlimit (0, 5, f)]; }

Colour signalColour (int s)
{
    auto& t = theme();
    return s == 0 ? t.sound : (s == 1 ? t.current : t.pulse);
}

struct Rng
{
    uint32 s;
    float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 16777216.0f; }
    float r (float a, float b) { return a + (b - a) * u(); }
};

static Path blob (Point<float> c, float rad, Rng& rng, int pts = 9, float wob = 0.35f)
{
    Path p;
    std::vector<Point<float>> v;
    for (int i = 0; i < pts; ++i)
    {
        float a = 6.2832f * (float) i / (float) pts;
        float rr = rad * (1.0f + rng.r (-wob, wob));
        v.push_back (c + Point<float> (std::cos (a) * rr, std::sin (a) * rr));
    }
    auto mid = [] (Point<float> a, Point<float> b) { return (a + b) * 0.5f; };
    p.startNewSubPath (mid (v.back(), v[0]));
    for (size_t i = 0; i < v.size(); ++i) p.quadraticTo (v[i], mid (v[i], v[(i + 1) % v.size()]));
    p.closeSubPath();
    return p;
}

void paintCanvas (Graphics& g, Rectangle<float> a, int seed)
{
    if (theme().pop)                  // the dotted window
    {
        g.setColour (theme().surface);
        g.fillRect (a);
        g.setColour (theme().dark ? Colour (0xff34364a) : Colour (0xffc4c8d8));
        for (float y = a.getY() + 5; y < a.getBottom(); y += 10)
            for (float x = a.getX() + 5; x < a.getRight(); x += 10)
                g.fillRect (x, y, 1.2f, 1.2f);
        return;
    }
    Rng rng { (uint32) (seed * 2654435761u + 7) };
    g.setColour (canvas);
    g.fillRect (a);
    const float A = a.getWidth() * a.getHeight() / (1600.0f * 1000.0f);
    auto rp = [&] { return Point<float> (rng.r (a.getX(), a.getRight()), rng.r (a.getY(), a.getBottom())); };
    // weave
    g.setColour (Colour (0x0a000000));
    for (float y = a.getY(); y < a.getBottom(); y += 3) g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
    // washes of the journal colours
    const Colour washes[] = { Colour (0xff1f3f8f), Colour (0xffd2a531), Colour (0xff7a4a2a), Colour (0xff4f6f7a) };
    for (int i = 0; i < (int) (40 * A) + 6; ++i)
    {
        g.setColour (washes[i % 4].withAlpha (rng.r (0.03f, 0.08f)));
        g.fillPath (blob (rp(), rng.r (60, 200), rng, 11, 0.4f));
    }
    // poured skeins
    for (int i = 0; i < (int) (70 * A) + 10; ++i)
    {
        auto p = rp();
        float ang = rng.r (0, 6.283f), w = rng.r (0.4f, 2.0f);
        int segs = 6 + (int) rng.r (0, 14);
        g.setColour (Colour::greyLevel (rng.r (0.05f, 0.25f)).withAlpha (rng.r (0.35f, 0.7f)));
        for (int s = 0; s < segs; ++s)
        {
            ang += rng.r (-0.7f, 0.7f);
            float len = rng.r (8.0f, 30.0f);
            auto q = p + Point<float> (std::cos (ang) * len, std::sin (ang) * len);
            w = jlimit (0.3f, 2.6f, w + rng.r (-0.4f, 0.4f));
            g.drawLine (Line<float> (p, q), w);
            p = q;
        }
    }
    // splatter with satellites
    for (int i = 0; i < (int) (700 * A) + 60; ++i)
    {
        float rad = std::pow (rng.u(), 3.2f) * 9.0f + 0.6f;
        auto c = rp();
        bool goldDrop = rng.u() < 0.08f;
        g.setColour (goldDrop ? gold.withAlpha (0.8f) : Colour::greyLevel (rng.r (0.0f, 0.2f)).withAlpha (rng.r (0.5f, 0.85f)));
        g.fillPath (blob (c, rad, rng, 8, 0.3f));
        int sat = rad > 4 ? (int) rng.r (2, 8) : (int) rng.r (0, 3);
        for (int s = 0; s < sat; ++s)
        {
            float an = rng.r (0, 6.283f), d = rad * rng.r (1.3f, 3.2f), sr = rad * rng.r (0.08f, 0.3f);
            g.fillEllipse (Rectangle<float> (sr * 2, sr * 2).withCentre (c + Point<float> (std::cos (an) * d, std::sin (an) * d)));
        }
    }
}

CableStyle& cableStyle() { static CableStyle s; return s; }

static void bez (Point<float> a, Point<float> b, Point<float>& c1, Point<float>& c2, float heavy = 1.0f)
{
    float dist = a.getDistanceFrom (b);
    const float slack = cableStyle().slack * heavy;
    float sag = jmin (80.0f * jmax (0.25f, slack), (14.0f + 0.12f * dist) * slack);   // elasticity: how far it hangs
    c1 = a + Point<float> (jmax (30.0f, (b.x - a.x) * 0.35f), sag * 0.6f);
    c2 = b + Point<float> (-jmax (30.0f, (b.x - a.x) * 0.35f), sag * 0.6f);
}

static Point<float> bezAt (Point<float> a, Point<float> c1, Point<float> c2, Point<float> b, float t)
{
    float u = 1 - t;
    return a * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + b * (t * t * t);
}

Path cablePath (Point<float> a, Point<float> b, float heavy)
{
    Point<float> c1, c2;
    bez (a, b, c1, c2, heavy);
    Path p;
    p.startNewSubPath (a);
    p.cubicTo (c1, c2, b);
    return p;
}

Point<float> cableMid (Point<float> a, Point<float> b)
{
    Point<float> c1, c2;
    bez (a, b, c1, c2);
    return bezAt (a, c1, c2, b, 0.5f);
}

float distanceToCable (Point<float> a, Point<float> b, Point<float> p)
{
    Point<float> c1, c2;
    bez (a, b, c1, c2);
    float best = 1e9f;
    auto prev = a;
    for (int i = 1; i <= 32; ++i)
    {
        auto q = bezAt (a, c1, c2, b, (float) i / 32.0f);
        Point<float> np;
        best = jmin (best, Line<float> (prev, q).getDistanceFromPoint (p, np));
        prev = q;
    }
    return best;
}

// the part soldered into the cord: fog = resistor, water = capacitor, door = diode, lucid = an eye
static void drawPart (Graphics& g, Point<float> c, float angle, int kind)
{
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (angle).translated (c));
    if (kind == 0 || kind == 5) return;
    g.setColour (Colour (0xffb9b9b4));
    g.drawLine (-13, 0, 13, 0, 1.2f);
    if (kind == 1)
    {
        Path p; p.addRoundedRectangle (-9.0f, -3.4f, 18.0f, 6.8f, 3.0f);
        g.setColour (Colour (0xffd8c296)); g.fillPath (p);
        g.setColour (Colour (0xff7d6a45)); g.strokePath (p, PathStrokeType (0.6f));
        const Colour bands[] = { Colour (0xff6b3e1e), Colours::black, Colour (0xffc0302a), Colour (0xffc8a24a) };
        for (int i = 0; i < 4; ++i) { g.setColour (bands[i]); g.fillRect (-5.5f + (float) i * 3.2f, -3.4f, 1.6f, 6.8f); }
    }
    else if (kind == 2)
    {
        Path p; p.addRoundedRectangle (-7.5f, -4.4f, 15.0f, 8.8f, 3.5f);
        g.setColour (Colour (0xff2f5f9c)); g.fillPath (p);
        g.setColour (Colour (0xff12305a)); g.strokePath (p, PathStrokeType (0.6f));
        g.setColour (Colour (0xffd8e2ee)); g.fillRect (-7.0f, -1.0f, 14.0f, 1.0f);
    }
    else if (kind == 3)
    {
        Path p; p.addRoundedRectangle (-7.0f, -2.6f, 14.0f, 5.2f, 2.2f);
        g.setColour (Colour (0xff1c1a18)); g.fillPath (p);
        g.setColour (Colour (0xffc9c9c9)); g.fillRect (3.6f, -2.6f, 1.8f, 5.2f);
    }
    else if (kind == 4)
    {
        g.setColour (paper); g.fillEllipse (-8, -5, 16, 10);
        g.setColour (ink); g.drawEllipse (-8, -5, 16, 10, 1.0f);
        g.setColour (blue); g.fillEllipse (-3, -3, 6, 6);
    }
}

static void stroke (Graphics& g, const Path& p, float w, Colour c, const float* dash = nullptr, int nd = 0, PathStrokeType::EndCapStyle cap = PathStrokeType::rounded)
{
    g.setColour (c);
    if (! dash) { g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, cap)); return; }
    Path d;
    PathStrokeType (w, PathStrokeType::curved, cap).createDashedStroke (d, p, dash, nd);
    g.fillPath (d);
}

void drawCablePath (Graphics& g, const Path& p, Colour col, int kind, bool hover, float alpha, int signal)
{
    auto& st = cableStyle();
    auto& th = theme();
    if (signal < 0)                                     // which signal is this colour
        signal = col == th.sound ? 0 : (col == th.current ? 1 : (col == th.pulse ? 2 : 0));
    const bool audio = signal == 0, pulse = signal == 2;
    if (! hover) alpha *= st.opacity;
    col = col.withMultipliedAlpha (alpha);
    const float w = audio ? 4.6f : 2.0f;                 // audio fat, CV thin

    if (st.glow > 0.01f)                                // lights down: everything glows, audio wider
    {
        auto halo = col.brighter (0.4f);
        stroke (g, p, w * 3.2f, halo.withMultipliedAlpha (0.10f * st.glow));
        stroke (g, p, w * 1.7f, halo.withMultipliedAlpha (0.24f * st.glow));
    }
    if (hover) stroke (g, p, w + 5.0f, th.pop ? th.checkA.withAlpha (0.5f) : Colour (0x99fff2b0));

    if (th.pop)                                         // the openprocess look: flat
    {
        if (audio)                                      // a thick dotted line on a faint thread
        {
            stroke (g, p, 1.2f, col.withMultipliedAlpha (0.45f));
            const float dash[] = { 3.2f, 2.4f };
            stroke (g, p, 4.4f, col, dash, 2, PathStrokeType::butt);
        }
        else if (pulse)                                 // thin dashes
        {
            const float dash[] = { 6.0f, 3.5f };
            stroke (g, p, 1.8f, col, dash, 2, PathStrokeType::butt);
        }
        else stroke (g, p, 1.7f, col);                  // thin and solid
    }
    else                                                // the journal look: real cables
    {
        if (st.glow <= 0.01f)
            g.setColour (Colour (0x26000000).withMultipliedAlpha (alpha)),
            g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (2.5f, audio ? 5.5f : 3.5f));
        const auto lit = st.glow > 0.01f;
        if (audio)                                      // braided: a dark jacket, the colour, a twist of shade, a sheen
        {
            stroke (g, p, w + 1.4f, lit ? col : col.darker (0.75f));
            stroke (g, p, w, lit ? col.brighter (0.5f) : col);
            const float twist[] = { 3.6f, 3.6f };
            stroke (g, p, w * 0.62f, (lit ? col : col.darker (0.45f)).withMultipliedAlpha (0.75f), twist, 2, PathStrokeType::butt);
            g.setColour (Colours::white.withAlpha ((0.30f + 0.4f * st.glow) * alpha));
            g.strokePath (p, PathStrokeType (1.0f), AffineTransform::translation (-0.7f, -1.1f));
        }
        else if (pulse)                                 // thin, beaded
        {
            stroke (g, p, 1.0f, (lit ? col : col.darker (0.4f)).withMultipliedAlpha (0.8f));
            const float beads[] = { 2.2f, 3.6f };
            stroke (g, p, 3.4f, lit ? col.brighter (0.4f) : col, beads, 2, PathStrokeType::rounded);
        }
        else                                            // thin and plain
        {
            stroke (g, p, w + 0.8f, lit ? col : col.darker (0.6f));
            stroke (g, p, w, lit ? col.brighter (0.5f) : col);
        }
    }
    float len = p.getLength();
    auto q0 = p.getPointAlongPath (len * 0.47f), q1 = p.getPointAlongPath (len * 0.53f);
    drawPart (g, p.getPointAlongPath (len * 0.5f), std::atan2 (q1.y - q0.y, q1.x - q0.x), kind);
}

void drawCable (Graphics& g, Point<float> a, Point<float> b, Colour col, int kind, bool hover, float alpha, int signal)
{
    drawCablePath (g, cablePath (a, b, signal == 0 ? 1.25f : 1.0f), col, kind, hover, alpha, signal);
}

void drawLoop (Graphics& g, Point<float> c, bool hot, Colour signal)
{
    if (theme().pop)                  // a ring, filled with the signal it carries
    {
        auto r = Rectangle<float> (11, 11).withCentre (c);
        g.setColour (hot ? theme().checkA : theme().card);
        g.fillEllipse (r);
        g.setColour (signal);
        g.drawEllipse (r.reduced (1.2f), 2.6f);
        g.fillEllipse (r.reduced (3.6f));
        return;
    }
    // a loop of braided copper, the Nabra's patch point
    auto r = Rectangle<float> (11, 11).withCentre (c);
    g.setColour (Colour (0x30000000)); g.fillEllipse (r.translated (1, 2));
    g.setColour (hot ? Colour (0xffffe08a) : copper.darker (0.25f));
    g.drawEllipse (r, 3.2f);
    g.setColour (hot ? Colours::white : copper.brighter (0.35f));
    for (int i = 0; i < 6; ++i)
    {
        float a = (float) i * 1.047f;
        g.fillEllipse (Rectangle<float> (2.2f, 2.2f).withCentre (c + Point<float> (std::cos (a) * 5.5f, std::sin (a) * 5.5f)));
    }
    g.setColour (signal.withAlpha (0.9f));
    g.fillEllipse (Rectangle<float> (3.4f, 3.4f).withCentre (c));
}

Font mono (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain)); }
Font hand (float h) { return Font (FontOptions ("Georgia", h, Font::italic)); }

void drawDymo (Graphics& g, Rectangle<float> r, const String& text)
{
    // black embossed label tape, like the Nabra's
    g.setColour (Colour (0xff101010));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRect (r.withHeight (1.5f).translated (0, 1.5f));
    g.setColour (Colour (0xfff2f2ee));
    g.setFont (Font (FontOptions ("Helvetica Neue", r.getHeight() * 0.62f, Font::bold)).withExtraKerningFactor (0.12f));
    g.drawText (text.toUpperCase(), r, Justification::centred, false);
}
}
