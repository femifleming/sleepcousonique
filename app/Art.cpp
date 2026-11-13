#include "Art.h"

using namespace juce;

namespace art
{
Colour familyColour (int f)
{
    switch (f)
    {
        case 0: return Colour (0xffd8b24a);   // source: gold glitter
        case 1: return Colour (0xffb06a4a);   // shape: copper red
        case 2: return Colour (0xff4f6f7a);   // space: grey water
        case 3: return Colour (0xff2c4a9a);   // current: deep blue
        case 4: return Colour (0xff6a6a64);   // hands: the SP-1's grey
        default: return Colour (0xff222222);  // ears
    }
}

Colour signalColour (int s)
{
    return s == 0 ? umber : (s == 1 ? blue : gold);
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

static void bez (Point<float> a, Point<float> b, Point<float>& c1, Point<float>& c2)
{
    float dist = a.getDistanceFrom (b);
    float sag = jmin (80.0f, 14.0f + 0.12f * dist);
    c1 = a + Point<float> (jmax (30.0f, (b.x - a.x) * 0.35f), sag * 0.6f);
    c2 = b + Point<float> (-jmax (30.0f, (b.x - a.x) * 0.35f), sag * 0.6f);
}

static Point<float> bezAt (Point<float> a, Point<float> c1, Point<float> c2, Point<float> b, float t)
{
    float u = 1 - t;
    return a * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + b * (t * t * t);
}

Path cablePath (Point<float> a, Point<float> b)
{
    Point<float> c1, c2;
    bez (a, b, c1, c2);
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

void drawCable (Graphics& g, Point<float> a, Point<float> b, Colour col, int kind, bool hover, float alpha)
{
    Point<float> c1, c2;
    bez (a, b, c1, c2);
    Path p; p.startNewSubPath (a); p.cubicTo (c1, c2, b);
    col = col.withMultipliedAlpha (alpha);
    g.setColour (Colour (0x26000000).withMultipliedAlpha (alpha));
    g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (2.5f, 5.0f));
    if (hover)
    {
        g.setColour (Colour (0x99fff2b0));
        g.strokePath (p, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (col.darker (0.7f));
    g.strokePath (p, PathStrokeType (4.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.28f * alpha));
    g.strokePath (p, PathStrokeType (0.9f), AffineTransform::translation (-0.6f, -0.8f));
    auto q0 = bezAt (a, c1, c2, b, 0.47f), q1 = bezAt (a, c1, c2, b, 0.53f);
    drawPart (g, bezAt (a, c1, c2, b, 0.5f), std::atan2 (q1.y - q0.y, q1.x - q0.x), kind);
}

void drawLoop (Graphics& g, Point<float> c, bool hot, Colour signal)
{
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
